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
#include "../ui/glyph_canvas.h"
#include "../ui/theme.h"
#include "../ui/fonts/font_ps2p8.h"
#include "../ui/fonts/font_vt323_22.h"
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

// Typographic boot splash (Claude Design v1.0.0): centered wordmark.
void splash_show_text(GlyphCanvas *canvas, Display *display)
{
    if (display->Buffer() == 0) return;
    int W = (int) canvas->Width();
    int H = (int) canvas->Height();
    canvas->Clear(theme::BG);

    const char *kicker = "BARE . METAL";
    int kw = canvas->TextWidth(&g_font_ps2p8, 2, kicker);
    canvas->Text(&g_font_ps2p8, 2, W / 2 - kw / 2, H / 2 - 120, kicker,
                 theme::VALUE, theme::BG, true);

    const char *title = "GENESIS";
    int tw = canvas->TextWidth(&g_font_ps2p8, 6, title);
    canvas->Text(&g_font_ps2p8, 6, W / 2 - tw / 2, H / 2 - 70, title,
                 theme::WHITE, theme::BG, true);

    canvas->FillRect(W / 2 - 80, H / 2 + 10, 160, 4, theme::SELECTION);

    const char *sub = "EMULATOR";
    int sw = canvas->TextWidth(&g_font_vt323_22, 1, sub);
    canvas->Text(&g_font_vt323_22, 1, W / 2 - sw / 2 - 30, H / 2 + 36, sub,
                 theme::TEXT_MUTED, theme::BG, true);
    canvas->IconButton(W / 2 + sw / 2 - 10, H / 2 + 34, 18, '6',
                       theme::ADJUST, theme::BG, &g_font_ps2p8);

    // Passive status, not a prompt: the splash auto-dismisses when the ROM
    // browser renders over it; nothing polls START at this stage.
    const char *wait = "PLEASE WAIT";
    int ww = canvas->TextWidth(&g_font_ps2p8, 2, wait);
    canvas->Text(&g_font_ps2p8, 2, W / 2 - ww / 2, H - 180, wait,
                 theme::WHITE, theme::BG, true);

    const char *foot = "RASPBERRY PI . CIRCLE . LIBRETRO / GENESIS PLUS GX";
    int fw = canvas->TextWidth(&g_font_vt323_22, 1, foot);
    canvas->Text(&g_font_vt323_22, 1, W / 2 - fw / 2, H - 120, foot,
                 theme::TEXT_DIM, theme::BG, true);

    canvas->Scanlines(0, 0, W, H, 60);
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
