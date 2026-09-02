#ifndef UACCESS_H
#define UACCESS_H

#include <stdint.h>

int user_range_valid(
    uint32_t virtual_address,
    uint32_t length
);

int copy_from_user(
    void *destination,
    uint32_t user_address,
    uint32_t length
);

#endif /* UACCESS_H */
