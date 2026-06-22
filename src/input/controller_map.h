//
// src/input/controller_map.h
//
// Bare Metal Sega Genesis
// Per-controller (VID:PID) calibration: map a pad's raw HID button bits onto our
// logical GP_* slots. Pure (no Circle dep) so it is host-testable.
//

#ifndef _input_controller_map_h
#define _input_controller_map_h

#include <stdint.h>
#include <stddef.h>

// One controller model's calibration. bit[i] is the raw HID button-bit index for
// PadButton i (A,B,X,Y,L,R,Start,Select); 255 = unmapped (button skipped/absent).
struct ControllerCal { uint16_t vid; uint16_t pid; uint8_t bit[8]; };

// Parse "vid:pid=b0,b1,...,b7" (vid/pid 4-hex, bits decimal 0..31 or 255) into
// out. Returns false on bad hex, wrong field count, an out-of-range bit, or a
// '#'/blank line.
bool controller_parse_line(const char *line, ControllerCal *out);

// Render c to "vid:pid=b0,...,b7" (NUL-terminated, truncated to out_size).
void controller_serialize_line(const ControllerCal &c, char *out, size_t out_size);

// Map raw_buttons -> face/shoulder GP_* mask: for i in 0..7, if bit[i]!=255 and
// (raw>>bit[i])&1, OR in pad_bit((PadButton)i). (D-pad handled by the caller.)
unsigned controller_decode(unsigned raw_buttons, const ControllerCal &cal);

// Find a calibration by vid/pid in arr[0..count); nullptr if absent.
const ControllerCal *controller_find(const ControllerCal *arr, unsigned count,
                                     uint16_t vid, uint16_t pid);

#endif
