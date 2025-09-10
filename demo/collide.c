#include "collide.h"

int COLLIDE_bbox_check(
    const sprite_t* sprite1, size_t x1, size_t y1,
    const sprite_t* sprite2, size_t x2, size_t y2)
{
    if ((x1 < x2 + sprite2->width) &&   // #2 right edge
        (x1 + sprite1->width > x2) &&   // #2 left edge
        (y1 < y2 + sprite2->height) &&  // #2 bottom edge
        (y1 + sprite1->height > y2)) {  // #2 top edge
        return 1;
    } else {
        return 0;
    }
}
