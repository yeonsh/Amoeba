/*	@(#)dir_lookup.c	1.3	94/04/07 11:01:06 */
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
dir_lookup(server, name, object)
capability *server, *object;
char *name;
{
	capset server_cs, object_cs;
	errstat result;

	if (server) {
		if (cs_singleton(&server_cs, server) == 0)
			return STD_NOSPACE;
		result = sp_lookup(&server_cs, name, &object_cs);
		cs_free(&server_cs);
	} else {
		/* Interpret name relative to default origin: */
		result = sp_lookup(SP_DEFAULT, name, &object_cs);
	}
	if (result == STD_OK) {
		result = cs_to_cap(&object_cs, object);
		cs_free(&object_cs);
		return result;
	}

	return result;
}
