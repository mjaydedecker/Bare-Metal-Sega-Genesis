//
// src/ui/ui_scale.cpp
//
// Bare Metal Sega Genesis
// See ui_scale.h.
//

#include "ui_scale.h"

unsigned ui_scale(unsigned fb_w, unsigned fb_h)
{
    unsigned sh = fb_h / UI_SCALE_MIN_H;
    unsigned sw = fb_w / UI_SCALE_MIN_W;
    unsigned s  = sh < sw ? sh : sw;
    return s < 1 ? 1 : s;
}
