#include "../src/input/pad_toast.h"
#include "../src/input/sega_pad.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    char buf[20];

    // 6-button on port 0 -> shown as P1.
    pad_toast_label(buf, sizeof buf, 0, SegaPadType::SixButton);
    assert(strcmp(buf, "GPIO P1: 6-button") == 0);

    // 3-button on port 1 -> shown as P2.
    pad_toast_label(buf, sizeof buf, 1, SegaPadType::ThreeButton);
    assert(strcmp(buf, "GPIO P2: 3-button") == 0);

    // Always NUL-terminated within the buffer.
    pad_toast_label(buf, sizeof buf, 0, SegaPadType::SixButton);
    assert(buf[sizeof buf - 1] == '\0' || strlen(buf) < sizeof buf);

    // Short buffer truncates safely (still NUL-terminated, no overflow).
    char small[8];
    for (unsigned i = 0; i < sizeof small; ++i) small[i] = (char)0x7F;  // poison
    pad_toast_label(small, sizeof small, 0, SegaPadType::SixButton);
    assert(small[sizeof small - 1] == '\0');
    assert(strncmp(small, "GPIO P1", 7) == 0);

    // Buffer too small to even reach the port digit: must not patch out of range.
    char tiny[4];
    pad_toast_label(tiny, sizeof tiny, 0, SegaPadType::SixButton);
    assert(tiny[sizeof tiny - 1] == '\0');
    assert(strncmp(tiny, "GPI", 3) == 0);

    printf("test_pad_toast: OK\n");
    return 0;
}
