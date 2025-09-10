#include "debug.h"
#include <i86.h>
#include <stdarg.h>
#include <stdio.h>

#define MDA_COLS (80)
#define MDA_STRIDE (MDA_COLS * 2)
#define MAX_DEBUG_LINE (80)
static char letter;
static char debug_buffer[MAX_DEBUG_LINE+1];
static unsigned int debug_line = 0;

static char __far * mda = MK_FP(0xB000, 0x0000);

void DEBUG_init()
{
    letter = 'A';
    debug_line = 0;
}

void DEBUG_fini()
{
}

void DEBUG_printf(const char* format, ...)
{
    va_list args;

    debug_buffer[0] = letter;
    letter++;
    if (letter > 'Z') {
        letter = 'A';
    }
    va_start(args, format);
    vsnprintf(debug_buffer + 1, sizeof(debug_buffer) - 1, format, args);
    va_end(args);

    {
        unsigned int x;
        unsigned int pos = debug_line * MDA_STRIDE;
        for (x = 0; debug_buffer[x] && x < MDA_COLS; x++, pos += 2) {
            mda[pos] = debug_buffer[x];
            mda[pos + 1] = 0x07;
        }
        for (; x < MDA_COLS; x++, pos += 2) {
            mda[pos] = ' ';
            mda[pos + 1] = 0x07;
        }
    }

    debug_line += 1;
    if (debug_line == 25) {
        debug_line = 0;
    }
    
}

