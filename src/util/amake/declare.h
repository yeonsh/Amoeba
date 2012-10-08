/*	@(#)declare.h	1.3	94/04/07 14:48:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

void InitDeclare	P((void));
void EnterDeclaration	P((struct expr *decl ));
void InitInclude	P((void));
void EnterInclusion	P((struct expr *incl ));
void InitDefault	P((void));
void EnterDefault	P((struct expr *def ));
struct slist *GetDefaults P((void));
