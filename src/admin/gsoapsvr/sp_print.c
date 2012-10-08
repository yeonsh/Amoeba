/*	@(#)sp_print.c	1.1	96/02/27 10:03:00 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "server/soap/soap.h"
#include "soapsvr.h"
#include "module/buffers.h"
#include "module/ar.h"
#include "assert.h"

#include "global.h"
#include "main.h"
#include "superfile.h"
#include "caching.h"
#include "seqno.h"
#include "sp_impl.h"

/*
 * Amount of debug messages depends on a few run-time configurable parameters:
 */
static int param_print_table = 0;

#ifdef __STDC__
char *cmd_name(command cmd)
#else
char *cmd_name(cmd) command cmd;
#endif
{
    static char unknown[20];

    switch (cmd) {
    case STD_TOUCH:	return "std_touch";
    case STD_DESTROY:	return "std_destroy";
    case SP_DISCARD:	return "sp_discard";
    case SP_CREATE:	return "sp_create";
    case SP_APPEND:	return "sp_append";
    case SP_REPLACE:	return "sp_replace";
    case SP_CHMOD:	return "sp_chmod";
    case SP_DELETE:	return "sp_delete";
    case SP_INSTALL:	return "sp_install";
    case SP_PUTTYPE:	return "sp_puttype";

    case SP_LIST:	return "sp_list";
    case SP_LOOKUP:	return "sp_lookup";
    case SP_SETLOOKUP:	return "sp_setlookup";
    case SP_GETMASKS:	return "sp_getmasks";
    }

    /* else unknown */
    sprintf(unknown, "unknown (%d)", cmd);
    return unknown;
}

void
print_row(sr, ncols)
struct sp_row *sr;
int ncols;
{
    int i;
    
    for (i = 0; i < ncols; i++) {
	printf("0x%x ", sr->sr_columns[i]);
    }
    printf("%ld %s\n", sr->sr_time, sr->sr_name);
}

void
sp_print_dir(sd)
struct sp_dir *sd;
{
    printf("ncol %d nrow %d rnd %s seq (%ld,%ld)\n",
	   sd->sd_ncolumns, sd->sd_nrows, ar_port(&sd->sd_random),
	   hseq(&sd->sd_seq_no), lseq(&sd->sd_seq_no));

#ifdef EXTENSIVE_DEBUG
    {
	int i;
    
	printf("columns:");
	for (i = 0; i < sd->sd_ncolumns; i++) {
	    printf("%s ", sd->sd_colnames[i]);
	}
	printf("\n");

	for (i = 0; i < sd->sd_nrows; i++) {
	    print_row(sd->sd_rows[i], sd->sd_ncolumns);
	}
    }
#endif
}

static void
sp_do_print_table()
{
    objnum obj;
    errstat err;

    printf("========== directory table ==========\n");

    for (obj = 1; obj < _sp_entries; obj++) {
	struct sp_table *st = &_sp_table[obj];

	if (sp_in_use(st)) {
	    int ttl;

	    /* Make sure it is cached in, but keep the time-to-live
	     * field as it is now:
	     */
	    ttl = get_time_to_live(obj);
	    err = sp_refresh(obj, SP_LOOKUP);
	    set_time_to_live(obj, ttl);

	    if (err != STD_OK) {
		scream("sp_print_table: refresh of %ld failed", (long) obj);
		continue;
	    } 

	    start_printing();
	    printf("obj %ld flags 0x%x time-left %d ",
		   obj, st->st_flags, st->st_time_left); 

	    assert(st->st_dir);
	    sp_print_dir(st->st_dir);
	    stop_printing();
	}
    }

    printf("=======================================\n");
}

void
sp_print_table()
{
    if (param_print_table) {
	sp_do_print_table();
    }
}

