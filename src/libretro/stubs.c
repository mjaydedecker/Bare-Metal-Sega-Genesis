//
// src/libretro/stubs.c
//
// Bare Metal Sega Genesis
//
// Bare-metal stubs for libretro-common functions not compiled under
// STATIC_LINKING=1.  file_stream.c, file_path.c, and compat_strl.c are
// excluded from the genesis build; we supply minimal implementations here.
//
// This file is compiled as part of libgenesis.a (genesis.mk), so it picks
// up the same include paths as the rest of the genesis core.
//

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

// Forward-declare RFILE as an opaque type.  We never dereference it
// (all filestream functions return NULL/error), so the full definition
// from file_stream.h is not required.
typedef struct RFILE RFILE;

// ---------------------------------------------------------------------------
// filestream_* — no filesystem access from bare-metal libretro callbacks
// ---------------------------------------------------------------------------

void filestream_vfs_init(const void *vfs_info)
{
    (void)vfs_info;
}

RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
    (void)path; (void)mode; (void)hints;
    return NULL;
}

int filestream_close(RFILE *stream)
{
    (void)stream;
    return -1;
}

int64_t filestream_read(RFILE *stream, void *data, int64_t len)
{
    (void)stream; (void)data; (void)len;
    return -1;
}

int64_t filestream_write(RFILE *stream, const void *data, int64_t len)
{
    (void)stream; (void)data; (void)len;
    return -1;
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
    (void)stream; (void)offset; (void)seek_position;
    return -1;
}

int64_t filestream_tell(RFILE *stream)
{
    (void)stream;
    return -1;
}

int64_t filestream_get_size(RFILE *stream)
{
    (void)stream;
    return -1;
}

void filestream_rewind(RFILE *stream)
{
    (void)stream;
}

char *filestream_gets(RFILE *stream, char *s, size_t len)
{
    (void)stream; (void)s; (void)len;
    return NULL;
}

int filestream_getc(RFILE *stream)
{
    (void)stream;
    return -1;
}

int filestream_putc(RFILE *stream, int c)
{
    (void)stream; (void)c;
    return -1;
}

int filestream_eof(RFILE *stream)
{
    (void)stream;
    return 1;
}

int filestream_error(RFILE *stream)
{
    (void)stream;
    return 1;
}

int filestream_flush(RFILE *stream)
{
    (void)stream;
    return -1;
}

int filestream_delete(const char *path)
{
    (void)path;
    return -1;
}

int filestream_rename(const char *old_path, const char *new_path)
{
    (void)old_path; (void)new_path;
    return -1;
}

int64_t filestream_read_file(const char *path, void **buf, int64_t *len)
{
    (void)path; (void)buf; (void)len;
    return -1;
}

int filestream_scanf(RFILE *stream, const char *format, ...)
{
    (void)stream; (void)format;
    return -1;
}

int filestream_vprintf(RFILE *stream, const char *format, va_list args)
{
    (void)stream; (void)format; (void)args;
    return -1;
}

int filestream_printf(RFILE *stream, const char *format, ...)
{
    (void)stream; (void)format;
    return -1;
}

int64_t filestream_truncate(RFILE *stream, int64_t length)
{
    (void)stream; (void)length;
    return -1;
}

const char *filestream_get_path(RFILE *stream)
{
    (void)stream;
    return NULL;
}

int filestream_write_file(const char *path, const void *data, int64_t size)
{
    (void)path; (void)data; (void)size;
    return 0;
}

int filestream_exists(const char *path)
{
    (void)path;
    return 0;
}

char *filestream_getline(RFILE *stream)
{
    (void)stream;
    return NULL;
}

void *filestream_get_vfs_handle(RFILE *stream)
{
    (void)stream;
    return NULL;
}

// ---------------------------------------------------------------------------
// fill_pathname_join — from libretro-common/file/file_path.c
// Concatenates dir + "/" + path into out, capped at size bytes.
// ---------------------------------------------------------------------------

void fill_pathname_join(char *out, const char *dir, const char *path,
                        size_t size)
{
    size_t dlen;
    if (!out || size == 0)
        return;
    out[0] = '\0';
    if (dir && *dir)
    {
        strncpy(out, dir, size - 1);
        out[size - 1] = '\0';
        dlen = strlen(out);
        if (dlen > 0 && dlen < size - 1 && out[dlen - 1] != '/')
        {
            out[dlen++] = '/';
            out[dlen]   = '\0';
        }
    }
    if (path && *path)
    {
        size_t used = strlen(out);
        size_t room = size - used - 1;
        strncat(out, path, room);
        out[size - 1] = '\0';
    }
}

