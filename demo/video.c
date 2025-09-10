#include "video.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <string.h>
#include <errno.h>

#define VGA_INPUT_STATUS (0x3DAu)
#define VRETRACE (0x08)

#define FLIP_AT_RETRACE

#ifndef FLIP_AT_RETRACE
pixel_t __far * VGA = (pixel_t __far *) MK_FP(0xA000, 0x0000);
#else
pixel_t __far * VGAMEM = (pixel_t __far *) MK_FP(0xA000, 0x0000);
pixel_t VGA[VGA_SIZE];
#endif

const unsigned char* ROMFONT = (const unsigned char __far *) MK_FP(0xF000, 0xFA6E);

void VIDEO_test(void)
{
    unsigned long int retrace = 0;
    unsigned long int scan = 0;
    // Not sure where we are now, wait for the retrace to start.
    while (0x00 == (inp(VGA_INPUT_STATUS) & VRETRACE)) {
        ;
    }
    // Time the retrace.
    while (VRETRACE == (inp(VGA_INPUT_STATUS) & VRETRACE)) {
        retrace++;
    }
    // Time the scan.
    while (0x00 == (inp(VGA_INPUT_STATUS) & VRETRACE)) {
        scan++;
    }
    printf("retrace=%ld, scan=%ld\r\n", retrace, scan);
}

void VIDEO_flip(void)
{
#ifdef FLIP_AT_RETRACE
    volatile unsigned long int counts = 0;
    // Wait for the retrace to start before copying.
    while (0 == (inp(VGA_INPUT_STATUS) & VRETRACE)) {
        counts++;
    }
    memcpy(VGAMEM, VGA, VGA_SIZE);
    //debugf("counts = %lu", counts);
#endif
}

void VIDEO_init(void)
{
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    memset(VGA, 0x00u, VGA_SIZE);
    VIDEO_flip();
}

void VIDEO_fini(void)
{
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
}

// Enough for one scanline of an image.
pixel_t sprite_buf[VGA_WIDTH];
void VIDEO_draw_sprite_from_file(const char* filename, size_t sprite_width, size_t x, size_t y)
{
    FILE* fp = fopen(filename, "rb");
    if (NULL != fp) {
        size_t base = 0;
        size_t offset = 0;
        
        while (!feof(fp) && (y < VGA_HEIGHT)) {
            // Read a complete scanline of the sprite.
            size_t bytes_read = 0;
            while (!(feof(fp)) && (bytes_read < sprite_width)) {
                bytes_read += fread(sprite_buf + bytes_read, 1, sprite_width - bytes_read, fp);
            }
            
            // Determine where to start blitting.
            base = (y * VGA_WIDTH) + x;
            
            // Blit the sprite, omitting alpha pixels.
            for (offset = 0; (offset < sprite_width) && (x + offset < VGA_WIDTH); offset++) {
                if (sprite_buf[offset] != ALPHA_PIXEL) {
                    VGA[base + offset] = sprite_buf[offset];
                }
            }
            
            // Next row.
            y++;
        }
        fclose(fp);
    }
}

void VIDEO_draw_sprite_from_buf(const unsigned char* buffer, size_t buffer_length, size_t sprite_width, size_t x, size_t y, flags_t flags)
{
    size_t buffer_index = 0;
    size_t base = 0;
    size_t offset = 0;
        
    while ((buffer_index < buffer_length) && (y < VGA_HEIGHT)) {
        // Get a complete scanline of the sprite if possible.
        size_t copy_len = min(sprite_width, (buffer_length - buffer_index));
        const pixel_t* sprite_row_ptr = &(buffer[buffer_index]);
        buffer_index += copy_len;
        
        // Determine where to start blitting.
        base = (y * VGA_WIDTH) + x;
            
        // Blit the sprite, omitting alpha pixels.
        if (flags & 0x01) {
            size_t src_offset = sprite_width - 1;
            for (offset = 0; (offset < sprite_width) && (x + offset < VGA_WIDTH); offset++, src_offset--) {
                if (sprite_row_ptr[src_offset] != ALPHA_PIXEL) {
                    VGA[base + offset] = sprite_row_ptr[src_offset];
                }
            }
        } else {
            for (offset = 0; (offset < sprite_width) && (x + offset < VGA_WIDTH); offset++) {
                if (sprite_row_ptr[offset] != ALPHA_PIXEL) {
                    VGA[base + offset] = sprite_row_ptr[offset];
                }
            }
        }
            
        // Next row.
        y++;
    }
}

void VIDEO_draw_sprite(const sprite_t* sprite, size_t x, size_t y, flags_t flags)
{
    VIDEO_draw_sprite_from_buf(
        sprite->data,
        sprite->data_length,
        sprite->width,
        x,
        y,
        flags
    );
}

void VIDEO_fill(size_t x, size_t y, size_t width, size_t height, pixel_t fill)
{
    size_t count = __min(x + width, VGA_WIDTH) - x;
    size_t yend = __min(y + height, VGA_HEIGHT);
    
    while ((y < yend)) {
        // Determine where to start blitting.
        size_t base = (y * VGA_WIDTH) + x;
        size_t offset = 0;
        
        // Blit the sprite, omitting alpha pixels.
        for (offset = 0; offset < count; offset++) {
            VGA[base + offset] = fill;
        }
        
        // Next row.
        y++;
    }
}

void VIDEO_load_text(size_t x, size_t y, const char* text, pixel_t fg, pixel_t bg)
{
    size_t offset;
    const unsigned char __far * glyph;
    char ch;
    size_t row;
    unsigned char scanline;
    unsigned char bitmask;
    while (ch = *text++) {
        glyph = ROMFONT + (ch * 8);
        offset = (y * VGA_WIDTH + x);
        for (row = 0; row < 8; row++, glyph++) {
            scanline = *glyph;
            bitmask = 0x80;
            while (bitmask) {
                if (scanline & bitmask) {
                    VGA[offset] = fg;
                } else if (bg != ALPHA_PIXEL) {
                    VGA[offset] = bg;
                }
                offset++;
                bitmask = bitmask >> 1u;
            }
            offset += (VGA_WIDTH - 8);
        }
        x += 8;
    }
}
