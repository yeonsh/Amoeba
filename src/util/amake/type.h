/*	@(#)type.h	1.2	94/04/07 14:55:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef TYPE_H
#define TYPE_H

/*
 * Types are implemented by setting some of the following bits in an int.
 * So an "LIST OF OBJECT" is represented by (T_LIST_OF|T_OBJECT)
 */
#define T_ERROR		0x001
#define T_STRING	0x002
#define T_BOOLEAN	0x004
#define T_OBJECT	0x008	/* general Amake object (but mostly a file) */
#define T_ANY		0x010	/* for 'generic' functions like  if  */
#define T_INTEGER	0x020
#define T_BASE		(T_ERROR|T_STRING|T_BOOLEAN|T_OBJECT|T_ANY|T_INTEGER)

#define T_LIST_OF	0x040
#define T_TYPE		(T_BASE|T_LIST_OF)

#define T_IN		0x0100
#define T_OUT		0x0200
#define T_TMP		0x0400
#define T_FILE		0x0800
#define T_SPECIAL	0x1000	/* for keywords RETURN, DIAG and IGNORE */

#define T_MISC		(T_IN|T_OUT|T_TMP|T_FILE|T_SPECIAL)


struct type {
    int		 tp_indicator;
    struct expr  *tp_attributes;
};

/* allocation definitions of struct type */
#include "structdef.h"
DEF_STRUCT(struct type, h_type, cnt_type)
#define new_type()        NEW_STRUCT(struct type, h_type, cnt_type, 10)
#define free_type(p)      FREE_STRUCT(struct type, h_type, p)

EXTERN struct type *ErrorType;
EXTERN struct type *BooleanType, *FileType, *AnyType, *StringType, *ObjectType;
EXTERN struct type *ListStringType, *ListFileType;

EXTERN void type_init P(( void ));

EXTERN char *type_name P(( int type ));
/*
 * Initialises a private buffer (that is destroyed on each call) with the
 * name of the type, and delivers a pointer to it.
 */

#ifdef DEBUG
EXTERN void PrType P((struct type *type));
#else
#define PrType(type)
#endif /* DEBUG */

#endif
