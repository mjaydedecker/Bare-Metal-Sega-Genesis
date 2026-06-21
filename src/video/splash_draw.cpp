//
// src/video/splash_draw.cpp
//
// Bare Metal Sega Genesis
// Circle-side boot-splash draws: blit the embedded (or SD-override) logo
// centered on a black framebuffer. See splash.h.
//

#include "splash.h"
#include "splash_data.h"
#include "display.h"
#include "blit.h"
#include "../ui/text_canvas.h"
#include "../storage/storage.h"

// Clear to black and blit a w*h RGB565 image centered, integer-scaled.
static void draw_image(TextCanvas *canvas, Display *display,
                       const unsigned short *pixels, unsigned w, unsigned h)
{
    if (display->Buffer() == 0) return;
    canvas->Clear(0x0000);
    unsigned s = splash_scale(display->Width(), display->Height(), w, h);
    blit_rgb565((unsigned short *) display->Buffer(), display->Pitch(),
                display->Width(), display->Height(),
                pixels, w * 2, w, h, s);   // blit_rgb565 centers the image
}

void splash_show_embedded(TextCanvas *canvas, Display *display)
{
    draw_image(canvas, display, g_splash_data, g_splash_w, g_splash_h);
}

void splash_apply_override(Storage *storage, TextCanvas *canvas, Display *display)
{
    const char *path = "SD:/splash.raw";
    if (!storage->Exists(path)) return;

    u8    *buf  = 0;
    size_t size = 0;
    if (!storage->ReadFile(path, &buf, &size)) return;   // ReadFile new[]s buf

    SplashImage img;
    if (splash_parse(buf, size, &img))
        draw_image(canvas, display, img.pixels, img.w, img.h);

    delete[] buf;
}
