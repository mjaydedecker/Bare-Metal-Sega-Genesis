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

    // Defaults for the new audio fields.
    assert(d.volume == 100);
    assert(d.mute == false);

    // Parse + clamp volume; mute truthy.
    Settings v = parse_settings("volume=70\nmute=on\n");
    assert(v.volume == 70);
    assert(v.mute == true);
    Settings vc = parse_settings("volume=250\n");      // clamps to 100
    assert(vc.volume == 100);
    Settings vb = parse_settings("volume=bogus\n");    // non-numeric -> default
    assert(vb.volume == 100);

    // Round-trip includes volume + mute.
    Settings as; as.volume = 30; as.mute = true;
    char abuf[256];
    serialize_settings(as, abuf, sizeof abuf);
    Settings art = parse_settings(abuf);
    assert(art.volume == 30);
    assert(art.mute == true);

    // Region defaults + parse + round-trip.
    assert(d.region == Region::Auto);
    assert(parse_settings("region=pal\n").region == Region::PAL);
    assert(parse_settings("region=ntsc\n").region == Region::NTSC);
    assert(parse_settings("region=bogus\n").region == Region::Auto);

    // Region file + core value mappings.
    assert(strcmp(region_file_value(Region::Auto), "auto") == 0);
    assert(strcmp(region_file_value(Region::NTSC), "ntsc") == 0);
    assert(strcmp(region_file_value(Region::PAL),  "pal")  == 0);
    assert(strcmp(region_core_value(Region::NTSC), "ntsc-u") == 0);
    assert(strcmp(region_core_value(Region::PAL),  "pal")    == 0);
    assert(strcmp(region_core_value(Region::Auto), "auto")   == 0);

    // auto_launch_rom defaults empty; parses a path with spaces; round-trips.
    assert(d.auto_launch_rom[0] == '\0');
    Settings al = parse_settings("auto_launch_rom=SD:/roms/Streets of Rage.md\n");
    assert(strcmp(al.auto_launch_rom, "SD:/roms/Streets of Rage.md") == 0);

    Settings rs; rs.region = Region::PAL;
    const char *p = "SD:/roms/Sonic 2.md";
    for (unsigned i = 0; p[i]; i++) rs.auto_launch_rom[i] = p[i];
    char rbuf[512];
    serialize_settings(rs, rbuf, sizeof rbuf);
    Settings rrt = parse_settings(rbuf);
    assert(rrt.region == Region::PAL);
    assert(strcmp(rrt.auto_launch_rom, "SD:/roms/Sonic 2.md") == 0);

    printf("All settings tests passed\n");
    return 0;
}
