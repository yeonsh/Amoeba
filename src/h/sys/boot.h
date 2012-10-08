/*	@(#)boot.h	1.2	94/04/06 17:17:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BOOT_H__
#define __BOOT_H__

struct memmap {
	long	mm_phys;
	long	mm_vir;
	long	mm_size;
};

struct bootinfo {
	long	bi_entry;
	short	bi_reserved;
	short	bi_nmap;
	struct memmap bi_map[1];
};

#endif /* __BOOT_H__ */
