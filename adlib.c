/*
 * ADLIB.C - AdLib music implementation
 *
 * FM synthesis music for authentic retro sound
 *
 * By: VonHoltenCodes (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <time.h>
#include "adlib.h"

#ifdef __DJGPP__
#include <pc.h>
#define outp(port, val) outportb(port, val)
#define inp(port) inportb(port)
#endif

/* Global state */
int adlibAvailable = 0;
int musicEnabled = 0;
int currentTrack = -1;

/* Music tracks - Original compositions for MAZE RUNNER */

/* LEVEL 1 - Dark Tension (Am progression) */
static MusicNote level1Music[] = {
    {0, 57, 90, 16},  /* A3 - bass */
    {1, 60, 70, 16},  /* C4 */
    {2, 64, 70, 16},  /* E4 */
    {0, 57, 90, 16},  /* A3 */
    {1, 62, 70, 16},  /* D4 */
    {2, 65, 70, 16},  /* F4 */
    {0, 55, 90, 16},  /* G3 */
    {1, 59, 70, 16},  /* B3 */
    {2, 62, 70, 16},  /* D4 */
    {0, 57, 90, 16},  /* A3 */
    {1, 60, 70, 16},  /* C4 */
    {2, 64, 70, 16},  /* E4 */
    {0, 0, 0, 0}      /* End marker */
};

/* LEVEL 2 - Rising Action (Em progression) */
static MusicNote level2Music[] = {
    {0, 52, 90, 16},  /* E3 */
    {1, 59, 70, 16},  /* B3 */
    {2, 64, 70, 16},  /* E4 */
    {0, 50, 90, 16},  /* D3 */
    {1, 57, 70, 16},  /* A3 */
    {2, 62, 70, 16},  /* D4 */
    {0, 48, 90, 16},  /* C3 */
    {1, 55, 70, 16},  /* G3 */
    {2, 60, 70, 16},  /* C4 */
    {0, 50, 90, 16},  /* D3 */
    {1, 57, 70, 16},  /* A3 */
    {2, 62, 70, 16},  /* D4 */
    {0, 0, 0, 0}
};

/* LEVEL 3 - Intense Pursuit (Dm progression) */
static MusicNote level3Music[] = {
    {0, 50, 95, 12},  /* D3 */
    {1, 57, 75, 12},  /* A3 */
    {2, 62, 75, 12},  /* D4 */
    {0, 53, 95, 12},  /* F3 */
    {1, 60, 75, 12},  /* C4 */
    {2, 65, 75, 12},  /* F4 */
    {0, 50, 95, 12},  /* D3 */
    {1, 57, 75, 12},  /* A3 */
    {2, 62, 75, 12},  /* D4 */
    {0, 48, 95, 12},  /* C3 */
    {1, 55, 75, 12},  /* G3 */
    {2, 60, 75, 12},  /* C4 */
    {0, 0, 0, 0}
};

/* LEVEL 4 - Ominous Advance (Cm progression) */
static MusicNote level4Music[] = {
    {0, 48, 95, 14},  /* C3 */
    {1, 55, 75, 14},  /* G3 */
    {2, 60, 75, 14},  /* C4 */
    {0, 51, 95, 14},  /* Eb3 */
    {1, 58, 75, 14},  /* Bb3 */
    {2, 63, 75, 14},  /* Eb4 */
    {0, 53, 95, 14},  /* F3 */
    {1, 60, 75, 14},  /* C4 */
    {2, 65, 75, 14},  /* F4 */
    {0, 48, 95, 14},  /* C3 */
    {1, 55, 75, 14},  /* G3 */
    {2, 60, 75, 14},  /* C4 */
    {0, 0, 0, 0}
};

