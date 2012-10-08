/*	@(#)fpathconf.c	1.2	94/04/07 10:27:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* fpathconf() POSIX 5.7.1
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include "tty/term.h"
#include "fifo/fifo.h"


long fpathconf(fd, name)
int fd;				/* file descriptor being interrogated */
int name;			/* property being inspected */
{
/* POSIX allows some of the values in <limits.h> to be increased at
 * run time.  The pathconf and fpathconf functions allow these values
 * to be checked at run time.
 */

  struct fd *f;
  errstat err;
  long size;

  if (fd < 0 || fd >= OPEN_MAX) 
	ERR(EBADF, "fpathconf: invalid filedescriptor");

  switch (name) {
  case _PC_PIPE_BUF:
	FDINIT();
	FDLOCK(fd, f);
	err = fifo_bufsize(&f->fd_cap, &size);
	FDUNLOCK(f);
	if (err == STD_COMBAD) {
		ERR(EINVAL, "fpathconf: _PC_PIPE_BUF not supported");
	} else if (err != STD_OK) {
		ERR(_ajax_error(err), "fpathconf: fifo_bufsize failed");
	}
	break;
  default:
	size = _ajax_conf(name);
  }

  return size;
}
