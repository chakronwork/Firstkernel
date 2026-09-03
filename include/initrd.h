#ifndef FIRSTOS_INITRD_H
#define FIRSTOS_INITRD_H

#include <stdint.h>

void initrd_init(uint32_t start, uint32_t end);
uint32_t initrd_get_start(void);
uint32_t initrd_get_size(void);

#endif
