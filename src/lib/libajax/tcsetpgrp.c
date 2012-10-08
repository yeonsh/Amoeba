/*	@(#)tcsetpgrp.c	1.2	94/04/07 09:54:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* tcsetpgrp() POSIX 7.2.4
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/types.h>

pid_t
tcsetpgrp(fd, pgrp_id)
	int fd;
	pid_t pgrp_id;
{
	ERR(ENOSYS, "tcsetpgrp: not supported");
}
