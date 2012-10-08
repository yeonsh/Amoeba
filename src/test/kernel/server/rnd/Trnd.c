/*	@(#)Trnd.c	1.2	94/04/06 17:42:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stderr.h"
#include "random/random.h"
#include "module/rnd.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "string.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>

#define RBUFFSIZE    151
#define REQUESTED    100	/* Must be less than RBUFFSIZE */

/* Test the random server. */
main(argc, argv)
	int argc; char **argv;
{
	capability rnd_cap;
	char *name;
	int i;
	char orig_buff[RBUFFSIZE], buff1[RBUFFSIZE], buff2[RBUFFSIZE];

	TEST_BEGIN(argv[0]);

	/* Get the name of the random server to be tested: */
	if (argc > 1 && argv[1])
		name = argv[1];
	else
		name = DEF_RNDSVR;

	TEST_ASSERT(name_lookup(name, &rnd_cap) == STD_OK, TEST_FATAL,
			("Cannot find specified random server: %s\n", name));

	rnd_setcap(&rnd_cap);

	/* Initialize the two buffers to the same non-random contents: */
	for (i = 1; i < RBUFFSIZE - 1; i++)
		orig_buff[i] = buff1[i] = buff2[i] = i;
	orig_buff[RBUFFSIZE - 1] = '\0';
	buff1[RBUFFSIZE - 1] = buff2[RBUFFSIZE - 1] = '\0';

	/* Get two random buffers: */
	TEST_STATUS(rnd_getrandom(buff1, REQUESTED), STD_OK,
					    "rnd_getrandom(buff1, 100)");

	TEST_STATUS(rnd_getrandom(buff2, REQUESTED), STD_OK,
					    "rnd_getrandom(buff2, 100)");

	/* Ensure that the same thing was not returned twice (odds of this
	 * happening legitimately are so low that an error is much more
	 * likely): */
	TEST_ASSERT(strncmp(buff1, buff2, REQUESTED) != 0, TEST_SERIOUS,
		("two calls to rnd_getrandom(buff, %d) returned same buff\n",
		 REQUESTED));

	/* Ensure that both the buffers really got modified: */
	TEST_ASSERT(strncmp(orig_buff, buff1, REQUESTED) != 0 &&
                    strncmp(orig_buff, buff2, REQUESTED) != 0, TEST_SERIOUS,
		("a call to rnd_getrandom(buff, %d) did not modify buff\n",
		    REQUESTED));

	/* Ensure that neither buffer was modified beyond the REQUESTED number
	 * of bytes: */
	TEST_ASSERT(strcmp(orig_buff+REQUESTED, buff1+REQUESTED) == 0 &&
		    strcmp(orig_buff+REQUESTED, buff1+REQUESTED) == 0,
		    TEST_SERIOUS,
		("rnd_getrandom(buff, %d) modified bytes beyond end of buff\n",
		    REQUESTED));

	TEST_END();
	/*NOTREACHED*/
}
