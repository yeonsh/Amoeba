/*	@(#)eval.h	1.3	94/04/07 14:49:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Type requested as current function result. Used to determine if `exec'
 * has to give contents of returnfile, or just a boolean.
 */
EXTERN int ReqType;

/* optimised is_equal: */
#define is_equal(left, pright) ((left)==(*pright) || IsEqual(left, pright))

void eval_init	P((void));
void InitEval	P((void));
int  IsEqual	P((struct expr *left , struct expr **right ));
int  is_member	P((struct expr *elem , struct expr *list ));

struct expr *EvalDiagAlso P((struct expr *e ));
struct expr *Eval	  P((struct expr *e ));

