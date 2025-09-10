#include "video.h"
#include "keybrd.h"
#include "gfx.h"
#include "debug.h"
#include "collide.h"
#include "sound.h"
#include "gup.h"

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>
#include <errno.h>

// Globals
sprite_t* gup_sprite = NULL;
sprite_t* gup_sprite_2 = NULL;

extern int gup_chooser(const char* title);

extern void VEGRESQ_init(void);
extern bool VEGRESQ_loop(void);
extern void VEGRESQ_fini(void);

extern void VBATTLE_init(void);
extern bool VBATTLE_loop(void);
extern void VBATTLE_fini(void);

#if 0
sample_t sound_gupx[] = {
    { 200,  60 },
    { 0,    60 },
    { 125,  60 },
    { 0,    60 },
    { SOUND_EOF, SOUND_EOF }
};
#else
sample_t sound_gupx[] = {
    { 30*3,  10 },
    { 0,   2 },
    { 32*3,  10 },
    { 0,   20 },
    { 24*3,  20 },
    { 0,   10 },
    { 26*3,  20 },
    { 0,   30 },

    { 30*3,  10 },
    { 0,   2 },
    { 32*3,  10 },
    { 0,   20 },
    { 27*3,  20 },
    { 0,   10 },
    { 32*3,  20 },
    { 0,   30 },
    { SOUND_EOF, SOUND_EOF }
};
#endif
sample_t sound_coin[] = {
    { 1000, 5 },
    { 1500, 5 },
    { 2000, 5 },
    { 0,    5 },
    { SOUND_EOF, SOUND_EOF }
};

void move_gup(vehicle_t* gup, movement_mode_t mode)
{
    if (NULL == gup) {
        return;
    }
    
    if (gup->ys < 0) {
        // Handle up movement.
        if (gup->y > abs(gup->ys)) {
            gup->y += gup->ys;
        } else if (MOVEMENT_BOUNCE == mode) {
            gup->y = 0;
            gup->ys = -gup->ys;
        } else if (MOVEMENT_NORMAL == mode) {
            gup->y = 0;
            gup->ys = 0;
        }
    } else {
        // Handle down movement.
        if (gup->y + gup->h + gup->ys < VGA_HEIGHT) {
            gup->y += gup->ys;
        } else if (MOVEMENT_BOUNCE == mode) {
            gup->y = (VGA_HEIGHT - gup->h) - 1;
            gup->ys = -gup->ys;
        } else if (MOVEMENT_NORMAL == mode) {
            gup->y = (VGA_HEIGHT - gup->h) - 1;
            gup->ys = 0;
        }
    }

    if (gup->xs < 0) {
        // Handle left movement.
        if (gup->x > abs(gup->xs)) {
            gup->x += gup->xs;
        } else if (MOVEMENT_BOUNCE == mode) {
            gup->x = 0;
            gup->xs = -gup->xs;
        } else if (MOVEMENT_NORMAL == mode) {
            gup->x = 0;
            gup->xs = 0;
        }
    } else {
        // Handle right movement.
        if (gup->x + gup->w + gup->xs < VGA_WIDTH) {
            gup->x += gup->xs;
        } else if (MOVEMENT_BOUNCE == mode) {
            gup->x = (VGA_WIDTH - gup->w) - 1;
            gup->xs = -gup->xs;
        } else if (MOVEMENT_NORMAL == mode) {
            gup->x = (VGA_WIDTH - gup->w) - 1;
            gup->xs = 0;
        }
    }

    if (gup->xs > 0) {
        gup->looking = RIGHT;
    } else if (gup->xs < 0) {
        gup->looking = LEFT;
    }
}


int main(int argc, char** argv)
{
    bool running = true;
    key_t key;
    int x;

    int stats_interval = REFRESH_RATE;
    clock_t this_marker = 0, last_marker = 0;

    srand(time(NULL) & 0xFFFFu);

    DEBUG_init();
    KEYBRD_init();

    SOUND_init();
    GFX_init();
    VIDEO_init();
    
    VIDEO_draw_sprite(LOGO01, 0, 0, 0);
    VIDEO_flip();
    
    KEYBRD_read();
    
    x = gup_chooser("Player 1, pick your GUP");
    switch (x) {
    case 0: gup_sprite = GUPA01; break;
    case 1: gup_sprite = GUPB01; break;
    case 2: gup_sprite = GUPC01; break;
    case 3: gup_sprite = GUPD01; break;
    case 4: gup_sprite = GUPE01; break;
    case 5: gup_sprite = GUPX01; SOUND_play_bg(sound_gupx, 1); break;
    }
    
    x = gup_chooser("Player 2, pick your GUP");
    switch (x) {
    case 0: gup_sprite_2 = GUPA01; break;
    case 1: gup_sprite_2 = GUPB01; break;
    case 2: gup_sprite_2 = GUPC01; break;
    case 3: gup_sprite_2 = GUPD01; break;
    case 4: gup_sprite_2 = GUPE01; break;
    case 5: gup_sprite_2 = GUPX01; SOUND_play_bg(sound_gupx, 1); break;
    }

//    VEGRESQ_init();
    VBATTLE_init();

    do {
//        running = VEGRESQ_loop();
        running = VBATTLE_loop();

        // Flip at vsync.
        VIDEO_flip();
        SOUND_poll();
        if (--stats_interval == 0) {
            this_marker = clock();
            if ((last_marker != 0) && (this_marker - last_marker) > 1100) {
                debugf("dropping frames! %u frame duration = %lu",
                       REFRESH_RATE, (this_marker - last_marker));
            }
            last_marker = this_marker;
            stats_interval = REFRESH_RATE;
        }
        
    } while (running);

//    VEGRESQ_fini();
    VBATTLE_fini();

    SOUND_fini();

    VIDEO_flip();

    do {
        key = KEYBRD_read();
        if (KX_ESCAPE == key || KX_ENTER == key) {
            break;
        }
    } while (1);
    
    VIDEO_fini();

    KEYBRD_fini();
    return 0;
}
