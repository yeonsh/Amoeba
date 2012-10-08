/*	@(#)typedefs.h	1.3	94/04/07 14:55:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define VOID void		/* Can't use typedef on some compilers */
typedef int BOOL;

typedef struct expr * ((*expr_fun)    P((struct expr* expr)));
typedef void 	      ((*void_func)   P((generic ptr)));
typedef char *        ((*string_func) P((generic ptr)));

