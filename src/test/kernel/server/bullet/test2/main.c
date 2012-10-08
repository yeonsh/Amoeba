/*	@(#)main.c	1.2	94/04/06 17:38:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: ti_test_all.c
 * 
 * Original	: March 24, 1992
 * Author(s)	: Irina
 * Modified	: Kees Verstoep (parallel/sequential merge + general cleanup)
 * Description	: Main program for test procedures for Bullet server
 * 
 */

#include "Tginit.h"

#define NUMBER_OF_TESTS	   14
#define NUMBER_MAX_THREADS 100

typedef bool (boolF) _ARGS((char *, ABUFPRINT));

EXTERN boolF
    test_b_create,
    test_b_size,
    test_b_read,
    test_b_modify,
    test_b_insert,
    test_b_delete,
    test_b_sync,
    test_b_fsck,
    test_std_touch,
    test_std_destroy,
    test_std_restrict,
    test_std_copy,
    test_std_status,
    test_std_info;

PRIVATE char *server;
char *Alternate_server = NULL;

semaphore   sem;
PRIVATE mutex result_mu;
PRIVATE int result[NUMBER_OF_TESTS + 1];	/* the same length as opt */

PRIVATE BUFPRINT ab[NUMBER_MAX_THREADS];

typedef boolF *boolPF;

PRIVATE struct {
    char  *test_fun_name;
    boolPF test_fun;
} test [NUMBER_OF_TESTS] = {
    { "b_create",     test_b_create	},
    { "b_size",       test_b_size	},
    { "b_read",       test_b_read	},
    { "b_modify",     test_b_modify	},
    { "b_insert",     test_b_insert	},
    { "b_delete",     test_b_delete	},
    { "b_sync",       test_b_sync	},
    { "b_fsck",       test_b_fsck	},
    { "std_touch",    test_std_touch	},
    { "std_destroy",  test_std_destroy	},
    { "std_restrict", test_std_restrict	},
    { "std_copy",     test_std_copy	},
    { "std_status",   test_std_status	},
    { "std_info",     test_std_info	}
};

PRIVATE void
do_execute_generic_test(index, testno)
int index;
int testno;
{
    if (testno < 0 || testno >= sizeof(test)/sizeof(test[0])) {
	printf("invalid test function index %d\n", testno);
	exit(1);
    }

    init_exception_flag(&ab[index]);
    if (!setjmp(*((jmp_buf *) thread_alloc(&identg, sizeof(jmp_buf))))) {
	int res =  (*test[testno].test_fun)(server, &ab[index]);

	mu_lock(&result_mu);
	result[testno] += res;
	mu_unlock(&result_mu);
    } else {
	test_for_exception(&ab[index]);
    }
}

/*ARGSUSED*/
PRIVATE void 
execute_generic_test(param, size)
char *param;
int   size;
{
    int *n = (int *) param;
    int testno = ab[*n].testno;

    do_execute_generic_test(*n, testno);
    sema_up(&sem);
    thread_exit();
}

PRIVATE void
perform_parallel_tests(opt, l)
bool opt[NUMBER_OF_TESTS + 1];
int  l;
{
    int i, k, ntests, nthreads;
    int *iab;
    int ib;

    if (opt[NUMBER_OF_TESTS]) {
	ntests = NUMBER_OF_TESTS;
    } else {
	ntests = 0;
	for (i = 0; i < NUMBER_OF_TESTS; i++) {
	    if (opt[i]) {
		ntests++;
	    }
	}
    }

    if (l * ntests > NUMBER_MAX_THREADS) {
	l = NUMBER_MAX_THREADS / ntests;
    }
    ib = 0;
    sema_init(&sem, 0);

    nthreads = 0;
    for (k = 0; k < l; k++) {
	for (i = 0; i < NUMBER_OF_TESTS; i++) {
	    if (opt[i] || opt[NUMBER_OF_TESTS]) {
		if ((iab = (int *) malloc(sizeof(int))) != 0 &&
		    (ab[ib].buf = (char *) malloc((size_t)(8 * K))) != 0)
		{
		    *iab = ib;
		    ab[ib].endi = ab[ib].buf + 8 * K;
		    ab[ib].p = bprintf(ab[ib].buf, ab[ib].endi, "");
		    ab[ib].aident = &identg;
		    ab[ib].testno = i;
		    ib++;
		    if (!thread_newthread(execute_generic_test, (int)(8*K),
					  (char *) iab, sizeof(int*)))
		    {
			printf("\nthread_newthread() failed\n");
			break;
		    } else {
			nthreads++;
		    }
		} else {
		    printf("Not enough memory to start new thread\n");
		}
	    }
	}
    }

    printf("Number of threads running = %d\n", nthreads);

    for (i = 0; i < nthreads; i++)
	sema_down(&sem);

    for (i = 0; i < nthreads; i++) {
	if (ab[i].p > ab[i].buf) {
	    printf("\nDiagnostics for thread %d (%s):\n\n",
		   i, test[ab[i].testno].test_fun_name);

	    fwrite(ab[i].buf, (size_t) (ab[i].p - ab[i].buf),
		   (size_t) 1, stdout);
	    printf("-----------------------------\n", i);
	}
    }
    for (i = 0; i < ib; i++) {
	free((_VOIDSTAR) ab[i].buf);
	ab[i].buf = NULL;
    }

    for (i = 0; i < NUMBER_OF_TESTS; i++) {
	if (opt[i] || opt[NUMBER_OF_TESTS]) {
	    printf("%-12.12s: %d test%s passed (%s)\n",
		   test[i].test_fun_name, 
		   result[i], result[i] != 1 ? "s" : "",
		   (result[i] == (nthreads / ntests) ? "ok" : "*not ok*"));
	}
    }
}