/* LEVEL 5 - Desperate Battle (Gm progression) */
static MusicNote level5Music[] = {
    {0, 43, 100, 12},  /* G2 */
    {1, 50, 80, 12},   /* D3 */
    {2, 55, 80, 12},   /* G3 */
    {0, 46, 100, 12},  /* Bb2 */
    {1, 53, 80, 12},   /* F3 */
    {2, 58, 80, 12},   /* Bb3 */
    {0, 45, 100, 12},  /* A2 */
    {1, 52, 80, 12},   /* E3 */
    {2, 57, 80, 12},   /* A3 */
    {0, 43, 100, 12},  /* G2 */
    {1, 50, 80, 12},   /* D3 */
    {2, 55, 80, 12},   /* G3 */
    {0, 0, 0, 0}
};

/* LEVEL 6 - Final Showdown (F#m progression - faster) */
static MusicNote level6Music[] = {
    {0, 42, 100, 10},  /* F#2 */
    {1, 49, 85, 10},   /* C#3 */
    {2, 54, 85, 10},   /* F#3 */
    {0, 45, 100, 10},  /* A2 */
    {1, 52, 85, 10},   /* E3 */
    {2, 57, 85, 10},   /* A3 */
    {0, 44, 100, 10},  /* G#2 */
    {1, 51, 85, 10},   /* D#3 */
    {2, 56, 85, 10},   /* G#3 */
    {0, 42, 100, 10},  /* F#2 */
    {1, 49, 85, 10},   /* C#3 */
    {2, 54, 85, 10},   /* F#3 */
    {0, 0, 0, 0}
};

/* MENU - Mysterious (Ambient) */
static MusicNote menuMusic[] = {
    {0, 36, 70, 32},   /* C2 - very low bass */
    {1, 48, 60, 32},   /* C3 */
    {2, 55, 60, 32},   /* G3 */
    {0, 40, 70, 32},   /* E2 */
    {1, 52, 60, 32},   /* E3 */
    {2, 59, 60, 32},   /* B3 */
    {0, 0, 0, 0}
};

/* VICTORY - Triumphant */
static MusicNote victoryMusic[] = {
    {0, 60, 100, 8},   /* C4 */
    {1, 64, 100, 8},   /* E4 */
    {2, 67, 100, 8},   /* G4 */
    {0, 64, 100, 8},   /* E4 */
    {1, 67, 100, 8},   /* G4 */
    {2, 72, 100, 8},   /* C5 */
    {0, 67, 100, 16},  /* G4 - hold */
    {1, 72, 100, 16},  /* C5 */
    {2, 76, 100, 16},  /* E5 */
    {0, 0, 0, 0}
};

static MusicTrack tracks[MUSIC_TRACK_COUNT];
static clock_t lastNoteTime = 0;
static int musicTempo = 140;  /* BPM */

/*============================================================================
 * OPL2 LOW-LEVEL ACCESS
 *===========================================================================*/

void writeOPL2(int reg, int data) {
    int i;

    /* Write register address */
    outp(OPL2_ADDRESS_PORT, reg);

    /* Wait 3.3 microseconds (6 reads from port) */
    for (i = 0; i < 6; i++) {
        inp(OPL2_ADDRESS_PORT);
    }

    /* Write data */
    outp(OPL2_DATA_PORT, data);

    /* Wait 23 microseconds (35 reads from port) */
    for (i = 0; i < 35; i++) {
        inp(OPL2_ADDRESS_PORT);
    }
}

int detectOPL2(void) {
    int status1, status2;

    /* Reset timers */
    writeOPL2(0x04, 0x60);
    writeOPL2(0x04, 0x80);

    /* Read status */
    status1 = inp(OPL2_ADDRESS_PORT);

    /* Set timer 1 */
    writeOPL2(0x02, 0xFF);
    writeOPL2(0x04, 0x21);

    /* Wait */
    delay(100);

    /* Read status again */
    status2 = inp(OPL2_ADDRESS_PORT);

    /* Reset timers */
    writeOPL2(0x04, 0x60);
    writeOPL2(0x04, 0x80);

    /* Check if OPL2 responded correctly */
    if ((status1 & 0xE0) == 0 && (status2 & 0xE0) == 0xC0) {
        return 1;  /* OPL2 detected */
    }

    return 0;
}

