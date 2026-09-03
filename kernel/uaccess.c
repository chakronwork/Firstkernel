#include <stdint.h>
#include <stddef.h>
#include "uaccess.h"

int uaccess_verify_read(const void *ptr, size_t size)
{
    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end = start + size;

    if (size == 0)
        return 1;

    if (end < start)
        return 0;

    if (start < USER_SPACE_START || end > USER_SPACE_END)
        return 0;

    return 1;
}

int uaccess_verify_write(void *ptr, size_t size)
{
    return uaccess_verify_read((const void *)ptr, size);
}

int copy_from_user(void *dst, const void *src, size_t size)
{
    if (!uaccess_verify_read(src, size))
        return 0;

    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < size; ++i)
        d[i] = s[i];

    return 1;
}

int copy_to_user(void *dst, const void *src, size_t size)
{
    if (!uaccess_verify_write(dst, size))
        return 0;

    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < size; ++i)
        d[i] = s[i];

    return 1;
}
