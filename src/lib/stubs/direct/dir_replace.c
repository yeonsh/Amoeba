/*	@(#)dir_replace.c	1.3	96/02/27 11:14:40 */
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
dir_replace(server, name, object)
capability *server;
char *name;
capability *object;
{
        capset server_cs, object_cs;
        errstat result;

        if (cs_singleton(&object_cs, object) == 0)
                return STD_NOSPACE;
	if (server) {
		if (cs_singleton(&server_cs, server) == 0) {
			cs_free(&object_cs);
			return STD_NOSPACE;
		}
		result = sp_replace(&server_cs, name, &object_cs);
		cs_free(&server_cs);
	} else {
		/* Interpret name relative to default origin: */
		result = sp_replace(SP_DEFAULT, name, &object_cs);
	}
        cs_free(&object_cs);

        return result;
}
