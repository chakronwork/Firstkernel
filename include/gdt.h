#ifndef GDT_H
#define GDT_H

/*
 * ============================================================
 * GDT selectors
 * ============================================================
 *
 * GDT layout:
 *
 *   0x00  null
 *   0x08  kernel code  (DPL 0)
 *   0x10  kernel data  (DPL 0)
 *   0x18  user code    (DPL 3)
 *   0x20  user data    (DPL 3)
 */
#define GDT_KERNEL_CODE 0x08U
#define GDT_KERNEL_DATA 0x10U

#define GDT_USER_CODE   0x18U
#define GDT_USER_DATA   0x20U
#define GDT_TSS         0x28U


void gdt_init(void);

#endif
