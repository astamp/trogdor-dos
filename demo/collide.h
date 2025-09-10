#ifndef COLLIDE_H_
#define COLLIDE_H_

#include "video.h"

int COLLIDE_bbox_check(
    const sprite_t* sprite1, size_t x1, size_t y1,
    const sprite_t* sprite2, size_t x2, size_t y2);
    

#endif // COLLIDE_H_
