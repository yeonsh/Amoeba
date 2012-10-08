/*	@(#)hostname.c	1.1	96/02/27 13:09:41 */
/*
hostname.c

Created Oct 14, 1991 by Philip Homburg

write the (ip) hostname of the amoeba system to stdout.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <server/ip/gen/netdb.h>

char hostname[256];
char *prog_name;

void main _ARGS(( int argc, char *argv[] ));
void usage _ARGS(( void ));

void main(argc, argv)
int argc;
char *argv[];
{
	int result;

	prog_name= argv[0];
	if (argc != 1)
		usage();

	result= gethostname(hostname, sizeof(hostname));
	if (result == -1)
	{
		fprintf(stderr, "%s: unable to gethostname(): %s\n",
			prog_name, strerror(result));
		exit(1);
	}
	hostname[sizeof(hostname)-1]= '\0';
	fputs(hostname, stdout);
	exit(0);
}

void usage()
{
	fprintf(stderr, "USAGE: %s\n", prog_name);
	exit(1);
}
