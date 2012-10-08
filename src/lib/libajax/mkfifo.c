/*	@(#)mkfifo.c	1.2	94/04/07 09:48:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* mkfifo() POSIX 5.4.2 */

#include <ajax.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ampolicy.h>
#include "fifo/fifo.h"

#ifdef __STDC__
int mkfifo(char *path, mode_t mode)
#else
int mkfifo(path, mode) char *path; mode_t mode;
#endif
{
    errstat err;
    capability fifosvr, fifocap;

    /* get default fifo server cap */
    if ((err = name_lookup(DEF_FIFOSVR, &fifosvr)) != STD_OK) {
	/* "no fifo svr" => "out of file allocation resources" */
	ERR(ENOSPC, "mkfifo: no fifo svr");
    }

    if ((err = fifo_create(&fifosvr, &fifocap)) != STD_OK) {
	ERR(_ajax_error(err), "mkfifo: fifo_create failed");
    }

    /* install the new fifo cap */
    if ((err = name_append(path, &fifocap)) != STD_OK) {
	(void) std_destroy(&fifocap);
	ERR(_ajax_error(err), "mkfifo: cannot install fifo cap");
    }

    /* TODO: mode stuff */
    return 0;
}

