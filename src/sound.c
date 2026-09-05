/*
 * sound.c - non-blocking sound effects for DJGPP
 *
 * Sound Blaster path: one single-cycle 8-bit DMA transfer per effect from a
 * buffer in conventional (DOS) memory. The old code handed the DMA controller
 * a protected-mode malloc() pointer, which is not a physical address, so the
 * card played whatever happened to live at that number below 1 MB. The buffer
 * now comes from __dpmi_allocate_dos_memory and is positioned so it never
 * crosses a 64 KB DMA page.
 *
 * PC speaker path: same (freq, ms) steps, stepped by sound_update() using the
 * BIOS tick counter, so nothing blocks the frame loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#include <sys/movedata.h>
#include "sound.h"

#define SAMPLE_RATE   11025
#define BUF_SIZE      16384                 /* ~1.5 s at 11025 Hz */
#define BIOS_TICKS_HZ 18.2065

static int sbBase = 0x220, sbDma = 1;
static int sbFound = 0;

static int dosSelector = -1;
static unsigned long dosPhys = 0;           /* physical address of the DMA buffer */
static unsigned char *render = NULL;        /* protected-mode staging copy */

static int  curPriority = -1;
static unsigned long curEndTick = 0;

/* PC speaker sequencer */
static const SoundFx *spkFx = NULL;
static int spkStep = 0;
static unsigned long spkStepEnd = 0;

static unsigned long biosTicks(void)
{
    return _farpeekl(_dos_ds, 0x46C);
}

static unsigned long msToTicks(int ms)
{
    unsigned long t = (unsigned long)(ms * BIOS_TICKS_HZ / 1000.0 + 0.999);
    return t ? t : 1;
}

/* ---- Sound Blaster DSP ------------------------------------------------- */

static void parseBlasterEnv(void)
{
    const char *e = getenv("BLASTER");
    if (!e) return;
    while (*e) {
        if (*e == 'A' || *e == 'a') sbBase = (int)strtol(e + 1, NULL, 16);
        else if (*e == 'D' || *e == 'd') sbDma = (int)strtol(e + 1, NULL, 10);
        while (*e && *e != ' ') e++;
        while (*e == ' ') e++;
    }
    if (sbDma < 0 || sbDma > 3) sbDma = 1;  /* 8-bit transfers only */
}

static void dspWrite(int v)
{
    int i;
    for (i = 0; i < 20000; i++) if (!(inportb(sbBase + 0xC) & 0x80)) break;
    outportb(sbBase + 0xC, v);
}

static int dspReset(void)
{
    int i;
    outportb(sbBase + 0x6, 1);
    for (i = 0; i < 40; i++) inportb(sbBase + 0x6);   /* > 3 us of ISA bus time */
    outportb(sbBase + 0x6, 0);
    for (i = 0; i < 4000; i++) {
        if ((inportb(sbBase + 0xE) & 0x80) && inportb(sbBase + 0xA) == 0xAA) return 1;
    }
    return 0;
}

static int allocDmaBuffer(void)
{
    static const int pagePort[4] = { 0x87, 0x83, 0x81, 0x82 };
    unsigned long phys;
    int seg;
    (void)pagePort;
    seg = __dpmi_allocate_dos_memory((2 * BUF_SIZE + 15) / 16, &dosSelector);
    if (seg < 0) { dosSelector = -1; return 0; }
    phys = (unsigned long)seg << 4;
    if ((phys & 0xFFFF) + BUF_SIZE > 0x10000)           /* would cross a 64 KB page: use the next page */
        phys = (phys & ~0xFFFFUL) + 0x10000;
    dosPhys = phys;
    return 1;
}

static void dmaStart(unsigned long phys, int len)
{
    static const int pagePort[4] = { 0x87, 0x83, 0x81, 0x82 };
    int addrPort = sbDma * 2, countPort = sbDma * 2 + 1;
    unsigned int off = phys & 0xFFFF, cnt = len - 1;
    outportb(0x0A, 0x04 | sbDma);           /* mask channel */
    outportb(0x0C, 0);                      /* clear flip-flop */
    outportb(0x0B, 0x48 | sbDma);           /* single mode, read (memory -> card), increment */
    outportb(addrPort, off & 0xFF);
    outportb(addrPort, off >> 8);
    outportb(pagePort[sbDma], (phys >> 16) & 0xFF);
    outportb(countPort, cnt & 0xFF);
    outportb(countPort, cnt >> 8);
    outportb(0x0A, sbDma);                  /* unmask */
}

