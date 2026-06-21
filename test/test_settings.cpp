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

    // Menu hotkey defaults + parse + invalid fallback.
    assert(d.menu_hotkey == MenuHotkey::StartSelect);
    assert(parse_settings("menu_hotkey=start+a\n").menu_hotkey == MenuHotkey::StartA);
    assert(parse_settings("menu_hotkey=start+b\n").menu_hotkey == MenuHotkey::StartB);
    assert(parse_settings("menu_hotkey=l+r\n").menu_hotkey      == MenuHotkey::LR);
    assert(parse_settings("menu_hotkey=bogus\n").menu_hotkey    == MenuHotkey::StartSelect);

    // File-value mapping.
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartSelect), "start+select") == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartA),      "start+a")      == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::StartB),      "start+b")      == 0);
    assert(strcmp(menu_hotkey_file_value(MenuHotkey::LR),          "l+r")          == 0);

    // Round-trip.
    Settings hs; hs.menu_hotkey = MenuHotkey::LR;
    char hbuf[512];
    serialize_settings(hs, hbuf, sizeof hbuf);
    assert(parse_settings(hbuf).menu_hotkey == MenuHotkey::LR);

    // PadButton tokens.
    assert(strcmp(pad_button_token(PadButton::A), "a") == 0);
    assert(strcmp(pad_button_token(PadButton::L), "l") == 0);
    assert(strcmp(pad_button_token(PadButton::Select), "select") == 0);

    // Default map reproduces current behavior.
    Settings dms;
    assert(dms.map1.b[0] == PadButton::Y);   // Genesis A -> physical Y
    assert(dms.map1.b[2] == PadButton::A);   // Genesis C -> physical A
    assert(dms.map2.b[7] == PadButton::Select);

    // Parse a valid map; round-trips.
    ButtonMap pm = parse_button_map("a,b,x,y,l,r,start,select");
    assert(pm.b[0] == PadButton::A && pm.b[7] == PadButton::Select);
    // Bad token -> default.
    assert(parse_button_map("a,b,x,y,l,r,start,nope").b[0] == PadButton::Y);
    // Wrong count -> default.
    assert(parse_button_map("a,b,c").b[0] == PadButton::Y);

    // Settings round-trip with a custom map.
    Settings cs; cs.map1.b[0] = PadButton::A;
    char cbuf[512];
    serialize_settings(cs, cbuf, sizeof cbuf);
    assert(parse_settings(cbuf).map1.b[0] == PadButton::A);

    // Video mode dims.
    {
        unsigned w = 9, h = 9;
        video_mode_dims(VideoMode::Native, w, h); assert(w == 0 && h == 0);
        video_mode_dims(VideoMode::P1080, w, h);  assert(w == 1920 && h == 1080);
        video_mode_dims(VideoMode::P720, w, h);   assert(w == 1280 && h == 720);
        video_mode_dims(VideoMode::P480, w, h);   assert(w == 720 && h == 480);
    }
    // Video mode defaults + parse + file value + round-trip.
    assert(d.video_mode == VideoMode::Native);
    assert(parse_settings("video_mode=720p\n").video_mode == VideoMode::P720);
    assert(parse_settings("video_mode=1080p\n").video_mode == VideoMode::P1080);
    assert(parse_settings("video_mode=480p\n").video_mode == VideoMode::P480);
    assert(parse_settings("video_mode=bogus\n").video_mode == VideoMode::Native);
    assert(strcmp(video_mode_file_value(VideoMode::P720), "720p") == 0);
    assert(strcmp(video_mode_file_value(VideoMode::Native), "native") == 0);
    Settings vms; vms.video_mode = VideoMode::P480;
    char vmbuf[512];
    serialize_settings(vms, vmbuf, sizeof vmbuf);
    assert(parse_settings(vmbuf).video_mode == VideoMode::P480);

    // Audio output defaults + parse + invalid fallback + file value + round-trip.
    assert(d.audio_output == AudioOutput::HDMI);
    assert(parse_settings("audio_output=analog\n").audio_output == AudioOutput::Analog);
    assert(parse_settings("audio_output=hdmi\n").audio_output   == AudioOutput::HDMI);
    assert(parse_settings("audio_output=bogus\n").audio_output  == AudioOutput::HDMI);
    assert(strcmp(audio_output_file_value(AudioOutput::HDMI),   "hdmi")   == 0);
    assert(strcmp(audio_output_file_value(AudioOutput::Analog), "analog") == 0);
    Settings aos; aos.audio_output = AudioOutput::Analog;
    char aobuf[512];
    serialize_settings(aos, aobuf, sizeof aobuf);
    assert(parse_settings(aobuf).audio_output == AudioOutput::Analog);

    printf("All settings tests passed\n");
    return 0;
}
