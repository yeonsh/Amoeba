/*	@(#)tmpid.c	1.3	96/02/27 11:12:06 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <string.h>
#include <module/rnd.h>


unsigned int
_temp_id()
/* Return a number that can be used to generate a "unique" temporary
 * filename.
 */
{
	port random;
	unsigned int id;
	
	uniqport(&random);
	/* use the first "sizeof(unsigned int)" bytes of the port */
	(void) memcpy(&id, &random, sizeof(unsigned int));
	return id;
}
