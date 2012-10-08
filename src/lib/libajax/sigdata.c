/*	@(#)sigdata.c	1.3	94/04/07 09:52:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Data set by signal module, read by others 
 * last modified apr 22 93 Ron Visser
 */


#include "ajax.h"
#include "sigstuff.h"

sigset_t _ajax_sighandle;
sigset_t _ajax_sigignore;
sigset_t _ajax_sigblock;
sigset_t _ajax_sigpending;
