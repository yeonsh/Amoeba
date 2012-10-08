/*	@(#)alloc.h	1.2	94/04/07 14:46:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef ALLOC_H
#define ALLOC_H

extern char *Salloc	P((char *str, unsigned len));
extern char *Malloc	P((unsigned size));
extern char *Realloc	P((char *mem, unsigned newlen));

/*	S T R U C T U R E - S T O R A G E  D E F I N I T I O N S	*/

typedef struct ALLOC_ {
	struct ALLOC_ *_A_next;
} *PALLOC_;

extern char *st_alloc P((char **phead, unsigned int size, int count));

#define	_A_st_free(ptr, phead, size)	(((PALLOC_)ptr)->_A_next = \
						(PALLOC_)(*phead), \
					 *((PALLOC_ *)phead) = \
						(PALLOC_) ptr)
#define st_free(ptr, phead, size)	_A_st_free(ptr, phead, size)

#endif /* ALLOC_H */
