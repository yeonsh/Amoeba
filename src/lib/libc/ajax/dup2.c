/*	@(#)dup2.c	1.4	94/04/07 10:25:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* dup2(2) system call emulation */

#include <fcntl.h>
#include <unistd.h>
#include "ajax.h"
#include "fdstuff.h"

int dup2(fd, fd2)
int fd, fd2;
{
    /* The behavior of dup2 is defined by POSIX in 6.2.1.2 as almost, but not
     * quite the same as fcntl.
     */

    if (fd2 < 0 || fd2 >= NOFILE) {
	ERR(EBADF, "dup2: bad fd");
    }

    /* Check to see if fildes is valid. */
    if (fcntl(fd, F_GETFL) < 0) {
	/* fd is not valid. */
	return(-1);
    } else {
	/* fd is valid. */
	if (fd == fd2) {
	    /* return original without closing it */
	    return(fd2);
	} else {
	    close(fd2);
	    return(fcntl(fd, F_DUPFD, fd2));
	}
    }
}
