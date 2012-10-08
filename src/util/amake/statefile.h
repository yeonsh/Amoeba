/*	@(#)statefile.h	1.3	94/04/07 14:54:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

EXTERN int ReadingCache;
EXTERN int Recovering;
EXTERN struct idf *Id_cache_entry;

void SharedExpressionUsed	P((struct expr *expr ));
void CheckStatefile		P((void));
void ReadStateFile		P((void));
void DumpCacheEntry		P((struct cached *c ));
void DumpStatefile		P((void));
void UpdateStateFile		P((void));
void InitUpdate			P((void));

