/*	@(#)getuptime.c	1.3	96/02/27 13:11:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdlib.h>
#include <module/systask.h>


long
getuptime(phost)
	capability *phost;
{
	errstat err;
	char buf[100];
	int size;
	
	err = sys_kstat(phost, 'u', buf, sizeof buf, &size);
	if (err != STD_OK)
		return err;
	return atol(buf);
}