/*============================================================================
 * INITIALIZATION
 *===========================================================================*/

int initAdLib(void) {
    int i;

    printf("[ MUSIC  ] Detecting AdLib/OPL2...\n");

    if (detectOPL2()) {
        adlibAvailable = 1;
        musicEnabled = 1;

        /* Reset OPL2 */
        for (i = 0; i < 256; i++) {
            writeOPL2(i, 0);
        }

        /* Enable waveform select */
        writeOPL2(0x01, 0x20);

        /* Initialize all music tracks */
        tracks[MUSIC_TRACK_MENU].notes = menuMusic;
        tracks[MUSIC_TRACK_MENU].noteCount = 6;
        tracks[MUSIC_TRACK_MENU].currentNote = 0;
        tracks[MUSIC_TRACK_MENU].tempo = 100;
        tracks[MUSIC_TRACK_MENU].loop = 1;

        tracks[MUSIC_TRACK_LEVEL1].notes = level1Music;
        tracks[MUSIC_TRACK_LEVEL1].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL1].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL1].tempo = 120;
        tracks[MUSIC_TRACK_LEVEL1].loop = 1;

        tracks[MUSIC_TRACK_LEVEL2].notes = level2Music;
        tracks[MUSIC_TRACK_LEVEL2].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL2].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL2].tempo = 130;
        tracks[MUSIC_TRACK_LEVEL2].loop = 1;

        tracks[MUSIC_TRACK_LEVEL3].notes = level3Music;
        tracks[MUSIC_TRACK_LEVEL3].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL3].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL3].tempo = 140;
        tracks[MUSIC_TRACK_LEVEL3].loop = 1;

        tracks[MUSIC_TRACK_LEVEL4].notes = level4Music;
        tracks[MUSIC_TRACK_LEVEL4].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL4].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL4].tempo = 145;
        tracks[MUSIC_TRACK_LEVEL4].loop = 1;

        tracks[MUSIC_TRACK_LEVEL5].notes = level5Music;
        tracks[MUSIC_TRACK_LEVEL5].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL5].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL5].tempo = 150;
        tracks[MUSIC_TRACK_LEVEL5].loop = 1;

        tracks[MUSIC_TRACK_LEVEL6].notes = level6Music;
        tracks[MUSIC_TRACK_LEVEL6].noteCount = 12;
        tracks[MUSIC_TRACK_LEVEL6].currentNote = 0;
        tracks[MUSIC_TRACK_LEVEL6].tempo = 160;
        tracks[MUSIC_TRACK_LEVEL6].loop = 1;

        tracks[MUSIC_TRACK_VICTORY].notes = victoryMusic;
        tracks[MUSIC_TRACK_VICTORY].noteCount = 9;
        tracks[MUSIC_TRACK_VICTORY].currentNote = 0;
        tracks[MUSIC_TRACK_VICTORY].tempo = 150;
        tracks[MUSIC_TRACK_VICTORY].loop = 0;

        printf("[ MUSIC  ] AdLib detected and initialized\n");
        printf("[ MUSIC  ] 8 music tracks loaded\n");
        return 1;
    }

    printf("[ MUSIC  ] AdLib not found - music disabled\n");
    return 0;
}

void shutdownAdLib(void) {
    int i;

    if (!adlibAvailable) return;

    /* Silence all channels */
    for (i = 0; i < 9; i++) {
        writeOPL2(0xB0 + i, 0);  /* Key off */
    }

    /* Reset all registers */
    for (i = 0; i < 256; i++) {
        writeOPL2(i, 0);
    }

    adlibAvailable = 0;
    musicEnabled = 0;
}

