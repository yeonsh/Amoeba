/*	@(#)check.h	1.3	94/04/07 14:47:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

void check_init		P((void));
void CheckCallID	P((struct idf **idp ));
struct expr *ArgPlace	P((struct expr *call , struct idf *arg_idp ));
void CheckAllArgsAssigned
			P((struct expr *call , int *ret_type ));
void CheckArg		P((struct idf *func , int count , struct expr *arrow ,
			   int *ret_type , int *any_type ));
