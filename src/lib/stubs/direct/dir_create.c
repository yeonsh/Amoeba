/*	@(#)dir_create.c	1.3	94/04/07 11:00:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "server/soap/soap.h"
#include "module/direct.h"


errstat
dir_create(server, object)
capability *server;
capability *object;
{
	extern char *_ds_columns[];
	capset server_cs, object_cs;
	errstat result;

	if (server) {
		if (cs_singleton(&server_cs, server) == 0)
			return STD_NOSPACE;
		result = sp_create(&server_cs, _ds_columns, &object_cs);
		cs_free(&server_cs);
	} else {
		/* Create directory using the server that owns the current
		 * WORK directory:
		 */
		result = sp_create(SP_DEFAULT, _ds_columns, &object_cs);
	}
	if (result == STD_OK) {
		result = cs_to_cap(&object_cs, object);
		cs_free(&object_cs);
		return result;
	}

	return result;
}
