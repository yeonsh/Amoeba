/*	@(#)i80386.h	1.6	94/04/06 15:58:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __I80386_H__
#define __I80386_H__
/*
 * i80386.h
 *
 * Becareful with expressions in defines since some of them are also
 * used in assembler files !
 */

/*
 * CPU types
 */
#define	CPU_NONE	0	/* misnomer, actually unknown */
#define	CPU_386		386	/* i386-DX or i386-SX CPU */
#define	CPU_486		486	/* i486-DX or i486-SX CPU */

/*
 * Every ordinary segment descriptor can access 4 Gb of memory
 * (at 4K granularity).
 */
#define SEG_SIZE	0xFFFFF

#define DESC_SIZE	8	/* size of segment descriptor */

/*
 * Segment descriptors access/type and granularity bytes
 */
#define UDATA_ACC	0xF2	/* present, dpl = 3, writeable */
#define KDATA_ACC	0x92	/* present, dpl = 0, writeable */
#define UCODE_ACC	0xFA	/* present, dpl = 3, readable */
#define KCODE_ACC	0x9A	/* present, dpl = 0, readable */
#define DATA_GRAN	0xC0	/* 4K granularity, 32 bit segment */
#define CODE_GRAN	0xC0	/* 4K granularity, 32 bit segment */
#define C286_GRAN	0x80	/* 4K granularity, 16 bit segment */
#define TSS_ACC		0x89	/* present, dpl = 0, available, accessed */
#define TSS_GRAN	0x00	/* byte granularity */
#define KINT_ACC	0x8E	/* present, dpl = 0, intr gate */
#define KTRP_ACC	0x8F	/* present, dpl = 0, trap gate */
#define UINT_ACC	0xEE	/* present, dpl = 3, intr gate */
#define	UTRP_ACC	0xEF	/* present, dpl = 3, trap gate */

/*
 * Boot segment descriptors. These are used to build a temporary
 * GDT during the reboot phase. Since all memory in the old kernel
 * is over written by the new kernel this GDT is stored at a "save"
 * location somewhere in low memory.
 */
#define BOOT_GDTPTR	0x1000
#define BOOT_GDTBASE	0x1010

#define BOOT_GDT_SIZE		3

#define BOOT_NULL_SELECTOR	0x00
#define BOOT_DS_SELECTOR	0x08
#define BOOT_CS_SELECTOR	0x10

/*
 * Temporary segment descriptors. These form a temporary GDT that
 * is used to warp the CPU from 16-bit mode into 32-bit mode. The
 * layout of the GDT conforms to what BIOS expects so that the it
 * can be used to do the switching. For non ISA/MCA machines the
 * same layout can be used.
 */
#define	TEMP_GDT_SIZE		8

#define	TEMP_NULL_SELECTOR	0x00
#define	TEMP_GDT_SELECTOR	0x08
#define	TEMP_IDT_SELECTOR	0x10
#define	TEMP_DS_SELECTOR	0x18
#define	TEMP_ES_SELECTOR	0x20
#define	TEMP_SS_SELECTOR	0x28
#define	TEMP_CS_SELECTOR	0x30
#define	TEMP_BIOS_SELECTOR	0x38

/*
 * Kernel global segment descriptors. U_DS_SEL and U_CS_SEL are used to
 * access the global descriptor table and U_DS_SELECTOR and U_CS_SELECTOR
 * are the real selectors (rpl = 3).
 */
#define GDT_SIZE		6

#define	NULL_SELECTOR		0x00
#define K_DS_SELECTOR		0x08
#define K_CS_SELECTOR		0x10
#define TSS_SELECTOR		0x18
#define U_DS_SEL		0x20
#define U_DS_SELECTOR		0x23	/* U_DS_SEL + 3 */
#define U_CS_SEL		0x28
#define U_CS_SELECTOR		0x2B	/* U_DS_SEL + 3 */

/*
 * Size of interrupt descriptor table
 */
