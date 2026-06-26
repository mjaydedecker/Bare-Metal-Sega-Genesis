//
// src/input/pad_toast.h
//
// Bare Metal Sega Genesis
// Pure formatter for the "GPIO pad detected" toast. Builds a label like
// "GPIO P1: 6-button" into a caller-provided buffer — no snprintf (bare metal),
// no Circle deps, bounds-safe and always NUL-terminated. Host-testable.
//

#ifndef _input_pad_toast_h
#define _input_pad_toast_h

#include "sega_pad.h"   // SegaPadType

// Write a detection label for a 0-based port (shown 1-based) and pad type into
// buf. Always NUL-terminates within bufsize; truncates safely if too small.
// For non-3/6-button types the label still forms but the count word is "?".
void pad_toast_label(char *buf, unsigned bufsize, unsigned port, SegaPadType type);

#endif
