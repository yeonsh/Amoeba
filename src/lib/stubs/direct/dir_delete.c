/*	@(#)dir_delete.c	1.3	94/04/07 11:00:49 */
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
dir_delete(server, name)
capability *server;
char *name;
{
	capset server_cs;
	errstat result;

	if (server) {
		if (cs_singleton(&server_cs, server) == 0)
			return STD_NOSPACE;
		result = sp_delete(&server_cs, name);
		cs_free(&server_cs);
	} else {
		/* Interpret name relative to default origin: */
		result = sp_delete(SP_DEFAULT, name);
	}

	return result;
}