/* Render a (freq, ms) list into 8-bit unsigned PCM. Square wave with a short
 * decay on every step so shots and hits have some attack. */
static int renderFx(const SoundFx *fx, unsigned char *out, int maxLen)
{
    int n = 0, s;
    for (s = 0; s < fx->count && n < maxLen; s++) {
        int len = SAMPLE_RATE * fx->steps[s].ms / 1000;
        int freq = fx->steps[s].freq, i;
        int period = freq > 0 ? SAMPLE_RATE / freq : 0;
        if (period < 2 && freq > 0) period = 2;
        if (n + len > maxLen) len = maxLen - n;
        for (i = 0; i < len; i++) {
            int amp = 64 - (i * 40) / (len ? len : 1);       /* 64 -> 24 */
            unsigned char v = 128;
            if (period) v = ((i % period) < period / 2) ? (unsigned char)(128 + amp) : (unsigned char)(128 - amp);
            out[n++] = v;
        }
    }
    return n;
}

static void sbPlay(const SoundFx *fx)
{
    int len = renderFx(fx, render, BUF_SIZE);
    if (len < 2) return;
    dosmemput(render, len, dosPhys);
    dspReset();                             /* stops anything in flight, known state */
    inportb(sbBase + 0xE);                  /* ack a pending 8-bit IRQ */
    dspWrite(0xD1);                         /* speaker on */
    dspWrite(0x40);                         /* time constant */
    dspWrite(256 - 1000000 / SAMPLE_RATE);
    dmaStart(dosPhys, len);
    dspWrite(0x14);                         /* 8-bit single-cycle output */
    dspWrite((len - 1) & 0xFF);
    dspWrite(((len - 1) >> 8) & 0xFF);
}

/* ---- PC speaker --------------------------------------------------------- */

static void spkTone(int freq)
{
    if (freq > 0) {
        unsigned int div = 1193180 / freq;
        outportb(0x43, 0xB6);
        outportb(0x42, div & 0xFF);
        outportb(0x42, div >> 8);
        outportb(0x61, inportb(0x61) | 3);
    } else {
        outportb(0x61, inportb(0x61) & ~3);
    }
}

static void spkBegin(const SoundFx *fx)
{
    spkFx = fx;
    spkStep = 0;
    spkTone(fx->steps[0].freq);
    spkStepEnd = biosTicks() + msToTicks(fx->steps[0].ms);
}

/* ---- public ------------------------------------------------------------- */

int sound_init(int verbose)
{
    parseBlasterEnv();
    render = (unsigned char *)malloc(BUF_SIZE);
    if (render && dspReset() && allocDmaBuffer()) {
        sbFound = 1;
        if (verbose) printf("[ AUDIO  ] Sound Blaster at %Xh, DMA %d\n", sbBase, sbDma);
        return 1;
    }
    if (verbose) printf("[ AUDIO  ] No Sound Blaster at %Xh - using PC speaker\n", sbBase);
    return 0;
}

void sound_shutdown(void)
{
    spkTone(0);
    if (sbFound) { dspReset(); dspWrite(0xD3); }
    if (dosSelector >= 0) { __dpmi_free_dos_memory(dosSelector); dosSelector = -1; }
    if (render) { free(render); render = NULL; }
}

void sound_update(void)
{
    unsigned long now;
    if (sbFound || !spkFx) return;
    now = biosTicks();
    if ((long)(now - spkStepEnd) < 0) return;
    spkStep++;
    if (spkStep >= spkFx->count) { spkTone(0); spkFx = NULL; return; }
    spkTone(spkFx->steps[spkStep].freq);
    spkStepEnd = now + msToTicks(spkFx->steps[spkStep].ms);
}

void sound_play(const SoundFx *fx)
{
    unsigned long now = biosTicks();
    int totalMs = 0, i;
    if (!fx || fx->count <= 0) return;
    if ((long)(curEndTick - now) > 0 && fx->priority < curPriority) return;   /* something louder is playing */
    for (i = 0; i < fx->count; i++) totalMs += fx->steps[i].ms;
    curPriority = fx->priority;
    curEndTick = now + msToTicks(totalMs);
    if (sbFound) sbPlay(fx);
    else spkBegin(fx);
}

int sound_blaster_present(void) { return sbFound; }
int sound_blaster_base(void)    { return sbBase; }
