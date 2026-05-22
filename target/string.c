#include <stddef.h>
#include <stdint.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;

    for (size_t i = 0; i < n; ++i)
        d[i] = (uint8_t)c;
    return dst;
}