PRIVATE void
perform_serial_tests(opt, nruns)
bool opt[NUMBER_OF_TESTS + 1];
int  nruns;
{
    int i, k;
    int ib;

    ib = 0;
    ab[ib].buf = (char *) malloc((size_t)(8 * K));
    if (ab[ib].buf == 0) {
	fprintf(stderr, "could not allocate print buffer\n");
	exit(1);
    }
    ab[ib].endi = ab[ib].buf + 8 * K;

    for (k = 0; k < nruns; k++) {
	for (i = 0; i < NUMBER_OF_TESTS; i++) {
	    if (opt[i] || opt[NUMBER_OF_TESTS]) {
		ab[ib].aident = &identg;
		ab[ib].p = bprintf(ab[ib].buf, ab[ib].endi, "");
		ab[ib].testno = i;
		do_execute_generic_test(ib, i);
		if (ab[ib].p > ab[ib].buf) {
		    printf("\nDiagnostics for test %s:\n\n",
			   test[i].test_fun_name);

		    fwrite(ab[ib].buf, (size_t) (ab[ib].p - ab[ib].buf),
			   (size_t) 1, stdout);
		    printf("-----------------------------\n", i);
		}
	    }
	}
    }

    free((_VOIDSTAR) ab[ib].buf);
    ab[ib].buf = NULL;

    for (i = 0; i < NUMBER_OF_TESTS; i++) {
	if (opt[i] || opt[NUMBER_OF_TESTS]) {
	    printf("%-12.12s: %d test%s passed (%s)\n",
		   test[i].test_fun_name, 
		   result[i], result[i] != 1 ? "s" : "",
		   (result[i] == nruns ? "ok" : "*not ok*"));
	}
    }
}

PUBLIC int 
main(argc, argv)
int         argc;
char       *argv[];
{
    char        car;
    int         l, i;
    char        numar[80];
    bool        opt[NUMBER_OF_TESTS + 1], got_opt;
    int		parallel;

    mu_init(&result_mu);

    /* Decide which Bullet server to use. */
    if (argc < 2 || argc > 3) {
	fprintf(stderr, "usage: %s bulletsvr [bulletsvr2]\n", argv[0]);
	exit(1);
    }
    server = argv[1];
    if (argc == 3) {
	Alternate_server = argv[2];
    }

    printf("\nBullet server to be tested is %s.\n\n", server);

    car = 'Z';

    verbose = 0;
    parallel = 1;

    while (car != 'z') {
	for (i = 0; i <= NUMBER_OF_TESTS; i++) {
	    opt[i] = FALSE;
	    result[i] = 0;
	}
	do {
	    got_opt = FALSE;
	    printf("\nPossible tests are:\n\n");
	    for (i = 0; i < NUMBER_OF_TESTS; i++) {
		printf("  %c - %s\n", 'a' + i, test[i].test_fun_name);
	    }
	    printf("  x - all tests\n\n");

	    printf("  s - seqential mode %s\n", parallel ? "on" : "off");
	    printf("  v - verbose mode %s\n", verbose ? "off" : "on");
	    printf("  z - exit\n\n\n");
	    printf("Tests to perform (e.g. abc) ==> ");
	    scanf("%40s", numar);
	    for (i = 0; numar[i] != '\0'; i++) {
		if ((numar[i] >= 'a') && (numar[i] <= 'n')) {
		    opt[numar[i] - 'a'] = (got_opt = TRUE);
		} else if (numar[i] == 's') {
		    parallel = !parallel;
		} else if (numar[i] == 'v') {
		    verbose = !verbose;
		} else if (numar[i] == 'x') {
		    opt[NUMBER_OF_TESTS] = (got_opt = TRUE);
		} else if (numar[i] == 'z') {
		    exit(1);
		}
	    }
	} while (!got_opt);

	if (parallel) {
	    printf("Number of threads per test = ");
	} else {
	    printf("Number of runs = ");
	}
	scanf("%s", numar);
	l = atoi(numar);
	if (l == 0) {
	    break;
	}

	if (parallel) {
	    perform_parallel_tests(opt, l);
	} else {
	    perform_serial_tests(opt, l);
	}
    }

    exit(0);
    /*NOTREACHED*/
} /* main */
