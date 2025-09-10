#include "ring.h"

volatile ring_item_t ring[RING_SIZE + 1];
static volatile unsigned int head;
static volatile unsigned int tail;
static volatile unsigned int count;

void RING_init(void)
{
    head = tail = count = 0;
}

int RING_empty(void)
{
    return (0 == count);
}

int RING_full(void)
{
    return (RING_SIZE == count);
}

int RING_put(ring_item_t item)
{
    if (RING_full()) {
        return 0;
    } else {
        ring[head++] = item;
        count++;
        if (RING_SIZE == head) {
            head = 0;
        }
        return 1;
    }
}

int RING_get(ring_item_t* item)
{
    if (RING_empty()) {
        return 0;
    } else {
        *item = ring[tail++];
        count--;
        if (RING_SIZE == tail) {
            tail = 0;
        }
        return 1;
    }
}

