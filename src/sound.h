/*
 * sound.h - non-blocking sound effects: Sound Blaster 8-bit DMA, PC speaker fallback
 *
 * A sound is a short list of (frequency Hz, duration ms) steps. On a Sound
 * Blaster the whole list is rendered to a DOS-memory buffer and played by one
 * single-cycle DMA transfer, so the game loop never waits. Without a card the
 * PC speaker plays the same list, stepped forward by sound_update() each frame.
 */
#ifndef SOUND_H
#define SOUND_H

typedef struct {
    short freq;       /* Hz, 0 = rest */
    short ms;         /* duration */
} SoundStep;

typedef struct {
    const SoundStep *steps;
    int count;
    int priority;     /* a new sound only interrupts one of lower or equal priority */
} SoundFx;

int  sound_init(int quiet);       /* returns 1 if a Sound Blaster answered, 0 = PC speaker */
void sound_shutdown(void);
void sound_update(void);          /* once per frame: advances the PC-speaker sequencer */
void sound_play(const SoundFx *fx);
int  sound_blaster_present(void);
int  sound_blaster_base(void);

#endif
