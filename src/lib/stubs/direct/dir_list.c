/*	@(#)dir_list.c	1.3	94/04/07 11:00:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "sp_dir.h"
#include "server/soap/soap.h"
#include "stdlib.h"
#include "module/direct.h"


struct dir_open *
dir_open(server)
capability *server;
{
	capset cs_dir;
	SP_DIR *sp_dirp;
	errstat result;

	if (!server && (server = dir_origin("")) == 0)
		return 0;

	if (cs_singleton(&cs_dir, server) == 0)
		return 0;
	result = sp_list(&cs_dir, &sp_dirp);
	cs_free(&cs_dir);

	if (result == STD_OK)
		return (struct dir_open *)sp_dirp;
	else
		return (struct dir_open *)0;
}

char *
dir_next(dp)
	register struct dir_open *dp;
{
	struct sp_direct *row;

	row = sp_readdir((SP_DIR *)dp);
	if (row == 0)
		return 0;
	return row->d_name;
}

errstat
dir_close(dp)
	register struct dir_open *dp;
{
	sp_closedir((SP_DIR *)dp);
	return STD_OK;
}

errstat
dir_rewind(dp)
	register struct dir_open *dp;
{
	sp_rewinddir((SP_DIR *)dp);
	return STD_OK;
}
