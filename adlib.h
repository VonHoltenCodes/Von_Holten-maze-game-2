/*
 * ADLIB.H - AdLib/OPL2 FM synthesizer music
 *
 * Provides cyberpunk soundtrack using FM synthesis
 * Compatible with AdLib, SoundBlaster, and OPL2/OPL3 cards
 *
 * By: VonHoltenCodes (2025)
 */

#ifndef ADLIB_H
#define ADLIB_H

/* OPL2 ports */
#define OPL2_ADDRESS_PORT  0x388
#define OPL2_DATA_PORT     0x389

/* OPL2 registers */
#define OPL2_TEST_LSI           0x01
#define OPL2_TIMER1             0x02
#define OPL2_TIMER2             0x03
#define OPL2_TIMER_CTRL         0x04
#define OPL2_WAVEFORM_ENABLE    0x01

/* Music tracks */
#define MUSIC_TRACK_MENU     0
#define MUSIC_TRACK_LEVEL1   1
#define MUSIC_TRACK_LEVEL2   2
#define MUSIC_TRACK_LEVEL3   3
#define MUSIC_TRACK_LEVEL4   4
#define MUSIC_TRACK_LEVEL5   5
#define MUSIC_TRACK_LEVEL6   6
#define MUSIC_TRACK_VICTORY  7
#define MUSIC_TRACK_COUNT    8

/* Note structure for sequencer */
typedef struct {
    int channel;      /* OPL2 channel (0-8) */
    int note;         /* MIDI note number */
    int velocity;     /* Volume (0-127) */
    int duration;     /* Ticks */
} MusicNote;

/* Track structure */
typedef struct {
    MusicNote *notes;
    int noteCount;
    int currentNote;
    int tempo;        /* BPM */
    int loop;         /* Loop track? */
} MusicTrack;

/* Global state */
extern int adlibAvailable;
extern int musicEnabled;
extern int currentTrack;

/* Function prototypes */
int initAdLib(void);
void shutdownAdLib(void);
void writeOPL2(int reg, int data);
int detectOPL2(void);

/* Music control */
void playMusic(int trackNumber);
void stopMusic(void);
void updateMusic(void);
void setMusicVolume(int volume);

/* Instrument programming */
void setPatch(int channel, int patch);

#endif /* ADLIB_H */
