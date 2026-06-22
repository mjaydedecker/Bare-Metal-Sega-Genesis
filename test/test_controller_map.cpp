#include "../src/input/controller_map.h"
#include "../src/input/joypad_map.h"   // GP_*, pad_bit, PadButton
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    // Parse a valid line.
    ControllerCal c;
    assert(controller_parse_line("2dc8:6101=0,1,3,4,6,7,10,11", &c));
    assert(c.vid == 0x2dc8 && c.pid == 0x6101);
    assert(c.bit[0] == 0 && c.bit[2] == 3 && c.bit[7] == 11);

    // 255 (unmapped) allowed.
    ControllerCal u;
    assert(controller_parse_line("1:2=0,255,2,3,4,5,6,7", &u));
    assert(u.bit[1] == 255);

    // Rejections: bad hex, wrong field count, out-of-range bit, comment/blank.
    assert(!controller_parse_line("zz:6101=0,1,2,3,4,5,6,7", &c));
    assert(!controller_parse_line("2dc8:6101=0,1,2,3,4,5,6", &c));    // 7 bits
    assert(!controller_parse_line("2dc8:6101=0,1,2,3,4,5,6,99", &c)); // 99 > 31, !=255
    assert(!controller_parse_line("# comment", &c));
    assert(!controller_parse_line("", &c));

    // Round-trip.
    ControllerCal in = { 0x045e, 0x028e, { 0,1,2,3,4,5,6,7 } };
    char line[64];
    controller_serialize_line(in, line, sizeof line);
    ControllerCal out;
    assert(controller_parse_line(line, &out));
    assert(out.vid == in.vid && out.pid == in.pid);
    for (int i = 0; i < 8; i++) assert(out.bit[i] == in.bit[i]);

    // Decode: identity-ish mapping sets the matching GP_* bits.
    ControllerCal idc = { 1, 1, { 0,1,2,3,4,5,9,8 } };  // A..Z bits, Start=bit9, Select=bit8
    unsigned raw = (1u << 0) | (1u << 1);               // A and B pressed
    unsigned m = controller_decode(raw, idc);
    assert(m == (pad_bit(PadButton::A) | pad_bit(PadButton::B)));

    // Decode: a remap (raw bit 1 -> A) and a 255 (unmapped) contributes nothing.
    ControllerCal swp = { 1, 1, { 1,0,2,3,4,5,9,8 } };  // A<-bit1, B<-bit0
    assert(controller_decode((1u << 1), swp) == pad_bit(PadButton::A));
    ControllerCal none = { 1, 1, { 255,255,255,255,255,255,255,255 } };
    assert(controller_decode(0xFFFFFFFF, none) == 0);

    // Find.
    ControllerCal arr[2] = { { 0x11,0x22,{0} }, { 0x33,0x44,{0} } };
    assert(controller_find(arr, 2, 0x33, 0x44) == &arr[1]);
    assert(controller_find(arr, 2, 0x99, 0x99) == 0);

    printf("All controller_map tests passed\n");
    return 0;
}
