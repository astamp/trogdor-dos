#ifndef VIDEO_H_
#define VIDEO_H_

#include <stdlib.h>
#include <stddef.h>

#define VGA_WIDTH (320u)
#define VGA_HEIGHT (200u)
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)
#define ALPHA_PIXEL (255u)

#define REFRESH_RATE (70)

typedef unsigned char pixel_t;
typedef unsigned int flags_t;

typedef struct {
    unsigned char* data;
    size_t data_length;
    size_t width;
    size_t height;
} sprite_t;

/* Transfers contents of video buffer to the display.
 * Waits for the start of the next vertical retrace. */
void VIDEO_flip(void);

/* Initializes the video system. */
void VIDEO_init(void);

/* Shuts down the video system and returns to text mode. */
void VIDEO_fini(void);

/* Draws a sprite to the display buffer at the given position. */
void VIDEO_draw_sprite_from_file(const char* filename, size_t sprite_width, size_t x, size_t y);

void VIDEO_draw_sprite_from_buf(const unsigned char* buffer, size_t buffer_length, size_t sprite_width, size_t x, size_t y, flags_t flags);

void VIDEO_draw_sprite(const sprite_t* sprite, size_t x, size_t y, flags_t flags);

/* Fills a rectange with the given color. */
void VIDEO_fill(size_t x, size_t y, size_t width, size_t height, pixel_t fill);

void VIDEO_test(void);

/* Load a string of text at x, y using the BIOS resident 8x8 font. */
void VIDEO_load_text(size_t x, size_t y, const char* text, pixel_t fg, pixel_t bg);

#endif // VIDEO_H_
