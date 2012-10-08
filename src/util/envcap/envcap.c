/*	@(#)envcap.c	1.3	96/02/27 13:08:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <stdio.h>
#include <stdlib.h>
#include <caplist.h>
extern struct caplist *capv;
#include <capset.h>
#include <module/buffers.h>


main(argc, argv)
	int argc;
	char **argv;
{
	if (argc > 3) {
		fprintf(stderr, "Usage: %s [ cap-name ]\n", argv[0]);
		exit(2);
	}
	if (argc == 1) {	/* Print the list of capability names: */
		register struct caplist *cl = capv;
		while (cl->cl_name != 0) {
			(void) puts(cl->cl_name);
			cl++;
		}
		exit(0);
	} else {
		capability *cap;
		char buf[512], *p;
		capset cs;

		if ((cap = getcap(argv[1])) != NULL) {
			(void)cs_singleton(&cs, cap);
			if ((p = buf_put_capset(buf, &buf[512], &cs)) == 0) {
				fprintf(stderr, "%s: can't put capset\n",
								argv[0]);
				exit(1);
			}
			(void) fwrite(buf, 1, (size_t) (p - buf), stdout);
			exit(0);
		} else {
			fprintf(stderr, "%s: capability %s not found.\n",
					argv[0], argv[1]);
			exit(1);
		}
	}
}
