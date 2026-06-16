//
// src/cstdlib_stubs.c
//
// Bare Metal Sega Genesis
// C standard-library functions needed by Genesis-Plus-GX-Wide that are not
// provided by Circle's bare-metal environment.
//
// Uses only Circle's own headers so glibc fortification wrappers
// (__memcpy_chk, __memset_chk, etc.) are never pulled in.
//

#include <circle/util.h>
#include <circle/alloc.h>
#include <circle/types.h>

// ---------------------------------------------------------------------------
// strtol
// ---------------------------------------------------------------------------

long strtol(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    long result   = 0;
    int  sign     = 1;

    while (*p == ' ' || *p == '\t') p++;
    if      (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }

    if ((base == 0 || base == 16) &&
        p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    if (base == 0) base = (p[0] == '0') ? 8 : 10;

    while (*p)
    {
        int digit;
        if (*p >= '0' && *p <= '9')      digit = *p - '0';
        else if (*p >= 'a' && *p <= 'z') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') digit = *p - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        p++;
    }

    if (endptr) *endptr = (char *)p;
    return sign * result;
}

// ---------------------------------------------------------------------------
// strtok
// ---------------------------------------------------------------------------

char *strtok(char *str, const char *delim)
{
    static char *saved = 0;

    if (str) saved = str;
    else if (!saved || !*saved) return 0;

    while (*saved && strchr(delim, *saved)) saved++;
    if (!*saved) return 0;

    char *start = saved;
    while (*saved && !strchr(delim, *saved)) saved++;

    if (*saved) { *saved = '\0'; saved++; }

    return start;
}

// ---------------------------------------------------------------------------
// strrchr
// ---------------------------------------------------------------------------

char *strrchr(const char *s, int c)
{
    const char *last = 0;
    do {
        if ((unsigned char)*s == (unsigned char)c) last = s;
    } while (*s++);
    return (char *)last;
}

// ---------------------------------------------------------------------------
// strdup
// ---------------------------------------------------------------------------

char *strdup(const char *s)
{
    if (!s) return 0;
    size_t n  = strlen(s) + 1;
    char  *p  = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

// ---------------------------------------------------------------------------
// rand / srand
// ---------------------------------------------------------------------------

static unsigned long s_rand_seed = 12345;

int rand(void)
{
    s_rand_seed = s_rand_seed * 1103515245UL + 12345;
    return (int)((s_rand_seed >> 16) & 0x7fff);
}

void srand(unsigned int seed)
{
    s_rand_seed = (unsigned long)seed;
}

// ---------------------------------------------------------------------------
// _setjmp / _longjmp
// ---------------------------------------------------------------------------

typedef int jmp_buf_arr[24];

extern int  setjmp  (jmp_buf_arr env);
extern void longjmp (jmp_buf_arr env, int val);

int _setjmp(jmp_buf_arr env)  { return setjmp(env); }
void _longjmp(jmp_buf_arr env, int val) { longjmp(env, val); }

// ---------------------------------------------------------------------------
// __assert_fail
// ---------------------------------------------------------------------------

__attribute__((noreturn))
void __assert_fail(const char *expr, const char *file,
                   unsigned int line, const char *func)
{
    (void)expr; (void)file; (void)line; (void)func;
    for (;;) ;
}

// ---------------------------------------------------------------------------
// __ctype_toupper_loc
// ---------------------------------------------------------------------------

static const unsigned short s_toupper_tbl[256] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

const unsigned short **__ctype_toupper_loc(void)
{
    static const unsigned short *p = s_toupper_tbl;
    return &p;
}

// ---------------------------------------------------------------------------
// __isoc99_sscanf — CD cue parsing; stubbed because CD is unsupported
// ---------------------------------------------------------------------------

int __isoc99_sscanf(const char *str, const char *fmt, ...)
{
    (void)str; (void)fmt;
    return 0;
}

// ---------------------------------------------------------------------------
// __memcpy_chk / __memset_chk — glibc _FORTIFY_SOURCE wrappers
// ---------------------------------------------------------------------------

void *__memcpy_chk(void *dest, const void *src, size_t len, size_t destlen)
{
    (void)destlen;
    return memcpy(dest, src, len);
}

void *__memset_chk(void *s, int c, size_t n, size_t destlen)
{
    (void)destlen;
    return memset(s, c, n);
}
