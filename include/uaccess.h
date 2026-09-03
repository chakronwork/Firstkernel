#ifndef FIRSTOS_UACCESS_H
#define FIRSTOS_UACCESS_H

#include <stdint.h>
#include <stddef.h>

#define USER_SPACE_START 0x40000000U
#define USER_SPACE_END   0x80000000U

int uaccess_verify_read(const void *ptr, size_t size);
int uaccess_verify_write(void *ptr, size_t size);
int copy_from_user(void *dst, const void *src, size_t size);
int copy_to_user(void *dst, const void *src, size_t size);

#endif
