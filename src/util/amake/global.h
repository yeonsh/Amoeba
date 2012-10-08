/*	@(#)global.h	1.3	94/04/07 14:50:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * Much used constants
 */
#define TRUE    (1)
#define FALSE   (0)

/*
 *  Macro	Use
 *
 *  EXTERN	declare identifier in a .h file (i.e., export it)
 *  PUBLIC	definition of an exported identifier
 *  PRIVATE	definition of a non-exported identifier
 */
#define EXTERN		extern
#define PUBLIC
#define PRIVATE		static

/* for function prototypes: */
#ifdef __STDC__
#define P(list)	list
#else
#define P(list)	()
#endif

#ifndef GENERIC_TYPE
#define GENERIC_TYPE void*
#endif

typedef GENERIC_TYPE generic;
#include "typedefs.h"

#if defined(AMOEBA) || defined(unix)
#  define amunix 1
#else
#  define amunix 0
#endif

#include "struct.def"

#if defined(__STDC__) || defined(AMOEBA) || defined(_MINIX) || defined(MIPSEB) || defined(SYSV)
#include <string.h>
#else
extern char *  strcpy();
extern char *  strncpy();
extern char *  strcat();
extern char *  strncat();
extern int     strcmp();
extern int     strncmp();
extern int     strlen();
extern char *  strchr();
extern char *  strrchr();
#endif

/* In ANSI C, NULL is defined in (among others) string.h, which we have
 * just included.
 */
#ifndef NULL
#define NULL    0	/* nil pointer */
#endif

#if __STDC__
#include <stdlib.h>
#else
extern char *getenv();
#endif

#ifdef __STDC__
/* A jmp_buf is an array (of length one) of a peculiar structure in ACK ANSI C.
 * To avoid a warning, we just cast a jmp_buf pointer to (void *).
 */
#define JMPCAST		(void *)
#else
#define JMPCAST		(jmp_buf *)
#endif

#endif /* GLOBAL_H */
