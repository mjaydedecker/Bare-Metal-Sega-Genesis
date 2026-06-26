//
// src/input/pad_toast.cpp
//
#include "pad_toast.h"

void pad_toast_label(char *buf, unsigned bufsize, unsigned port, SegaPadType type)
{
    if (bufsize == 0)
        return;

    char count = (type == SegaPadType::SixButton)   ? '6'
               : (type == SegaPadType::ThreeButton) ? '3'
               : '?';

    // Template with placeholders: port digit at index 6, count word at index 9.
    const char *tmpl = "GPIO P0: N-button";
    unsigned i = 0;
    for (; tmpl[i] != '\0' && i < bufsize - 1; ++i)
        buf[i] = tmpl[i];
    buf[i] = '\0';

    // Patch placeholders only if they survived truncation.
    if (i > 6) buf[6] = (char)('1' + port);   // 0-based port shown 1-based
    if (i > 9) buf[9] = count;
}
