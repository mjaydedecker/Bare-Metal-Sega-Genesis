//
// src/video/splash.cpp
//
// Bare Metal Sega Genesis
// See splash.h — pure SGSP parse + integer-scale helpers (no Circle deps).
//

#include "splash.h"

bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out)
{
    if (buf == 0 || len < 8) return false;
    if (buf[0] != 'S' || buf[1] != 'G' || buf[2] != 'S' || buf[3] != 'P')
        return false;
    unsigned w = (unsigned) buf[4] | ((unsigned) buf[5] << 8);
    unsigned h = (unsigned) buf[6] | ((unsigned) buf[7] << 8);
    if (w == 0 || h == 0) return false;
    if (len != (size_t) 8 + (size_t) w * h * 2) return false;
    out->w = w;
    out->h = h;
    out->pixels = (const unsigned short *) (buf + 8);
    return true;
}

unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH)
{
    if (imgW == 0 || imgH == 0) return 1;
    unsigned maxW = fbW * 7 / 10;
    unsigned maxH = fbH * 7 / 10;
    unsigned s = 1;
    while (imgW * (s + 1) <= maxW && imgH * (s + 1) <= maxH) s++;
    return s;
}
