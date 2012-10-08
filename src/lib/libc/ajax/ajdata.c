/*	@(#)ajdata.c	1.3	94/04/07 10:22:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Global variables defined by Ajax 
 * last modified apr 22 93 Ron Visser
 */

#include <amoeba.h>
#include "ajax.h"

int errno;		/* Error returned by last failing call */
char *_ajax_error_msg;	/* Error string from last failing call */
int   _ajax_forklevel;	/* Number of times this process image is removed
			   from an ancester started with exec*() */
mutex _ajax_forkmu;	/* To serialize fork attempts */


/* process attributes defined by posix */
int   _ajax_umask;	/* Umask set by umask() and used by open() */
uid_t _ajax_uid = -1;	/* the following attributes are only */
uid_t _ajax_euid = -1;  /* initialized when needed */
gid_t _ajax_gid = -1;
gid_t _ajax_egid = -1;

