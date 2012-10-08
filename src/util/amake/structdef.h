/*	@(#)structdef.h	1.2	94/04/07 14:54:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef STRUCT_DEF
#define STRUCT_DEF

/* Generate structure allocation definitions */

#ifdef DEBUG
extern char *std_alloc();

#define DEF_STRUCT(Type, Freelist, Counter) \
	extern Type *Freelist;	\
	extern int   Counter;

#define NEW_STRUCT(Type, Freelist, Counter, Incr) \
	((Type *) std_alloc((char **)&Freelist, sizeof(Type), Incr, &Counter))

#else
extern char *st_alloc();

#define DEF_STRUCT(Type, Freelist, Counter) \
	extern Type *Freelist;	/* no Counter */ 

#define NEW_STRUCT(Type, Freelist, Counter, Incr) \
	((Type *) st_alloc((char **)&Freelist, sizeof(Type), Incr))

#endif

#define FREE_STRUCT(Type, Freelist, Structp) \
	st_free(Structp, &Freelist, sizeof(Type))

#endif
