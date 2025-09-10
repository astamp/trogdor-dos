#include "sound.h"
#include "debug.h"
#include <stdlib.h>
#include <i86.h>

typedef struct _soundq {
    sample_t* sample;
    size_t ticks;
    int idx;
    int loop;
} soundq_t;

static soundq_t fg;
static soundq_t bg;

static void set_sound(unsigned int freq) {
    if (freq > 0) {
        sound(freq);
    } else {
        nosound();
    }
}

static void process_sound(soundq_t* q, int silent)
{
    // Fast-path the do nothing case.
    if (q->ticks > 0) {
        q->ticks--;
        return;
    }
    
    q->idx++;
    if (SOUND_EOF == q->sample[q->idx].freq) {
        if (q->loop) {
            q->idx = 0;
        } else {
            q->sample = NULL;
            return;
        }
    }

    q->ticks = q->sample[q->idx].duration;
    
    if (silent) {
        return;
    }

    set_sound(q->sample[q->idx].freq);
}

void SOUND_init()
{
    nosound();
    fg.sample = NULL;
    fg.ticks = 0;
    fg.idx = 0;
    fg.loop = 0;
    bg.sample = NULL;
    bg.ticks = 0;
    bg.idx = 0;
    bg.loop = 0;
}

void SOUND_fini()
{
    nosound();
}

void SOUND_poll()
{
    if (fg.sample) {
        process_sound(&fg, 0);
        process_sound(&bg, 1);
        // HACK: Resume background on foreground termination.
        if ((NULL == fg.sample) && (NULL != bg.sample)) {
            set_sound(bg.sample[bg.idx].freq);
        }
    } else if (bg.sample) {
        process_sound(&bg, 0);
    }
}

void SOUND_play_fg(sample_t* buffer)
{
    fg.sample = buffer;
    fg.ticks = 0;
    fg.idx = -1;
}

void SOUND_stop_fg(void)
{
    fg.sample = NULL;
    fg.ticks = 0;
}

void SOUND_play_bg(sample_t* buffer, int loop)
{
    bg.sample = buffer;
    bg.ticks = 0;
    bg.idx = -1;
    bg.loop = loop;
}

void SOUND_stop_bg(void)
{
    bg.sample = NULL;
    bg.ticks = 0;
}