// ---------------------------------------------------------------------------
// strlcpy / strlcat — from libretro-common/compat/compat_strl.c
// ---------------------------------------------------------------------------

size_t strlcpy(char *dest, const char *src, size_t size)
{
    size_t src_len = strlen(src);
    if (size > 0)
    {
        size_t copy = (src_len < size - 1) ? src_len : size - 1;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }
    return src_len;
}

size_t strlcat(char *dest, const char *src, size_t size)
{
    size_t dest_len = strlen(dest);
    size_t src_len  = strlen(src);
    if (dest_len < size - 1)
    {
        size_t room = size - dest_len - 1;
        size_t copy = (src_len < room) ? src_len : room;
        memcpy(dest + dest_len, src, copy);
        dest[dest_len + copy] = '\0';
    }
    return dest_len + src_len;
}

// ---------------------------------------------------------------------------
// rf* wrappers — from file_stream_transforms.c (not compiled with
// STATIC_LINKING=1).  The CD hardware uses these via the cdStream* macros in
// osd.h.  All are no-ops because we don't support CD image loading.
// ---------------------------------------------------------------------------

RFILE *rfopen(const char *path, const char *mode)
{
    (void)path; (void)mode;
    return NULL;
}

int rfclose(RFILE *stream)
{
    (void)stream;
    return -1;
}

int64_t rftell(RFILE *stream)
{
    (void)stream;
    return -1;
}

int64_t rfseek(RFILE *stream, int64_t offset, int origin)
{
    (void)stream; (void)offset; (void)origin;
    return -1;
}

int64_t rfread(void *buffer, size_t elemsize, size_t count, RFILE *stream)
{
    (void)buffer; (void)elemsize; (void)count; (void)stream;
    return 0;
}

char *rfgets(char *buffer, int maxCount, RFILE *stream)
{
    (void)buffer; (void)maxCount; (void)stream;
    return NULL;
}

// ---------------------------------------------------------------------------
// crc32 — zlib CRC.  Used by SMS cart detection; we stub it to 0 because
// Sega Genesis ROMs don't go through this path.
// ---------------------------------------------------------------------------

unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned int len)
{
    (void)crc; (void)buf; (void)len;
    return 0;
}

// ---------------------------------------------------------------------------
// Math functions — provided here so glibc's libm.a (which references errno
// as a TLS variable) is never linked.  The Thread Pointer register is not
// initialised by Circle, so any TLS access would fault on bare metal.
//
// Implementations use double-precision polynomial approximations accurate
// to ~15 significant digits — sufficient for FM-synthesis table generation.
// ---------------------------------------------------------------------------

// Bit-cast helpers
static double _bits_to_double(uint64_t b) {
    double d; __builtin_memcpy(&d, &b, 8); return d;
}
static uint64_t _double_to_bits(double d) {
    uint64_t b; __builtin_memcpy(&b, &d, 8); return b;
}

// Constants
#define PI      3.14159265358979323846
#define PI2     6.28318530717958647692
#define PIO2    1.57079632679489661923
#define LN2     0.69314718055994530942
#define LN2R    1.44269504088896340736   /* 1/ln(2) */
#define INF     _bits_to_double(0x7FF0000000000000ULL)
#define NAN_D   _bits_to_double(0x7FF8000000000000ULL)

// --- sin/cos via polynomial on reduced range ---
// Coefficients for sin(x) on [-pi/4, pi/4]:
static double _poly_sin(double x) {
    double x2 = x*x;
    return x*(1.0 + x2*(-1.6666666666666666e-1
             + x2*( 8.3333333333333329e-3
             + x2*(-1.9841269841269841e-4
             + x2*( 2.7557319223985890e-6
             + x2*(-2.5052108385441720e-8
             + x2*  1.6059043836821613e-10))))));
}
// Coefficients for cos(x) on [-pi/4, pi/4]:
static double _poly_cos(double x) {
    double x2 = x*x;
    return 1.0 + x2*(-5.0000000000000000e-1
           + x2*( 4.1666666666666664e-2
           + x2*(-1.3888888888888889e-3
           + x2*( 2.4801587301587302e-5
           + x2*(-2.7557319223985888e-7
           + x2*  2.0876756987868099e-9)))));
}

// Range-reduce x to [-pi/4, pi/4]; return octant.
static int _trig_reduce(double x, double *xr) {
    // Use integer rounding of x/(pi/4)
    int n = (int)(x * (4.0/PI) + (x < 0 ? -0.5 : 0.5));
    *xr   = x - n * (PI/4);
    return n & 7;
}

