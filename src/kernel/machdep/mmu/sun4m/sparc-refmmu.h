/*	@(#)sparc-refmmu.h	1.1	94/04/06 09:46:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Defines for Sparc reference MMU, as found on sun4m machines
 */

/* PTE entry defines */
/*
 * ------------------------------------------------------
 * | Rsvd |       PPN           | C | M | R | ACC | ET  |
 * ------------------------------------------------------
 * 31   27 26                 08 07  06  05  04 02 01  00
 */

#define PTE_PPN_SHIFT	8
#define PTE_PPN_MASK	0x07FFFF00
#define PTE_C		0x00000080
#define PTE_M		0x00000040
#define PTE_R		0x00000020
#define PTE_ACC_SHIFT	2
#define PTE_ACC_MASK	0x0000001C
#define PTE_ACC_R__R__	0x00000000
#define PTE_ACC_RW_RW_	0x00000004
#define PTE_ACC_R_XR_X	0x00000008
#define PTE_ACC_RWXRWX	0x0000000C
#define PTE_ACC___X__X	0x00000010
#define PTE_ACC_R__RW_	0x00000014
#define PTE_ACC____R_X	0x00000018
#define PTE_ACC____RWX	0x0000001C
#define PTE_ET_MASK	0x00000003
#define PTE_ET_INVALID	0x00000000
#define PTE_ET_PTP	0x00000001
#define PTE_ET_PTE	0x00000002

/* Diagnostic tag access format */

#define TAG_VA_MASK	0xFFFFF000
#define TAG_CTX_MASK	0x00000FC0
#define TAG_CTX_SHIFT	6
#define TAG_V		0x00000020
#define TAG_LEVEL_MASK	0x00000018
#define TAG_LEVEL_SHIFT	3
#define TAG_S		0x00000004
#define TAG_IO		0x00000002
#define TAG_PTP		0x00000001

#define MMU_LEVEL1_SHIFT	12
#define MMU_LEVEL2_SHIFT	 6
#define MMU_LEVEL3_SHIFT	 0
