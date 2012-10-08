/*	@(#)aout386info.c	1.3	94/04/07 14:29:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NO_386_AOUT

#include <stdlib.h>
#include "acv.h"
#include "byteorder.h"
#include "my_aout.h"

#define SEGSIZE		0x1000		/* from a header? */
#define SEGALIGN(addr)	(SEGSIZE * (((addr) + SEGSIZE - 1) / SEGSIZE))

#ifdef KERNEL
#define MY_TXTADDR(x)	0x2000		/* from a header? */
#else
#define MY_TXTADDR(x)	0x20000000	/* from a header? */
#endif

extern long lseek();

/* user-level programs and kernels should both be linked with -n: */
#define MAGIC_n		0x108

int
isaout386(fd)
int fd;
{
    struct exec aout_head;

    /* read and check a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	return 0;
    }
    if (read(fd, (char *)&aout_head, sizeof(aout_head)) != sizeof(aout_head)) {
	return 0;
    }

    dec_l_le(&aout_head.a_magic);

#ifdef KERNEL
    if (aout_head.a_magic != MAGIC_n) {
	/* kernel compiled with wrong flags? */
	printf("unrecognised a_magic: 0x%x\n", aout_head.a_magic);
    }
#endif

    return (aout_head.a_magic == MAGIC_n);
}

char *
aout386info(fd, aip)
int fd;
struct aoutinfo *aip;
{
    struct exec aout_head;
    long magic;

    if (!isaout386(fd)) {
	return "not a 386 a.out binary";
    }

    /* read and convert the a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	return "lseek failed";
    }
    if (read(fd, (char *)&aout_head, sizeof(aout_head)) != sizeof(aout_head)) {
	return "failed to read a.out header";
    }

    dec_l_le(&aout_head.a_text);
    dec_l_le(&aout_head.a_data);
    dec_l_le(&aout_head.a_bss);
    dec_l_le(&aout_head.a_syms);
    dec_l_le(&aout_head.a_entry);
    dec_l_le(&aout_head.a_trsize);
    dec_l_le(&aout_head.a_drsize);

#ifdef DEBUG
    printf("a_text:     %9ld\n", aout_head.a_text);
    printf("a_data:     %9ld\n", aout_head.a_data);
    printf("a_bss:      %9ld\n", aout_head.a_bss);
    printf("a_entry: hex%9lx\n", aout_head.a_entry);
    printf("a_syms:     %9ld\n", aout_head.a_syms);
#endif

    aip->ai_architecture = "i80386";

    aip->ai_txt_offset = MY_TXTOFF(aout_head);
    aip->ai_txt_virtual = MY_TXTADDR(aout_head);
    aip->ai_txt_size = aout_head.a_text;

    aip->ai_dat_offset = MY_DATOFF(aout_head);
    aip->ai_dat_virtual = SEGALIGN(aip->ai_txt_virtual + aip->ai_txt_size);
    aip->ai_dat_size = aout_head.a_data;

    aip->ai_bss_virtual = SEGALIGN(aip->ai_dat_virtual + aip->ai_dat_size);
    aip->ai_bss_size = aout_head.a_bss;

    /*
     * read and convert the symbol table
     */

    aip->ai_sym_size = aout_head.a_syms;

    if (aip->ai_sym_size == 0) {
	aip->ai_sym_tab = 0;
    } else {
	int i, nsyms;

	if ((aip->ai_sym_tab = malloc(aip->ai_sym_size)) == 0) {
	    return "couldn't allocate symbol table";
	}
	if (lseek(fd, (long)MY_SYMOFF(aout_head), 0) != MY_SYMOFF(aout_head)) {
	    return "seek to symbol table failed";
	}
	if (read(fd, aip->ai_sym_tab, aip->ai_sym_size) != aip->ai_sym_size) {
	    return "read of symbol table failed";
	}

	nsyms = aip->ai_sym_size / sizeof(struct nlist);
	for (i = 0; i < nsyms; i++) {
	    struct nlist *sym;

	    sym = ((struct nlist *) aip->ai_sym_tab) + i; 
	    dec_l_le((long *) &sym->n_name);
	    dec_s_le(&sym->n_desc);
	    dec_l_le(&sym->n_value);

#ifdef SYM_DEBUG
	    printf("entry %d: n_name = %ld; n_desc = %d; n_value = x%lx\n",
	            i, (long) sym->n_name, sym->n_desc, sym->n_value);
#endif
	}
    }

    /*
     * read the string table
     */
    if (aip->ai_sym_size == 0) {
	aip->ai_str_size = 0; /* empty symbol table => empty string table */
    } else {
	/* first read how big it is */
	long str_off = MY_STROFF(aout_head);

	if (lseek(fd, str_off, 0) != str_off) {
	    return "seek to string table failed";
	}
	if (read(fd, (char *)&aip->ai_str_size, sizeof(long))!= sizeof(long)) {
	    return "couldn't read string table length";
	}

	dec_l_le(&aip->ai_str_size);
#ifdef DEBUG
	printf("string table size: %ld\n", aip->ai_str_size);
#endif
    }

    if (aip->ai_str_size == 0) {
	aip->ai_str_tab = 0;	/* no string table */
    } else {
	if ((aip->ai_str_tab = malloc(aip->ai_str_size)) == 0) {
	    return "couldn't allocate string table";
	}
	/* first 4 bytes a full string table is the byte count */
	*(long *)aip->ai_str_tab = aip->ai_str_size;

	/* read the string table itself */
	if (read(fd, aip->ai_str_tab + sizeof(long),
		 aip->ai_str_size - sizeof(long))
	      != aip->ai_str_size - sizeof(long)) {
	    return "couldn't read string table";
	}
    }

    /*
     * and finally set the entry point
     */
    aip->ai_entrypoint = aout_head.a_entry;

    /* return success by means of a null pointer */
    return 0;
}

#endif /* NO_386_AOUT */
