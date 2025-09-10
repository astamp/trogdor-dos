#ifndef KEYBRD_H
#define KEYBRD_H

typedef enum _keystate {
    KS_UP,
    KS_DOWN,
    KS_NOCHANGE,
} keystate_t;

typedef enum _key {
    KX_NONE,
    KX_INVALID,
    KX_ESCAPE,
    KX_ENTER,

    KD_P1_UP,
    KD_P1_DOWN,
    KD_P1_LEFT,
    KD_P1_RIGHT,
    KU_P1_UP,
    KU_P1_DOWN,
    KU_P1_LEFT,
    KU_P1_RIGHT,

    KD_P2_UP,
    KD_P2_DOWN,
    KD_P2_LEFT,
    KD_P2_RIGHT,
    KU_P2_UP,
    KU_P2_DOWN,
    KU_P2_LEFT,
    KU_P2_RIGHT,

    KX_SPACE,
    KX_COUNT,

    // Generic arrow keys map to player 2 (arrows).
    KD_UP = KD_P2_UP,
    KD_DOWN = KD_P2_DOWN,
    KD_LEFT = KD_P2_LEFT,
    KD_RIGHT = KD_P2_RIGHT,
    KU_UP = KU_P2_UP,
    KU_DOWN = KU_P2_DOWN,
    KU_LEFT = KU_P2_LEFT,
    KU_RIGHT = KU_P2_RIGHT
} key_t;

/* Wait for one keystroke and return the enum. */
key_t KEYBRD_read(void);

/* Return one keystroke enum if available else KX_NONE. */
key_t KEYBRD_poll(void);

/* Return the state of a given key. */
keystate_t KEYBRD_getstate(key_t key);

/* Initialize the keyboard system. */
void KEYBRD_init(void);

/* Restores original keyboard handler. */
void KEYBRD_fini(void);

#endif // KEYBRD_H
