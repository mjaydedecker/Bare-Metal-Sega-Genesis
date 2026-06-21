//
// src/video/splash.h
//
// Bare Metal Sega Genesis
// Boot splash: pure SGSP-buffer parse + integer-scale helpers (host-testable),
// plus Circle-side draws (defined in splash_draw.cpp).
//

#ifndef _video_splash_h
#define _video_splash_h

#include <stddef.h>

// A parsed view over an SGSP buffer; pixels point INTO the caller's buffer.
struct SplashImage { unsigned w; unsigned h; const unsigned short *pixels; };

// Validate an SGSP buffer: magic 'SGSP', non-zero dims, and exactly
// 8 + w*h*2 bytes. On success fills out and returns true; else returns false.
bool splash_parse(const unsigned char *buf, size_t len, SplashImage *out);

// Largest integer scale with imgW*scale <= fbW*7/10 AND imgH*scale <= fbH*7/10
// (minimum 1; returns 1 if either image dimension is 0).
unsigned splash_scale(unsigned fbW, unsigned fbH, unsigned imgW, unsigned imgH);

// Circle-side draws (splash_draw.cpp). Forward-declared dependencies.
class TextCanvas;
class Display;
class Storage;

// Clear to black and blit the EMBEDDED logo centered, integer-scaled.
void splash_show_embedded(TextCanvas *canvas, Display *display);
// If SD:/splash.raw parses as a valid SGSP image, clear + blit it instead.
void splash_apply_override(Storage *storage, TextCanvas *canvas, Display *display);

#endif
