/*	@(#)namelist.h	1.4	96/02/27 10:26:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __NAMELIST_H
#define __NAMELIST_H

#include <_ARGS.h>
/*
 * Format of a symbol table entry
 */
struct	nlist {
	union {
		char	*n_name;	/* for use when in-core */
		long	n_strx;		/* index into file string table */
	} n_un;
	unsigned char	n_type;		/* type flag (N_TEXT,..)  */
	char	n_other;		/* unused */
	short	n_desc;			/* see <stab.h> */
	unsigned long	n_value;	/* value of symbol (or sdb offset) */
};

#ifndef N_TYPE
/*
 * Simple values for n_type.
 */
#define	N_UNDF	0x0		/* undefined */
#define	N_ABS	0x2		/* absolute */
#define	N_TEXT	0x4		/* text */
#define	N_DATA	0x6		/* data */
#define	N_BSS	0x8		/* bss */
#define	N_COMM	0x12		/* common (internal to ld) */
#define	N_FN	0x1e		/* file name symbol */

#define	N_EXT	01		/* external bit, or'ed in */
#define	N_TYPE	0x1e		/* mask for all the type bits */
#endif /* N_TYPE */

/*
 * Dbx entries have some of the N_STAB bits set.
 * These are given in <stab.h>
 */
#define	N_STAB	0xe0		/* if any of these bits set, a dbx symbol */

/*
 * Format for namelist values.
 */
#define	N_FORMAT	"%08x"

/*
 * The following two values are Amoeba specific.
 * They are only used in checking the byteorder independent
 * internal symbol table format.
 */
#define N_AM_MAGIC	0x89abcdef	/* magic value */

#define N_AM_VERSION	1L		/* representation as of 5 feb 91 */
#define N_AM_FOREIGN	0L		/* non-standard symtab format */

#define	buf_put_symtab	_buf_put_symtab
#define	buf_get_symtab	_buf_get_symtab

int	buf_put_symtab	_ARGS((char *p, char *endbuf, struct nlist *symtab,
				long nsymtab, char *strtab, long nstrtab));
int	buf_get_symtab	_ARGS((char *p, char *endbuf, struct nlist **symtab,
				long *nsymtab, char **strtab, long *nstrtab));

#endif /* __NAMELIST_H */