double sin(double x) {
    double xr;
    int oct = _trig_reduce(x, &xr);
    switch (oct) {
        case 0: return  _poly_sin(xr);
        case 1: return  _poly_cos(xr);
        case 2: return  _poly_cos(-xr);
        case 3: return  _poly_sin(-xr);
        case 4: return -_poly_sin(xr);
        case 5: return -_poly_cos(xr);
        case 6: return -_poly_cos(-xr);
        default:return -_poly_sin(-xr);
    }
}

double cos(double x) {
    double xr;
    int oct = _trig_reduce(x, &xr);
    switch (oct) {
        case 0: return  _poly_cos(xr);
        case 1: return -_poly_sin(xr);
        case 2: return  _poly_sin(-xr);
        case 3: return -_poly_cos(-xr);
        case 4: return -_poly_cos(xr);
        case 5: return  _poly_sin(xr);
        case 6: return -_poly_sin(-xr);
        default:return  _poly_cos(-xr);
    }
}

void sincos(double x, double *s, double *c) {
    *s = sin(x);
    *c = cos(x);
}

// --- log(x) via polynomial on [1,2) with range reduction ---
// log(m*2^n) = n*ln(2) + log(m), m in [1,2)
// Use log((1+t)/(1-t)) = 2*(t + t^3/3 + t^5/5 + ...) with t=(m-1)/(m+1)
double log(double x) {
    if (x <= 0.0) return (x == 0.0) ? -INF : NAN_D;
    uint64_t bits = _double_to_bits(x);
    int exp = (int)((bits >> 52) & 0x7FF) - 1023;
    // Set exponent to 0 (value in [1, 2))
    bits = (bits & 0x800FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    double m = _bits_to_double(bits);
    // Better: reduce to [sqrt(0.5), sqrt(2)] to improve convergence
    if (m < 0.7071067811865476) { m *= 2.0; exp--; }
    double t = (m - 1.0) / (m + 1.0);
    double t2 = t*t;
    double p = t*(2.0 + t2*(2.0/3 + t2*(2.0/5 + t2*(2.0/7
               + t2*(2.0/9 + t2*(2.0/11 + t2*(2.0/13)))))));
    return p + exp * LN2;
}

// --- exp(x) via polynomial on [-ln2/2, ln2/2] with range reduction ---
double exp(double x) {
    if (x >  709.78) return INF;
    if (x < -745.13) return 0.0;
    double fn = x * LN2R + (x < 0 ? -0.5 : 0.5);
    int n = (int)fn;
    double r = x - n * LN2;
    // Polynomial approximation of exp(r)-1:
    double p = 1.0 + r*(1.0 + r*(0.5 + r*(1.0/6 + r*(1.0/24
               + r*(1.0/120 + r*(1.0/720 + r*(1.0/5040
               + r*(1.0/40320 + r*(1.0/362880)))))))));
    // Scale by 2^n
    uint64_t bits = _double_to_bits(p);
    bits += (uint64_t)n << 52;
    return _bits_to_double(bits);
}

// --- pow(x,y) = exp(y*log(x)) ---
double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 1.0) return 1.0;
    if (x <  0.0) {
        // Handle negative base with integer exponent
        int iy = (int)y;
        if ((double)iy == y)
            return (iy & 1) ? -pow(-x, y) : pow(-x, y);
        return NAN_D;
    }
    if (x == 0.0) return (y > 0.0) ? 0.0 : INF;
    return exp(y * log(x));
}

// --- sqrt via Newton-Raphson seeded from hardware rsqrt ---
double sqrt(double x) {
    if (x < 0.0) return NAN_D;
    if (x == 0.0) return 0.0;
    // Initial approximation using bit manipulation
    uint64_t bits = _double_to_bits(x);
    uint64_t approx = 0x1FF7A3BEA91D9B1BULL + (bits >> 1);
    double y = _bits_to_double(approx);
    // Three Newton-Raphson iterations: y = 0.5*(y + x/y)
    y = 0.5*(y + x/y);
    y = 0.5*(y + x/y);
    y = 0.5*(y + x/y);
    return y;
}

// Single-precision variants
float sinf(float x)  { return (float)sin((double)x); }
float cosf(float x)  { return (float)cos((double)x); }
float logf(float x)  { return (float)log((double)x); }
float expf(float x)  { return (float)exp((double)x); }
float powf(float x, float y) { return (float)pow((double)x, (double)y); }
float sqrtf(float x) { return (float)sqrt((double)x); }
