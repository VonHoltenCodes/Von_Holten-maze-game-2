/*
 * SOUND.C - Sound Blaster audio for MAZE
 * Adapted from BONK DOS Edition
 */

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __DJGPP__
#include <pc.h>
#define outp(port, val) outportb(port, val)
#define inp(port) inportb(port)
#endif

/* Sound Blaster ports */
#define SB_BASE       0x220
#define SB_RESET      (SB_BASE + 0x06)
#define SB_READ       (SB_BASE + 0x0A)
#define SB_WRITE      (SB_BASE + 0x0C)
#define SB_READ_STATUS (SB_BASE + 0x0E)
#define SB_DMA_CHANNEL 1
#define SB_IRQ        5

/* Sound Blaster commands */
#define DSP_SPEAKER_ON  0xD1
#define DSP_SPEAKER_OFF 0xD3
#define DSP_SET_TIME_CONSTANT 0x40
#define DSP_SINGLE_DMA  0x14

/* Audio system state */
static int sbAvailable = 0;
static unsigned char *audioBuffer = NULL;
static int audioBufferSize = 0;

/*============================================================================
 * SOUND BLASTER HARDWARE ACCESS
 *===========================================================================*/

int sbReset(void) {
    int i;

    /* Send reset */
    outp(SB_RESET, 1);
    for (i = 0; i < 100; i++);
    outp(SB_RESET, 0);

    /* Wait for 0xAA */
    for (i = 0; i < 1000; i++) {
        if (inp(SB_READ_STATUS) & 0x80) {
            if (inp(SB_READ) == 0xAA) {
                return 1;
            }
        }
    }
    return 0;
}

void sbWriteDSP(unsigned char value) {
    while (inp(SB_WRITE) & 0x80);
    outp(SB_WRITE, value);
}

void sbSpeakerOn(void) {
    sbWriteDSP(DSP_SPEAKER_ON);
}

void sbSpeakerOff(void) {
    sbWriteDSP(DSP_SPEAKER_OFF);
}

/*============================================================================
 * DMA CONTROLLER
 *===========================================================================*/

void dmaSetup(unsigned char channel, unsigned char *buffer, unsigned int length) {
    unsigned long addr = (unsigned long)buffer;
    unsigned int page = (addr >> 16) & 0xFF;
    unsigned int offset = addr & 0xFFFF;

    /* Disable DMA channel */
    outp(0x0A, 0x04 | channel);

    /* Clear flip-flop */
    outp(0x0C, 0x00);

    /* Set mode (single transfer, read) */
    outp(0x0B, 0x48 | channel);

    /* Set address */
    outp(0x02, offset & 0xFF);
    outp(0x02, (offset >> 8) & 0xFF);

    /* Set page */
    outp(0x83, page);

    /* Set count */
    length--;
    outp(0x03, length & 0xFF);
    outp(0x03, (length >> 8) & 0xFF);

    /* Enable DMA channel */
    outp(0x0A, channel);
}

/*============================================================================
 * PCM SAMPLE GENERATION
 *===========================================================================*/

void generateToneSample(unsigned char *buffer, int length, int frequency, int sampleRate) {
    int i;

    /* Handle silence (frequency = 0) */
    if (frequency == 0) {
        for (i = 0; i < length; i++) {
            buffer[i] = 128;  /* Silence = middle value */
        }
        return;
    }

    int samplesPerCycle = sampleRate / frequency;
    int halfCycle = samplesPerCycle / 2;

    /* Prevent division by zero if samplesPerCycle is 0 */
    if (samplesPerCycle == 0) {
        samplesPerCycle = 1;
    }

    /* Generate square wave - classic arcade beep sound */
    for (i = 0; i < length; i++) {
        /* Square wave: alternates between high (200) and low (56) */
        if ((i % samplesPerCycle) < halfCycle) {
            buffer[i] = 200;  /* High */
        } else {
            buffer[i] = 56;   /* Low */
        }
    }
}

/*============================================================================
 * PUBLIC API
 *===========================================================================*/

int initAudio(void) {
    printf("[ AUDIO  ] Detecting Sound Blaster...\n");

    if (sbReset()) {
        sbAvailable = 1;
        printf("[ AUDIO  ] Sound Blaster detected at 0x%X\n", SB_BASE);

        /* Allocate audio buffer (11025 Hz, 1 second) */
        audioBufferSize = 11025;
        audioBuffer = (unsigned char *)malloc(audioBufferSize);

        if (!audioBuffer) {
            printf("[ AUDIO  ] Failed to allocate audio buffer\n");
            sbAvailable = 0;
            return 0;
        }

        sbSpeakerOn();
        return 1;
    }

    printf("[ AUDIO  ] Sound Blaster not found - no audio\n");
    return 0;
}

void playToneBlocking(int frequency, int durationMs) {
    if (!sbAvailable || !audioBuffer) {
        /* Fallback to PC speaker */
        if (frequency > 0) {
            unsigned int divisor = 1193180 / frequency;
            outp(0x43, 0xB6);
            outp(0x42, divisor & 0xFF);
            outp(0x42, divisor >> 8);
            unsigned char tmp = inp(0x61);
            outp(0x61, tmp | 3);
            delay(durationMs);
            outp(0x61, tmp);
        } else {
            delay(durationMs);
        }
        return;
    }

    /* Generate PCM sample */
    int sampleRate = 11025;
    int samples = (sampleRate * durationMs) / 1000;
    if (samples > audioBufferSize) samples = audioBufferSize;

    generateToneSample(audioBuffer, samples, frequency, sampleRate);

    /* Set sample rate */
    unsigned int timeConstant = 256 - (1000000 / sampleRate);
    sbWriteDSP(DSP_SET_TIME_CONSTANT);
    sbWriteDSP(timeConstant);

    /* Setup DMA */
    dmaSetup(SB_DMA_CHANNEL, audioBuffer, samples);

    /* Start playback */
    sbWriteDSP(DSP_SINGLE_DMA);
    sbWriteDSP((samples - 1) & 0xFF);
    sbWriteDSP(((samples - 1) >> 8) & 0xFF);

    /* Wait for completion */
    delay(durationMs);
}

void shutdownAudio(void) {
    if (sbAvailable) {
        sbSpeakerOff();
    }
    if (audioBuffer) {
        free(audioBuffer);
    }
}

int isAudioAvailable(void) {
    return sbAvailable;
}
