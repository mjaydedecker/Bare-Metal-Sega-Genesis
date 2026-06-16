//
// src/stdlib_stubs.cpp
//
// Bare Metal Sega Genesis
// vsprintf — implemented using Circle's CString::FormatV.
// Needs to be in a .cpp file because CString is a C++ class.
//

#include <circle/string.h>
#include <circle/types.h>
#include <string.h>
#include <stdarg.h>

extern "C" int vsprintf(char *buf, const char *fmt, va_list ap)
{
    CString s;
    s.FormatV(fmt, ap);
    const char *result = static_cast<const char *>(s);
    size_t len = strlen(result);
    memcpy(buf, result, len + 1);
    return (int)len;
}

extern "C" int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

extern "C" int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    CString s;
    va_list ap;
    va_start(ap, fmt);
    s.FormatV(fmt, ap);
    va_end(ap);
    const char *result = static_cast<const char *>(s);
    size_t len = strlen(result);
    if (size > 0)
    {
        size_t copy = (len < size - 1) ? len : size - 1;
        memcpy(buf, result, copy);
        buf[copy] = '\0';
    }
    return (int)len;
}

extern "C" int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    CString s;
    s.FormatV(fmt, ap);
    const char *result = static_cast<const char *>(s);
    size_t len = strlen(result);
    if (size > 0)
    {
        size_t copy = (len < size - 1) ? len : size - 1;
        memcpy(buf, result, copy);
        buf[copy] = '\0';
    }
    return (int)len;
}