/*============================================================================
 * MUSIC PLAYBACK
 *===========================================================================*/

void playMusic(int trackNumber) {
    int i;

    if (!adlibAvailable || !musicEnabled) return;
    if (trackNumber < 0 || trackNumber >= MUSIC_TRACK_COUNT) return;

    /* Stop any currently playing music */
    stopMusic();

    /* Set up FM patches for all channels */
    for (i = 0; i < 3; i++) {
        setPatch(i, 0);  /* Simple sine wave patch */
    }

    currentTrack = trackNumber;
    tracks[trackNumber].currentNote = 0;
    lastNoteTime = clock();  /* Initialize timing */

    printf("[ MUSIC  ] Now playing track %d\n", trackNumber);
}

void stopMusic(void) {
    int i;

    if (!adlibAvailable) return;

    /* Silence all channels */
    for (i = 0; i < 9; i++) {
        writeOPL2(0xB0 + i, 0);
    }

    currentTrack = -1;
}

/* Convert MIDI note to OPL2 frequency */
static unsigned int noteToFreq(int note) {
    /* A4 = 440 Hz = MIDI note 69 */
    /* Formula: freq = 440 * 2^((note - 69) / 12) */
    static const unsigned int freqTable[128] = {
        8,    9,    9,    10,   10,   11,   12,   12,   /* 0-7 */
        13,   14,   15,   15,   16,   17,   18,   19,   /* 8-15 */
        21,   22,   23,   24,   26,   27,   29,   31,   /* 16-23 */
        33,   35,   37,   39,   41,   44,   46,   49,   /* 24-31 */
        52,   55,   58,   62,   65,   69,   73,   78,   /* 32-39 */
        82,   87,   92,   98,   104,  110,  117,  123,  /* 40-47 */
        131,  139,  147,  156,  165,  175,  185,  196,  /* 48-55 */
        208,  220,  233,  247,  262,  277,  294,  311,  /* 56-63 */
        330,  349,  370,  392,  415,  440,  466,  494,  /* 64-71 (A4=440) */
        523,  554,  587,  622,  659,  698,  740,  784,  /* 72-79 */
        831,  880,  932,  988,  1047, 1109, 1175, 1245, /* 80-87 */
        1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, /* 88-95 */
        2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, /* 96-103 */
        3322, 3520, 3729, 3951, 4186, 4435, 4699, 4978, /* 104-111 */
        5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902, /* 112-119 */
        8372, 8870, 9397, 9956, 10548,11175,11840,12544 /* 120-127 */
    };
    if (note < 0 || note >= 128) return 440;
    return freqTable[note];
}

/* Play a note on a specific channel */
static void playNote(int channel, int note, int velocity) {
    unsigned int freq;
    unsigned int fnum;
    unsigned int block;
    int volume;

    if (channel < 0 || channel >= 9) return;

    freq = noteToFreq(note);

    /* Convert frequency to F-Number and Block */
    /* Block 0-7, F-Number 0-1023 */
    block = 4;  /* Middle octave */
    while (freq > 1023 && block < 7) {
        freq >>= 1;
        block++;
    }
    while (freq < 512 && block > 0) {
        freq <<= 1;
        block--;
    }
    fnum = freq;

    /* Convert velocity (0-127) to OPL2 volume (63-0, inverted) */
    /* REDUCED VOLUME - make music 50% quieter to balance with sound effects */
    volume = 63 - ((velocity * 32) / 127);  /* Max volume now 32 instead of 63 */

    /* Set carrier volume */
    writeOPL2(0x40 + channel + 3, volume);

    /* Set frequency low 8 bits */
    writeOPL2(0xA0 + channel, fnum & 0xFF);

    /* Set key on, block, and frequency high 2 bits */
    writeOPL2(0xB0 + channel, 0x20 | (block << 2) | ((fnum >> 8) & 0x03));
}

