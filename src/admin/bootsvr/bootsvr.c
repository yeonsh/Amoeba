/*	@(#)bootsvr.c	1.5	94/04/06 11:38:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "stderr.h"
#include "monitor.h"
#include "semaphore.h"
#include "svr.h"

extern errstat intr_init();
extern errstat ml_boot_svr();

/*
 *	Some global data
 */
semaphore start_service;
capability *get_capp, putcap;

/*
 *	Verbose is initially 2, and changed
 *	to 0 some time after initialisation.
 */
bool debugging = 0, verbose = 2, closing = 0, SaveState = 0, TimePrint = 0;

/*ARGSUSED*/
static void
call_mainloop(param, parsize)
char *param;
int   parsize;
{
    static mutex mu;

    (void) timeout((interval) 10000);
    sema_down(&start_service);
    mu_lock(&mu);
    if (intr_init() != STD_OK) abort();
    MU_CHECK(mu);
    mu_unlock(&mu);
    (void) ml_boot_svr(&get_capp->cap_port);
    prf("%nMainloop exits!?!?!?!?!!?!?!?\n");
    thread_exit();
} /* call_mainloop */


static void
usage(progname)
    char *progname;
{
    static char usg[] = "Usage: %s [-dtv] [rootdir]\n";
    prf(usg, progname);
    exit(2);
} /* usage */

/*ARGSUSED*/
int
main(argc, argv)
    int argc;
    char *argv[];
{
    int i;
    extern int optind;

    while ((i = getopt(argc, argv, "dtv")) != EOF)
	switch (i) {
	case 'd':
	    debugging = 1;
	    break;
	case 't':
	    TimePrint = 1;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	default:
	    usage(argv[0]);
	}
    argv += optind;
    argc -= optind;
    if (argc > 1) usage(argv[0]);

    /* This is to allow starting by hand */
    if (argc == 1) {
	capability root, *capp;
	errstat err;

	err = name_lookup(argv[0], &root);
	if (err != STD_OK) {
	    prf("%ncannot find %s\n", argv[0]);
	    exit(2);
	}
	/* To do: Should validate that this capability is suitable */
	capp = getcap("ROOT");
	if (capp == NULL) abort();
	*capp = root;
    }
    timeout((interval) 10000);
    sema_init(&start_service, 0);

    TimerInit();
    /* Start some threads */
    for (i = 0; i < N_TIMER_THREADS; ++i)
	if (!thread_newthread(TimerThread, TIMER_STACK, (char *) NULL, 0)) {
	    prf("%nCannot start a poll thread\n");
	    if (i == 0) exit(1);
	}

    for (i = 0; i < N_SVR_THREADS - 1; ++i)
	if (!thread_newthread(call_mainloop, BOOT_STACK, (char *) NULL, 0)) {
	    prf("%nCannot start a server thread\n");
	    break;
	}

    /*
     *	Run one server in the main thread:
     */
    call_mainloop((char *) NULL, 0);
    /*NOTREACHED*/
} /* main */
