/*	@(#)my_aout.h	1.2	94/04/07 14:30:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BSD style struct exec/struct nlist definitions:
 */
struct exec {
    unsigned long   a_magic;        /* magic number */
    unsigned long   a_text;         /* size of text segment */
    unsigned long   a_data;         /* size of initialized data */
    unsigned long   a_bss;          /* size of uninitialized data */
    unsigned long   a_syms;         /* size of symbol table */
    unsigned long   a_entry;        /* entry point */
    unsigned long   a_trsize;       /* size of text relocation */
    unsigned long   a_drsize;       /* size of data relocation */
};

#define MY_TXTOFF(x)	(sizeof (struct exec))
#define MY_DATOFF(x)	(MY_TXTOFF(x) + (x).a_text)
#define MY_SYMOFF(x)	(MY_TXTOFF(x) + (x).a_text + (x).a_data + \
			 		(x).a_trsize + (x).a_drsize)
#define MY_STROFF(x)	(MY_SYMOFF(x) + (x).a_syms)

struct nlist {
    char         *n_name;
    unsigned char n_type;
    char          n_other;
    short         n_desc;
    unsigned long n_value;
};
