#include <stdint.h>
#include "initrd.h"

static uint32_t initrd_start_addr = 0;
static uint32_t initrd_end_addr = 0;

void initrd_init(uint32_t start, uint32_t end)
{
    initrd_start_addr = start;
    initrd_end_addr = end;
}

uint32_t initrd_get_start(void)
{
    return initrd_start_addr;
}

uint32_t initrd_get_size(void)
{
    if (initrd_end_addr > initrd_start_addr) {
        return initrd_end_addr - initrd_start_addr;
    }
    return 0;
}
