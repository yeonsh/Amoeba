/*	@(#)floatp.c	1.4	96/02/27 10:53:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * A simple test of the correctness of the floating-point implementation.
 * Eventually it should generate all sorts of exceptions and trap them.
 * It should also test the performance of the floating-point unit.
 */

#include <math.h>
#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "exception.h"
#include "module/syscall.h"
#include "module/signals.h"
#include "signal.h"


/*ARGSUSED*/
void
my_catcher(sig, us, extra)
signum sig;
thread_ustate *us;
_VOIDSTAR extra;
{
    printf("Got signal %s\n", exc_name(sig));
    exit(0);
}

/*ARGSUSED*/
void
siggle(sig)
int sig;
{
    printf("you pressed the interrupt key\n");
}

main(argc, argv)
int	argc;
char *	argv[];
{
    int		i;
    int		loopcnt;
    float	a;
    float	b;
    double	f1;
    double	f2;
    interval	t1;
    interval	t2;
    errstat	err;
    char *	p;

    if (argc != 2)
    {
	printf("usage: %s loopcount\n", argv[0]);
	exit(1);
    }
    signal(SIGINT, siggle);
    err = sig_catch((signum) EXC_FPE, my_catcher, (_VOIDSTAR) 0);
    if (err != STD_OK)
    {
	printf("signal catcher FPE could not be set: %s\n", err_why(err));
	exit(1);
    }
    err = sig_catch((signum) EXC_MEM, my_catcher, (_VOIDSTAR) 0);
    if (err != STD_OK)
    {
	printf("signal catcher MEM could not be set: %s\n", err_why(err));
	exit(1);
    }
    err = sig_catch((signum) EXC_DIV, my_catcher, (_VOIDSTAR) 0);
    if (err != STD_OK)
    {
	printf("signal catcher DIV could not be set: %s\n", err_why(err));
	exit(1);
    }

    loopcnt = atoi(argv[1]);
    if (loopcnt <= 0)
    {
	printf("%s: loopcount must be greater than zero\n", argv[0]);
	exit(1);
    }

    printf("Starting trivial fp test\n");
    a = 1.0;
    b = 9.0 + 1.0;
    b -= 3.5;
    b *= 3.2;
    b /= a;
    a = sqrt(b) * exp(1.0);

    printf("Starting floating point performance test.\n");
    t1 = sys_milli();
    f2 = 5.0;
    for (i = 0; i < loopcnt; i++)
    {
	f1 = sqrt(f2);
	f2 = log(f1);
	f1 = exp(f2);
	f1 = f2 * sin(f1);
	if (f1 == 0.0 || f2 < 1.0) {
	    f1 = b * f2 + 7.2;
	    f2 = 3.2 + sqrt(f1) - a;
	}
	f2 /= 1037.236;
	f1 /= 1100.2 * sqrt(f1);
	f1 = sin(f1);
	f2 = cos(f2);
    }
    t2 = sys_milli() - t1;
    printf("last value for f1 = %lf\n", f1);
    printf("last value for f2 = %lf\n", f2);
    printf("elapsed time for %d loops is %d ms\n", loopcnt, t2);

    /* Now some FP exception handling? */
    printf("Exception testing (expect FP exception or Reference to non-existent memory)\n");
    a = 0.0;
    b /= a;

    printf("b = %f\n", b);

    b = b * f2;
    p = 0;
    *p = 3;

    /* Shouldn't get here - the signal catcher should get us */
    printf("Failed to generate exception ?!\n");
    exit(1);
    /*NOTREACHED*/
}
