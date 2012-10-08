/*	@(#)generic.h	1.2	94/04/07 14:50:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


struct Generic {
    struct idf	 *gen_idp;
    struct scope *gen_scope;		/* parameters */
    char 	 *gen_text;		/* text between '{' and '}' */
};

/* allocation definitions of struct Generic */
#include "structdef.h"
DEF_STRUCT(struct Generic, h_Generic, cnt_Generic)
#define new_Generic()    NEW_STRUCT(struct Generic, h_Generic, cnt_Generic, 5)
#define free_Generic(p)  FREE_STRUCT(struct Generic, h_Generic, p)

EXTERN int GenericSize; /* # bytes allocated, set when GenericRead() returns */

char *ReadBody		P((void));
void MakeInstance	P((struct expr *instance ));
