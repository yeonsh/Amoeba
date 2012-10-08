/*	@(#)Trnd2.c	1.2	94/04/06 17:42:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Tests for threads and rnd_getrandom.
 */

#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/rnd.h"
#include "module/name.h"
#include "thread.h"
#include "semaphore.h"
#include "test.h"

#define NTHREADS	4
#define NRANDOM_SEQ	500
#define NRANDOM_PAR	100

semaphore sem_ready, sem_lock;
int ident;

typedef struct {
    char buffer[100];
} BUFFER;

/*ARGSUSED*/
static void 
execute_test(param, size)
char *param;
int   size;
{
    errstat     err;
    int		i;
    BUFFER     *s = (BUFFER *) thread_alloc(&ident, sizeof(BUFFER));

    if (!s) {
	sema_down(&sem_lock);
	TEST_ASSERT(0, TEST_SERIOUS, ("thread_alloc failed\n"));
	sema_up(&sem_lock);
    } else {
	for (i = 0; i < NRANDOM_PAR; i++) {
	    err = rnd_getrandom(s->buffer, sizeof(BUFFER));
	    if (err != STD_OK) {
		sema_down(&sem_lock);
		TEST_ASSERT(0, TEST_SERIOUS,
			   ("rnd_getrandom failed, err = %s\n", err_why(err)));
		sema_up(&sem_lock);
	    }
	}
    }
    sema_up(&sem_ready);
    thread_exit();
}

main(argc, argv)
int   argc;
char *argv[];
{
    errstat    err;
    char      *name;
    capability rnd_cap;
    int        maxthread, nthread;
    int        i;

    TEST_BEGIN(argv[0]);

    /* Get the name of the random server to be tested: */
    if (argc > 1) {
	name = argv[1];
    } else {
	name = DEF_RNDSVR;
    }

    TEST_ASSERT(name_lookup(name, &rnd_cap) == STD_OK, TEST_FATAL,
		("Cannot find specified random server: %s\n", name));

    rnd_setcap(&rnd_cap);

    sema_init(&sem_ready, 0);
    sema_init(&sem_lock, 1);

    /* first get the random numbers sequentially */
    for (i = 0; i < NRANDOM_SEQ; i++) {
	char rnd[4];

	err = rnd_getrandom(rnd, sizeof(rnd));
	TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
		    ("rnd_getrandom failed, err = %s\n", err_why(err)));
    }

    /* now get them in parallel */
    maxthread = NTHREADS;
    nthread = 0;

    for (i = 0; i < maxthread; i++) {
	if (!thread_newthread(execute_test, 8000, (char *) 0, 0)) {
	    TEST_ASSERT(0, TEST_SERIOUS, ("thread_newthread() failed\n"));
	} else {
	    nthread++;
	}
    }

    for (i = 0; i < nthread; i++) {
	sema_down(&sem_ready);
    }

    TEST_END();
    /*NOTREACHED*/
}
