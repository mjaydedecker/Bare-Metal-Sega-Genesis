//
// src/runtime_stubs.c
//
// Bare Metal Sega Genesis
// Bare-metal runtime stubs for stack-protector and errno.
//
// glibc math functions are replaced by our own implementations in
// src/libretro/stubs.c, so libm.a is no longer linked.  We still need
// these stubs for Circle code that uses glibc headers at STDLIB_SUPPORT=1.
//

#include <stdint.h>

// ---------------------------------------------------------------------------
// errno — plain global (no TLS needed; libm is not linked)
// ---------------------------------------------------------------------------

int errno = 0;

// __errno_location() — glibc convention; returns &errno.
int *__errno_location(void)
{
    return &errno;
}

// ---------------------------------------------------------------------------
// Stack-protector support
// ---------------------------------------------------------------------------

// A fixed canary is fine for bare metal: we have no adversary to leak it.
uintptr_t __stack_chk_guard = (uintptr_t)0xdeadc0de;

__attribute__((noreturn))
void __stack_chk_fail(void)
{
    // Spin forever — a real fault handler could be added later.
    for (;;)
        ;
}
