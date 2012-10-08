/*	@(#)gethostname.c	1.2	94/04/07 10:34:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* gethostname(2) system call emulation */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <_ARGS.h>
#include <ampolicy.h>
#include <server/ip/gen/netdb.h>

int
gethostname(buf, len)
char *buf;
size_t len;
{
	char *hostname_file;
	int fd;
	int result;
	char *nl;

	hostname_file= getenv("HOSTNAME_FILE");
	if (hostname_file && !hostname_file[0])
		hostname_file= NULL;
	if (!hostname_file)
		hostname_file= HOSTNAME_FILE;

	fd= open(hostname_file, O_RDONLY);
	if (fd == -1)
		return -1;

	result= read(fd, buf, len);
	close(fd);
	if (result == -1)
		return -1;
	buf[len-1]= '\0';
	nl= strchr(buf, '\n');
	if (nl)
		*nl= '\0';
	return 0;
}
