//
// src/ui/ui_scale.h
//
// Bare Metal Sega Genesis
// Integer UI scale for the current output resolution. The framebuffer IS the
// HDMI mode, so fixed-pixel menus shrink as the mode grows; GlyphCanvas divides
// the physical size by this scale to get a logical drawing surface that is
// always at least UI_SCALE_MIN_W x UI_SCALE_MIN_H (every screen fits 640x480).
// Whole-number steps keep the bitmap fonts pixel-perfect. Pure; no Circle deps.
//

#ifndef _ui_ui_scale_h
#define _ui_ui_scale_h

#define UI_SCALE_MIN_W 640
#define UI_SCALE_MIN_H 480

// max(1, min(fb_h / UI_SCALE_MIN_H, fb_w / UI_SCALE_MIN_W)).
// 480p/720p -> 1, 1080p -> 2, 1440p -> 3. Never returns 0.
unsigned ui_scale(unsigned fb_w, unsigned fb_h);

#endif
