/*	@(#)dump.h	1.3	94/04/07 14:49:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

typedef void ((*WriterFunc) P(( char *s  )));
EXTERN WriterFunc Write;

void Incr P((void));
void Decr P((void));
void NL P((void));
void DefaultWriter P((char *s ));
void Indent P((void));
char *QuotedId P((char *id ));

