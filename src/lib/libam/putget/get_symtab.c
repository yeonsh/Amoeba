/*	@(#)get_symtab.c	1.3	94/04/07 10:17:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include <stdlib.h>
#include <string.h>
#include "namelist.h"
#include "module/buffers.h"

#ifndef __STDC__
#define NULL 0
#endif

/*
 * buf_get_symtab(): get a symbol table and a string table from a buffer
 * containing an Amoeba symbol table, which has the following format:
 *
 * long			: magic value, for checking purposes
 * long			: version, ditto
 * long  		: size of symbol table following it
 * struct nlist [] 	: symbol table
 * long			: size of string table following it
 * char []		: string table
 *
 * The buffers for the symbol and string table are allocated by this
 * routine, as the caller doesn't know how big they should be.
 *
 * The differences between a Unix symbol table and a symbol table
 * present in Amoeba binaries are the following:
 * 
 * - a long magic & version prefix
 * - the Amoeba symbol table has an integer prefix.
 *   This is needed because there is no ``a_syms'' field in an Amoeba
 *   process descriptor.
 * - all integers are in network byte order.
 *   This has the advantage that programs like `nm' can easily work on
 *   binaries of different endianness.
 * - The integer telling how big the string table is does not include the
 *   4 bytes of itself.
 *   This was only done to make things more consistent.
 */

int		/* succes == 0, failure == 1 */
buf_get_symtab(buf, end, symtab, nsymtab, strtab, nstrtab)
char 		*buf;		/* in:	head of buffer to get it from */
char 		*end;		/* in:	tail of buffer to get it from */
struct nlist   **symtab;	/* out: symbol table */
long 	        *nsymtab;	/* out: length of symtab in bytes */
char	       **strtab;	/* out: string table (incl. long in front) */
long		*nstrtab;	/* out: full length of string table */
{
    register char *p = buf;	/* always the current position in buf */
    struct nlist *sym_table = NULL;
    char *string_table = NULL;
    int32 sym_bytes, num_strtab;
    int32 check;

    /* if something goes wrong call Failure(), so that everything
     * gets cleaned up properly under all circumstances.
     */
#   define Failure()	{ goto failure; }

    /* check magic & version field */
    if ((p = buf_get_int32(p, end, &check)) == NULL || check != N_AM_MAGIC) {
	Failure();
    }
    if ((p = buf_get_int32(p, end, &check)) == NULL) {
	Failure();
    }
    if (check != N_AM_VERSION && check != N_AM_FOREIGN) {
	/* It is neither a symbol table of the most recent format,
	 * nor a foreign symbol table (which is put in the string table)
	 */
	Failure();
    }

    /* look how big the symbol table is */
    if ((p = buf_get_int32(p, end, &sym_bytes)) == NULL) {
	Failure();
    }
    if ((sym_bytes % sizeof(struct nlist)) != 0) {	/* sanity check */
	Failure();
    }

    if (sym_bytes == 0) { /* this is the case for foreign symbol tables */
	sym_table = NULL;
    } else {
	register long n;
	register struct nlist *sym;
	long nsyms = sym_bytes / sizeof(struct nlist);

	/* allocate symbol table */
	if ((sym_table = (struct nlist *)malloc(sym_bytes)) == NULL) {
	    Failure();
	}

	/* avoid checking in each iteration, do it now for the entire symtab */
	if (p == NULL || (end - p) < sym_bytes) {
	    Failure();
	}

	/* convert symbol table to our endianness */
	sym = sym_table;
	for (n = 0; n < nsyms; n++) {
	    int32 int32_val;
	    int16 int16_val;
	    int8  int8_val;

	    p = buf_get_int32(p, end, &int32_val);
	    sym->n_un.n_strx = int32_val;
	    p = buf_get_int8 (p, end, &int8_val);
	    sym->n_type = int8_val;
	    p = buf_get_int8 (p, end, &int8_val);
	    sym->n_other = int8_val;
	    p = buf_get_int16(p, end, &int16_val);
	    sym->n_desc = int16_val;
	    p = buf_get_int32(p, end, &int32_val);
	    sym->n_value = int32_val;

	    sym++;
	}
    }

    /* get integer telling how big the string table is */
    if ((p = buf_get_int32(p, end, &num_strtab)) == NULL) {
	Failure();
    }

    /* check that string table is indeed fully present in buffer: */
    if (num_strtab < 0 || p == NULL || (end - p) < num_strtab) {
	Failure();
    }

    if ((string_table = (char *) malloc(sizeof(long) + num_strtab)) == NULL) {
	Failure();
    }

    /* Unix type string table is a long byte count + sequence of strings */
    *(long *)string_table = num_strtab + sizeof(long);
    (void) memcpy(string_table + sizeof(long), p, (unsigned int)num_strtab);

    /* no problems encountered; set the output values */
    *symtab = sym_table;
    *nsymtab = sym_bytes;
    *strtab = string_table;
    *nstrtab = num_strtab + sizeof(long);	/* see explanation above */
    return(0);

failure:
    if (sym_table != NULL) free(sym_table);
    if (string_table != NULL) free(string_table);
    return(1);
}

