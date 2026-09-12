#include "../src/version.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// BMSG_VERSION must be plain MAJOR.MINOR.PATCH (no leading 'v', no suffix):
// tools/mkdist.sh parses it and builds "v<version>" release names from it.
static bool is_semver(const char *s)
{
    int parts = 0;
    while (true)
    {
        if (*s < '0' || *s > '9') return false;          // each part starts with a digit
        if (*s == '0' && s[1] >= '0' && s[1] <= '9') return false;   // no leading zeros
        while (*s >= '0' && *s <= '9') s++;
        parts++;
        if (*s == '\0') return parts == 3;
        if (*s != '.' || parts == 3) return false;
        s++;
    }
}

int main(void)
{
    // The checker itself.
    assert(is_semver("0.1.0"));
    assert(is_semver("10.20.30"));
    assert(!is_semver("v0.1.0"));
    assert(!is_semver("0.1"));
    assert(!is_semver("0.1.0.1"));
    assert(!is_semver("0.1.0-dev"));
    assert(!is_semver("01.1.0"));
    assert(!is_semver(""));

    // The project version.
    assert(is_semver(BMSG_VERSION));
    assert(strcmp(BMSG_VERSION, "0.1.0") == 0);

    // Display form used by the firmware ("v" prefix via string concatenation).
    assert(strcmp(BMSG_VERSION_LABEL, "v0.1.0") == 0);

    printf("test_version: OK\n");
    return 0;
}
