/*	@(#)error.c	1.1	93/10/07 15:22:19 */
/*
 *
 * Replicated RPC Package
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, The Netherlands
 *
 * Previous versions
 * Copyright 1990, 1991 by Mark D. Wood and the Meta Project, Cornell University
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the authors.
 *
 * Error handling routines
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include "amoeba.h"
#include "stderr.h"
#include "rrpc/rpc_err.h"


char * program_name = "";


void msg_fatal_error(err)
char *err;

{
#ifndef KERNEL
    extern int errno;
#endif

    extern int msg_trace;
    extern void exit();

    if (program_name)
      fprintf(stderr,"%s: ", program_name);

    fprintf(stderr,"Msg panic: %s", err);

#ifndef KERNEL
    if (errno)
      fprintf(stderr, "; errno = %d", errno);
#endif

    fprintf(stderr,"\n");

#ifndef KERNEL
#ifdef DEBUG
    if (1 | msg_trace) {
	fflush(stdout);
	abort();
    }
#endif
#endif
      
    exit(-1);
}



int check_amoeba_error(code, err_msg)
int code;
char *err_msg;

{
    extern int msg_trace;

    code = ERR_CONVERT(code);

    if ((code < 0)) {

	switch (code) {
	  case BC_ABORT :
	  case RPC_NOTFOUND :
	  case RPC_FAILURE :
#ifdef DEBUG
	    if (msg_trace) {
		if (program_name)
		  fprintf(stderr,"%s: ", program_name);
		fprintf(stderr, "%s:  ", err_msg);
		fprintf(stderr, "%s (%d)\n", err_why(code),code);
		fprintf(stderr, "continuing...\n");
	    }
#endif
	    return code;

	  default :
	    if (program_name)
	      fprintf(stderr,"%s: ", program_name);
	    fprintf(stderr, "%s:  ", err_msg);
	    fprintf(stderr, "%s (%d)\n", err_why(code),code);
	    break;
	}
      
	fflush(stdout);
	abort();
    }
    return 0;
}

