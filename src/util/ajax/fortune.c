/*	@(#)fortune.c	1.1	91/04/24 17:34:53 */
/*  fortune  -  hand out Chinese fortune cookies	Author: Bert Reuling */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COOKIEJAR "/usr/lib/fortune.dat"

#ifndef lint
static char *Copyright = "\0Copyright (c) 1990 Bert Reuling";
#endif
unsigned long seed;

/*ARGSUSED*/
main(argc, argv)
int argc;
char *argv[];
{
    int c1, c2, c3;
    long magic();
    struct stat cookie_stat;
    FILE *cookie;

    if ((cookie = fopen(COOKIEJAR, "r")) == NULL) {
	printf("\nSome things better stay closed.\n  - %s\n", argv[0]);
	exit (-1);
    }

    /* Create seed from : date, time, user-id and process-id. we can't get
    * the position of the moon, unfortunately. Note that super cookies
    * are not affected by chance...
    */
    seed = time((time_t *) 0) * (long) getuid() * (long) getpid();

    if (stat(COOKIEJAR, &cookie_stat) != 0) {
	printf("\nIt furthers one to see the super guru.\n  - %s\n", argv[0]);
	exit (-1);
    }
    /* move by magic... */
    (void) fseek(cookie, magic(cookie_stat.st_size), 0);

    c2 = c3 = '\n';
    while (((c1 = getc(cookie)) != EOF) &&
			((c1 != '%') || (c2 != '%') || (c3 != '\n'))) {
	c3 = c2;
	c2 = c1;
    }

    if (c1 == EOF) {
	printf("\nSomething unexpected has happened.\n  - %s", argv[0]);
	exit (-1);
    }

    c2 = c3 = '\n';
    while (((c1 = getc(cookie)) != '%') || (c2 != '%') || (c3 != '\n')) {
	if (c1 == EOF) {
		rewind(cookie);
		continue;
	}
	putc(c2, stdout);
	c3 = c2;
	c2 = c1;
    }
    putc('\n', stdout);
    fclose(cookie);
    exit (0);
}

/*  magic  -  please study carefully: there is more than meets the eye */
long magic(range)
long range;
{
    seed = 9065531L * (seed % 9065533L) - 2 * (seed / 9065531L) + 1L;
    return (seed % range);
}
