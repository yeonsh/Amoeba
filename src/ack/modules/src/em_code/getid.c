/*	@(#)getid.c	1.2	94/04/06 11:18:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


/*	Get a unique id for C_insertpart, etc.
*/

C_getid()
{
	static int id = 0;

	return ++id;
}
