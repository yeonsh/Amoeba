/*	@(#)dir_root.c	1.4	96/02/27 11:55:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** UNIX version of dir_root.
**
** This version allows $CAPABILITY to override $HOME/.capability.
** It also prints error messages if something is wrong.
*/

#include <stdio.h>
#include "amoeba.h"
#include "direct/direct.h"
#include "amupolicy.h"
#include "stdlib.h"
#include "string.h"
#include "module/buffers.h"

static void
std_err(s)
char *s;
{
	write(2, s, strlen(s));
}

dir_root(cap)
capability *cap;
{
    char *	home;
    char *	capfile;
    char	buf[DIR_BUFFER]; /* full name of .capability file */
    char	cap_buf[CAPSIZE];	/* buffer to hold capability */
    int		fd;
    int		n;

    if ((capfile = getenv("CAPABILITY")) == 0)
    {
	if ((home = getenv("HOME")) == 0 || *home == '\0')
	{
	    std_err("dir_root: $HOME not set\n");
	    return -1;
	}
	(void) sprintf(buf, "%s/%s", home, CAP_FILENAME);
	capfile = buf;
    }
    if ((fd = open(capfile, 0)) < 0)
    {
	std_err("dir_root: ");
	perror(capfile);
	return -1;
    }
    n = read(fd, cap_buf, sizeof(capability));
    close(fd);
    if (n != sizeof(capability) ||
		    buf_get_cap(cap_buf, &cap_buf[CAPSIZE], cap) == 0 ||
		    NULLPORT(&cap->cap_port))
    {
	std_err("dir_root: ");
	std_err(capfile);
	std_err(" doesn't contain a valid capability\n");
	return -1;
    }
    return 0;
}
