/*	@(#)dir_append.c	1.4	96/02/27 11:14:29 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "server/soap/soap.h"
#include "module/direct.h"


extern void _dir_soapcompat();

errstat
dir_append(server, name, object)
capability *server;
char *name;
capability *object;
{
	capset server_cs, object_cs;
	errstat result;
	extern int _ds_ncolumns;
	extern long _ds_colmasks[];

	_dir_soapcompat();
	if (cs_singleton(&object_cs, object) == 0)
		return STD_NOSPACE;
	if (server) {
		if (cs_singleton(&server_cs, server) == 0) {
			cs_free(&object_cs);
			return STD_NOSPACE;
		}
		result = sp_append(&server_cs, name, &object_cs,
					_ds_ncolumns, _ds_colmasks);
		cs_free(&server_cs);
	} else {
		/* Interpret name relative to default origin: */
		result = sp_append(SP_DEFAULT, name, &object_cs,
					_ds_ncolumns, _ds_colmasks);
	}
	cs_free(&object_cs);

	return result;
}
