#define RING_SIZE (32)
typedef int ring_item_t;

/* Initialize the ring buffer and pointers. */
void RING_init(void);

/* Returns 1 if the buffer is empty, cannot get(). */
int RING_empty(void);

/* Returns 1 if the buffer is full, cannot put(). */
int RING_full(void);

/* If space is available, puts item into the buffer and returns 1.
 * Returns 0 if no space available. */
int RING_put(ring_item_t item);

/* If items in buffer, puts item into *item and returns 1.
 * Returns 0 if no items available. */
int RING_get(ring_item_t* item);

