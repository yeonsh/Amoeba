/*	@(#)kbootdir.h	1.3	94/04/06 15:41:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Layout for bootable kernel directory on disk
 */

#define	KD_MAGIC	0x3017504
#define	KD_NENTRIES	21

#define	KDE_NAMELEN	16

/*
 * The 80386/ISA boot code depends on the following condition
 */
#if (4+4+(KD_NENTRIES*(4+4+KDE_NAMELEN))) == 512
struct kerneldir {
	long	kd_magic;
	struct kerneldir_ent {
		long	kde_secnum;
		long	kde_size;
		char	kde_name[KDE_NAMELEN];
	} kd_entry[KD_NENTRIES];
	long	kd_unused;
};
#endif
