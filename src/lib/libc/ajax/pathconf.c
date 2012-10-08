/*	@(#)pathconf.c	1.4	96/02/27 11:05:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* pathconf() POSIX 5.7.1 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"
#include <fcntl.h>
#include <unistd.h>
#include "ampolicy.h"
#include "fifo/fifo.h"

long pathconf(path, name)
char *path;		/* name of file being interrogated */
int name;		/* property being inspected */
{
/* POSIX allows some of the values in <limits.h> to be increased at
 * run time.  The pathconf and fpathconf functions allow these values
 * to be checked at run time.  
 * We cannot simply open path and call fpathconf because some files
 * cannot be opened (e.g. directories).
 */

  int fd;
  long val;
  errstat err;

  switch (name) {
  case _PC_PIPE_BUF:
	/* path must be opened with O_NONBLOCK in case it's a fifo */
	if ((fd = open(path, O_RDONLY | O_NONBLOCK)) >= 0) {
		val = fpathconf(fd, name);
		close(fd);
	} else {
		/* see if it's a directory */
		capability cap;
		capability *origin;

		if ((origin = dir_origin(path)) == NULL) {
			ERR(EIO, "pathconf: cannot get /");
		}
		err = _ajax_lookup(origin, path, &cap,
				   (int *) NULL, (long *) NULL);
		if (err != STD_OK) {
			ERR(_ajax_error(err), "pathconf: lookup failed");
		}

		if (_is_am_dir(&cap)) {
			/* Path is a directory, so determine the bufsize for
			 * fifo's that can be created in this directory.
		 	 */
			if ((err = name_lookup(DEF_FIFOSVR, &cap)) != STD_OK) {
				ERR(EINVAL, "pathconf: fifo server not found");
			}
			err = fifo_bufsize(&cap, &val);
			if (err != STD_OK) {
				ERR(EINVAL, "pathconf: fifo_bufsiz failed");
			}
		} else {
			/* open() failed, but it's no directory */
			ERR(errno, "pathconf: open failed");
		}
	}
	break;

  default:
	val = _ajax_conf(name);
	break;
  }

  return(val);
}
