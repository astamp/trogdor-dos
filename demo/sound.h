#ifndef SOUND_H
#define SOUND_H

#define SOUND_EOF (0xFFFFu)

typedef struct _sample {
    unsigned int freq;
    unsigned int duration;
} sample_t;

/* Plays a foreground sound effect. */
void SOUND_play_fg(sample_t* buffer);
/* Stops a foreground sound effect. */
void SOUND_stop_fg(void);

/* Plays a background sound effect. */
void SOUND_play_bg(sample_t* buffer, int loop);
/* Stops a background sound effect. */
void SOUND_stop_bg(void);

/* Initialize the sound queues. */
void SOUND_init();

/* Disables all sounds. */
void SOUND_fini();

/* Poll the sound machine. */
void SOUND_poll();

#endif // SOUND_H
