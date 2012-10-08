/*	@(#)type.c	1.3	94/04/07 14:55:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include "dbug.h"
#include "global.h"
#include "alloc.h"
#include "error.h"
#include "expr.h"
#include "type.h"

PUBLIC struct type *ErrorType;
PUBLIC struct type *BooleanType, *AnyType, *StringType, *ObjectType;
PUBLIC struct type *ListStringType;

#define def_type(Type,Indicator) (Type = new_type())->tp_indicator = Indicator

PUBLIC void
type_init()
{
    def_type(ErrorType, T_ERROR);
    def_type(BooleanType, T_BOOLEAN);
    def_type(AnyType, T_ANY);
    def_type(StringType, T_STRING);
    def_type(ObjectType, T_OBJECT);
    def_type(ListStringType, T_LIST_OF | T_STRING);
}

PUBLIC char *
type_name(type)
int type;
/* Delivers the pointer to the name of a type. */
{
    static char type_string[20];
    /* big enough, we have no recursive type structure. */

    type_string[0] = '\0';	/* make type_string empty */

    switch(type & T_MISC) {
    case T_IN:		(void)strcat(type_string, "IN "); break;
    case T_OUT:		(void)strcat(type_string, "OUT "); break;
    case T_TMP:		(void)strcat(type_string, "TMP "); break;
    }

    switch(type & T_BASE) {
    case T_OBJECT:	(void)strcat(type_string, "OBJECT"); break;
    case T_BOOLEAN:	(void)strcat(type_string, "BOOLEAN"); break;
    case T_STRING:	(void)strcat(type_string, "STRING"); break;
    case T_INTEGER:	(void)strcat(type_string, "INTEGER"); break;
    case T_ANY:		(void)strcat(type_string, "ANY"); break;
    default:		(void)strcat(type_string, "ERROR"); break;
    }

    if (type & T_LIST_OF) {
	(void)strcat(type_string, " LIST");
    }

    return(type_string);
}

#ifdef DEBUG

PUBLIC void
PrType(type)
struct type *type;
{
    DBUG_WRITE((type_name(type->tp_indicator)));
    if (type->tp_attributes != NULL) {
	DBUG_WRITE((" [")); DBUG_Expr(type->tp_attributes); DBUG_WRITE(("] "));
    }
}

#endif /* DEBUG */
