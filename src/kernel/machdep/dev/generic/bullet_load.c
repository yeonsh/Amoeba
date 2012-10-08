/*	@(#)bullet_load.c	1.2	94/04/06 09:08:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/direct.h"
#include "bullet/bullet.h"

/*ARGSUSED*/
errstat
bullet_loader(base, size, bulletname, longpar)
char *base;
char *bulletname;
{
	capability bfile;
	errstat err;
	b_fsize num_read;

	if ((err = dir_lookup((capability *) 0, bulletname, &bfile)) != STD_OK) {
		printf("Couldn't lookup %s\n", bulletname);
		return err;
	}
	if ((err = b_read(&bfile, (b_fsize) 0, base, size, &num_read)) != STD_OK) {
		printf("Read failed\n");
		return err;
	}
	if (num_read != size)
		printf("Short read, continuing anyway\n");
	return STD_OK;
}
