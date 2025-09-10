#include "keybrd.h"

#include <conio.h>
#include <stdio.h>

key_t KEYBRD_read(void)
{
    key_t key = K_INVALID;

    int ch = getch();
    switch (ch) {
    case EOF:
        key = K_NONE;
        break;

    case 0:
        ch = getch();
        //printf("extended key=%d 0x%x [%c]\r\n", ch, ch, ch);
        switch (ch) {
        case 0x48: key = K_UP; break;
        case 0x50: key = K_DOWN; break;
        case 0x4B: key = K_LEFT; break;
        case 0x4D: key = K_RIGHT; break;
        default: break;
        }

        break;

    case 0x1B: key = K_ESCAPE; break;
    case ' ': key = K_SPACE; break;

    default:
        //printf("standard key=%d 0x%x [%c]\r\n", ch, ch, ch);
        break;
    }
    return key;
}

key_t KEYBRD_poll(void)
{
    if (kbhit()) {
        return KEYBRD_read();
    } else {
        return K_NONE;
    }
}

