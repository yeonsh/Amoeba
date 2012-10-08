/*	@(#)conversion.h	1.3	94/04/07 14:48:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

void ConversionInit	P((void));
struct expr *GetBoolean	P((struct expr *expr ));
struct expr *GetObject	P((struct expr *expr ));
struct expr *GetString	P((struct expr *expr ));
struct expr *GetInteger	P((struct expr *e ));
struct expr *GetAny	P((struct expr *expr ));
struct expr *GetList	P((struct expr *expr ));
struct expr *GetListOfType P((struct expr *expr , int type ));
struct expr *GetExprOfType P((struct expr *expr , int type ));