/* Stop a note on a specific channel */
static void stopNote(int channel) {
    if (channel < 0 || channel >= 9) return;
    /* Key off - keep block and fnum, but clear key-on bit */
    writeOPL2(0xB0 + channel, 0);
}

void updateMusic(void) {
    MusicTrack *track;
    MusicNote *note;
    clock_t now;
    long elapsedMs;
    long noteDurationMs;
    static int firstNote = 1;

    if (!adlibAvailable || !musicEnabled || currentTrack < 0) return;
    if (currentTrack >= MUSIC_TRACK_COUNT) return;

    track = &tracks[currentTrack];
    if (track->noteCount == 0 || track->notes == NULL) return;

    now = clock();

    /* Check if it's time to play next note */
    if (lastNoteTime == 0) {
        lastNoteTime = now;
        firstNote = 1;
    }

    elapsedMs = ((now - lastNoteTime) * 1000) / CLOCKS_PER_SEC;

    /* Get current note */
    note = &track->notes[track->currentNote];

    /* Calculate note duration in milliseconds */
    /* duration is in ticks, tempo is BPM */
    /* ms per tick = (60000 / BPM) / ticksPerBeat */
    /* Assuming 16 ticks per beat */
    noteDurationMs = (note->duration * 60000) / (track->tempo * 16);

    /* Play first note immediately, then wait for durations */
    if (firstNote || elapsedMs >= noteDurationMs) {
        firstNote = 0;
        /* Stop previous note */
        if (track->currentNote > 0) {
            MusicNote *prevNote = &track->notes[track->currentNote - 1];
            stopNote(prevNote->channel);
        }

        /* Move to next note */
        track->currentNote++;

        /* Check for end of track */
        if (track->currentNote >= track->noteCount ||
            track->notes[track->currentNote].duration == 0) {
            /* Loop or stop */
            if (track->loop) {
                track->currentNote = 0;
            } else {
                stopMusic();
                return;
            }
        }

        /* Play new note */
        note = &track->notes[track->currentNote];
        playNote(note->channel, note->note, note->velocity);

        lastNoteTime = now;
    }
}

void setMusicVolume(int volume) {
    /* TODO: Implement volume control */
    /* Adjust OPL2 operator levels */
}

/*============================================================================
 * INSTRUMENT PATCHES
 *===========================================================================*/

void setPatch(int channel, int patch) {
    /* IMPROVED FM SYNTHESIS PATCH - Better tone quality */
    /* Uses proper ADSR envelopes and waveform selection for musical sound */

    int op1 = channel;        /* Modulator operator */
    int op2 = channel + 3;    /* Carrier operator */

    /* Waveform selection - use sine waves for smooth tone */
    writeOPL2(0x20 + op1, 0x21);  /* Modulator: tremolo off, vibrato on, sustain on */
    writeOPL2(0x20 + op2, 0x21);  /* Carrier: tremolo off, vibrato on, sustain on */

    /* Attack/Decay rates - SMOOTH attack for less "computer" sound */
    writeOPL2(0x60 + op1, 0xA4);  /* Modulator: Medium attack (A), medium decay (4) */
    writeOPL2(0x60 + op2, 0xA4);  /* Carrier: Medium attack (A), medium decay (4) */

    /* Sustain/Release rates - LONGER release for smoother sound */
    writeOPL2(0x80 + op1, 0x66);  /* Modulator: Sustain 6, Release 6 */
    writeOPL2(0x80 + op2, 0x66);  /* Carrier: Sustain 6, Release 6 */

    /* Output levels - modulator quieter for subtle FM */
    writeOPL2(0x40 + op1, 0x10);  /* Modulator: low volume for FM modulation */
    writeOPL2(0x40 + op2, 0x00);  /* Carrier: will be set by playNote() */

    /* Feedback and connection - add warmth */
    writeOPL2(0xC0 + channel, 0x01);  /* Feedback 0, FM synthesis mode */
}
