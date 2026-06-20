#include "../src/settings/settings.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Defaults for empty / NULL input.
    Settings d = parse_settings("");
    assert(d.scale_mode == ScaleMode::Integer);
    assert(d.widescreen == false);
    assert(parse_settings(0).scale_mode == ScaleMode::Integer);

    // Valid values.
    Settings a = parse_settings("video_scale=stretch\nwidescreen=on\n");
    assert(a.scale_mode == ScaleMode::Stretch);
    assert(a.widescreen == true);

    // Comments, blank lines, surrounding whitespace, case-insensitive.
    Settings b = parse_settings(
        "# comment\n\n  VIDEO_SCALE = Stretch  \nWidescreen=TRUE\n");
    assert(b.scale_mode == ScaleMode::Stretch);
    assert(b.widescreen == true);

    // Invalid value -> default; unknown key ignored; widescreen=0 -> false.
    Settings c = parse_settings("video_scale=bogus\nfoo=bar\nwidescreen=0\n");
    assert(c.scale_mode == ScaleMode::Integer);
    assert(c.widescreen == false);

    // Round-trip: serialize then parse yields equal settings.
    Settings src; src.scale_mode = ScaleMode::Stretch; src.widescreen = true;
    char buf[256];
    serialize_settings(src, buf, sizeof buf);
    Settings rt = parse_settings(buf);
    assert(rt.scale_mode == ScaleMode::Stretch);
    assert(rt.widescreen == true);

    printf("All settings tests passed\n");
    return 0;
}
