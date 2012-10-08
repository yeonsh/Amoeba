/*	@(#)put_symtab.c	1.3	94/04/07 10:18:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "module/buffers.h"
#include "namelist.h"

/*
 * buf_put_symtab() puts a symbol table + string table in an
 * buffer to form an Amoeba symbol table.
 * See buf_get_symtab() for a description of its structure.
 */

int		/* succes == 0, failure == 1 */
buf_put_symtab(buf, end, symtab, nsymtab, strtab, nstrtab)
char 		*buf;		/* out:	head of buffer to put it in	   */
char 		*end;		/* out:	tail of buffer to put it in	   */
struct nlist    *symtab;	/* in:  symbol table			   */
long 	         nsymtab;	/* in:  length of symtab		   */
char	        *strtab;	/* in:  string table (incl. long in front) */
long		 nstrtab;	/* in:  length of full string table	   */
{
    register char *p = buf;	/* current position in buf */

    /* First check that it will all fit.
     * nstrtab also includes the four bytes for the long in front.
     */
    if ((3 * sizeof(long) + nsymtab + nstrtab) > end - buf) {
	return(1);
    }
    
    /* perform some further sanity checks before doing anything real */
    if (nsymtab < 0 || nstrtab < sizeof(long)
		    || (nsymtab % sizeof(struct nlist)) != 0) {
	return(1);
    }
	
    /* magic & version field for consistency checks */
    p = buf_put_int32(p, end, (int32) N_AM_MAGIC);

    if (nsymtab == 0 && nstrtab > 0) {
	/* If the symbol table is empty, but the string table is not,
	 * assume that some foreign symbol table (e.g. COFF) is presented.
	 * We just put this, without conversion, in the place reserved for
	 * the string table, because we don't know which fields need
	 * byte swapping.
	 */
	p = buf_put_int32(p, end, (int32) N_AM_FOREIGN);
    } else {
        p = buf_put_int32(p, end, (int32) N_AM_VERSION);
    }

    /* symbol table length prefix */
    p = buf_put_int32(p, end, (int32) nsymtab);

    if (nsymtab > 0) {
	/* copy symbol table in buffer in network byte order */
	register long n;
	register struct nlist *sym;
	long nsyms = nsymtab / sizeof(struct nlist);

	sym = symtab;
	for (n = 0; n < nsyms; n++) {
	    p = buf_put_int32(p, end, (int32) sym->n_un.n_strx);
	    p = buf_put_int8 (p, end, (int8)  sym->n_type);
	    p = buf_put_int8 (p, end, (int8)  sym->n_other);
	    p = buf_put_int16(p, end, (int16) sym->n_desc);
	    p = buf_put_int32(p, end, (int32) sym->n_value);

	    sym++;
	}
    }

    /* put string table in buffer: a long + the string table itself */
    p = buf_put_int32(p, end, (int32) (nstrtab - sizeof(long)));
    (void) memcpy(p, strtab + sizeof(long),
		  (unsigned int) (nstrtab - sizeof(long)));

    return(0);
}