#define IDT_SIZE		256

/*
 * Size of task state segment
 */
#define	MAX_TSS_IOADDR		0x3FF	/* maximum I/O addresses */
#define	SIZEOF_TSS		104	/* sizeof(struct tss_s) */
#define TSS_SIZE		104+128	/* SIZEOF_TSS+((MAX_TSS_IOADDR+7)/8) */

/*
 * Eflag bits
 */
#define	EFL_CF			0x00000001	/* carry flag */
#define	EFL_PF			0x00000004	/* parity flag */
#define	EFL_AF			0x00000010	/* BCD parity flag */
#define	EFL_ZF			0x00000040	/* zero flag */
#define	EFL_NF			0x00000080	/* negative flag */
#define	EFL_TF			0x00000100	/* trap flag */
#define	EFL_IF			0x00000200	/* interrupt enable flag */
#define	EFL_DF			0x00000400	/* direction flag */
#define	EFL_OF			0x00000800	/* overflow flag */
#define	EFL_IOPL		0x00003000	/* IOPL mask */
#define	EFL_NT			0x00004000	/* nested task bit */
#define	EFL_RF			0x00010000	/* resume flag */
#define	EFL_VM			0x00020000	/* virtual 8086 mode */
#define	EFL_AC			0x00040000	/* alignment check */

/*
 * CR0 bits
 */
#define CR0_PROT		0x00000001	/* protection enable */
#define	CR0_MP			0x00000002	/* math present */
#define	CR0_EM			0x00000004	/* emulation enabled */
#define	CR0_TS			0x00000008	/* task switched */
#define	CR0_ET			0x00000010	/* extension type */
#define CR0_NE			0x00000020	/* NDP error enable */
#define CR0_AM			0x00040000	/* alignment check enable */
#define CR0_PG			0x80000000	/* paging enabled */

/*
 * Shift values
 */
#define OFFSET_HIGH_SHIFT	16	/* (gate) offset -> offset_high */
#define BASE_HIGH_SHIFT		24	/* base -> base_high */
#define BASE_MID_SHIFT		16	/* base -> base_middle */
#define LIMIT_HIGH_SHIFT	16	/* limit -> gran */

/*
 * Paging constants
 */
#define PAGE_SIZE 		4096	/* size of physical page */
#define PAGE_TABLE_ENTRIES 	1024	/* # of page table entries */
#define PAGE_TABLE_SIZE		4096	/* page table size */
#define PAGE_DIR_SIZE		4096	/* page directory size */
#define MAP_CLICKS		1024	/* clicks per map */

/*
 * Page table entry mode bits
 */
#define PG_P			0x00000001
#define PTE_P			0x00000001
#define PTE_RW			0x00000002
#define PTE_US			0x00000004
#define PTE_K			0x00000000
#define PTE_UR			PTE_US
#define PTE_UW			PTE_US | PTE_RW
#define PTE_0			0x00000000
#define PDINDXSHFT		22

/*
 * Macros used in tables.c to initialize misc. types of gates
 */
#define MKINTG(rtn) \
	{ (uint32)rtn,(uint16)K_CS_SELECTOR,(uint8) 0,(uint8)(KINT_ACC) }
#define MKKTRPG(rtn) \
	{ (uint32)rtn,(uint16)K_CS_SELECTOR,(uint8) 0,(uint8)(KTRP_ACC) }
#define MKUTRPG(rtn) \
	{ (uint32)rtn,(uint16)K_CS_SELECTOR,(uint8) 0,(uint8)(UTRP_ACC) }
#define MKUCALLG(rtn) \
	{ (unsigned long)rtn, (((long)K_CS_SELECTOR)<<16)|((char)(UINT_ACC)) }
#define MKDSCR(base, limit, acc1, acc2) \
	{ (unsigned long)base, ((long)limit|(acc2<<20)|(acc1<<24)) }

#endif /* __I80386_H__ */
