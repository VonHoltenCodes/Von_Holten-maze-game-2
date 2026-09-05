/*
 * keyboard.c - key-state keyboard driver (INT 9 handler) for DJGPP
 * See keyboard.h for the why.
 */
#include <string.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include "keyboard.h"

static volatile unsigned char keyState[128];
static volatile unsigned char keyEdge[128];
static _go32_dpmi_seginfo oldHandler, newHandler;
static int installed = 0;

/* Everything the handler touches must be locked in memory (no paging under DPMI). */
static void kbHandler(void)
{
    unsigned char sc = inportb(0x60);
    if (sc != 0xE0 && sc != 0xE1) {        /* extended-key prefixes carry no state */
        if (sc & 0x80) {
            keyState[sc & 0x7F] = 0;
        } else {
            if (!keyState[sc]) keyEdge[sc] = 1;
            keyState[sc] = 1;
        }
    }
    outportb(0x20, 0x20);                  /* EOI to the master PIC */
}
static void kbHandlerEnd(void) {}

int kb_install(void)
{
    if (installed) return 1;
    memset((void *)keyState, 0, sizeof keyState);
    memset((void *)keyEdge, 0, sizeof keyEdge);
    _go32_dpmi_lock_data((void *)keyState, sizeof keyState);
    _go32_dpmi_lock_data((void *)keyEdge, sizeof keyEdge);
    _go32_dpmi_lock_code((void *)kbHandler, (unsigned long)kbHandlerEnd - (unsigned long)kbHandler);

    _go32_dpmi_get_protected_mode_interrupt_vector(9, &oldHandler);
    newHandler.pm_offset = (unsigned long)kbHandler;
    newHandler.pm_selector = _go32_my_cs();
    if (_go32_dpmi_allocate_iret_wrapper(&newHandler) != 0) return 0;
    if (_go32_dpmi_set_protected_mode_interrupt_vector(9, &newHandler) != 0) {
        _go32_dpmi_free_iret_wrapper(&newHandler);
        return 0;
    }
    installed = 1;
    return 1;
}

void kb_remove(void)
{
    if (!installed) return;
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &oldHandler);
    _go32_dpmi_free_iret_wrapper(&newHandler);
    installed = 0;
    while (kbhit()) getkey();              /* drop anything the BIOS buffered before we hooked */
}

int kb_down(int sc)
{
    return keyState[sc & 0x7F];
}

int kb_pressed(int sc)
{
    sc &= 0x7F;
    if (keyEdge[sc]) { keyEdge[sc] = 0; return 1; }
    return 0;
}

void kb_clear(void)
{
    memset((void *)keyState, 0, sizeof keyState);
    memset((void *)keyEdge, 0, sizeof keyEdge);
}
