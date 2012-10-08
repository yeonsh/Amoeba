/*	@(#)Tsignal.c	1.2	94/04/06 17:42:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tsignal.c
 *
 * Original	: Mar 14, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Test suite for Posix signals between processes.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<test.h>


/* General stuff. Should go into a separate header file sometime. */
#define	PRIVATE	static
#define PUBLIC
#define EXTERN	extern

#define TRUE	1
#define FALSE	0
#define OK	0
#define ERR	-1


typedef	int		bool;
typedef	unsigned char	byte;


/* Some global flags controlling the output and tests. */
PRIVATE	bool	verbose   = FALSE;	/* Give lots of info on tests. */
PRIVATE	bool	goodf	  = FALSE;	/* Only test with valid arguments. */
PRIVATE bool	badf	  = FALSE;	/* Only test with invalid arguments. */

/* The name of the current test_procedure. */
PRIVATE char	testname[32] = "";


PRIVATE	int	glo_parent	= 0;
PRIVATE	int	glo_child	= 0;
PRIVATE	int	glo_caught_sig	= 0;

/* Externally defined variables. */
EXTERN	int	sys_nerr;
EXTERN	char	*sys_errlist[];
EXTERN	int	errno;




PRIVATE	bool	test_ok(err, testcode)
/* Checks the error code returned by supporting function calls.
 * Returns FALSE if 'err' == ERR and (sub)test has to be aborted.
 */

int	err;
char	testcode[];
{
  char	*s;


  s = errno < sys_nerr ? sys_errlist[errno] : "unknown error";

  TEST_ASSERT(err != ERR, TEST_SERIOUS,
		("%s, %s (%s, test aborted)\n", testname, testcode, s));
  if (err != ERR) {
    if (verbose) 
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else
    return FALSE;

}  /* test_ok() */




PRIVATE	bool	test_good(err, testcode)
/* Checks if a tested function call returned no error.
 * Returns TRUE if 'err' != ERR.
 */

int	err;
char	testcode[];
{
  char	*s;


  s = errno < sys_nerr ? sys_errlist[errno] : "unknown error";

  TEST_ASSERT(err != ERR, TEST_SERIOUS,
		("%s, %s (%s)\n", testname, testcode, s));
  if (err != ERR) {
    if (verbose)
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else
    return FALSE;

}  /* test_good() */




PRIVATE	bool	test_bad(err, exp, testcode, argname)
/* Checks if a tested function call returned an error.
 * Returns TRUE if 'err' == ERR.
 */

int	err, exp;
char	testcode[], argname[];
{
  char	*serr, *sexp;


  TEST_ASSERT(err == ERR, TEST_SERIOUS,
	("%s, %s (no error for %s)\n", testname, testcode, argname));
  if (err == ERR) {
    if (exp != OK)
      TEST_ASSERT(errno == exp, TEST_SERIOUS,
	("%s, %s (wrong error for %s)\n", testname, testcode, argname));

    if (verbose) {
      serr = errno < sys_nerr ? sys_errlist[errno] : "unknown error";
      sexp = exp < sys_nerr ? sys_errlist[exp] : "unknown error";

      if (errno != exp && exp != OK)
	printf("expected error: %s    returned error: %s\n", sexp, serr);
      else
	printf("passed: %s, %s (%s)\n", testname, testcode, serr);
    }

    return TRUE;
  }
  else
    return FALSE;

}  /* test_bad() */




PRIVATE void	handler(sig)
/* This is the signal handler. It only sets a flag. */

int	sig;
{
  glo_caught_sig = sig;

}  /* handler() */




PRIVATE void	test_signal()

{
  void	(*prev)();
  long	mask;


  (void)strcpy(testname, "test_signal()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  if (test_good((int)(prev = signal(SIGINT, handler)), "1a")) {
    TEST_ASSERT(prev == SIG_DFL, TEST_SERIOUS,
		("%s, 1a (SIG_DFL expected)\n", testname));
    glo_caught_sig = 0;
    if (test_good(kill(glo_child, SIGUSR1), "1a")) {
      TEST_ASSERT(sleep(5) > 0, TEST_SERIOUS,
		("%s, 1a (overslept)\n", testname));
      TEST_ASSERT(glo_caught_sig == SIGINT, TEST_SERIOUS,
		("%s, 1a (signal %d expected but %d caught)\n",
					testname, SIGINT, glo_caught_sig));
    }
  }

  mask = sigblock((long) sigmask(SIGINT));
  glo_caught_sig = 0;
  if (test_good(kill(glo_child, SIGUSR1), "1b")) {
    TEST_ASSERT(sleep(2) == 0, TEST_SERIOUS,
		("%s, 1b (didn't sleep well)\n", testname));
    TEST_ASSERT(glo_caught_sig == 0, TEST_SERIOUS,
		("%s, 1b (no signal expected but %d caught)\n",
						testname, glo_caught_sig));
  }

  (void)sigsetmask(mask);
  glo_caught_sig = 0;
  if (test_good(kill(glo_child, SIGUSR1), "1c")) {
    TEST_ASSERT(sleep(5) > 0, TEST_SERIOUS,
		("%s, 1c (overslept)\n", testname));
    TEST_ASSERT(glo_caught_sig == SIGINT, TEST_SERIOUS,
		("%s, 1c (signal %d expected but %d caught)\n",
					testname, SIGINT, glo_caught_sig));
  }

  if (test_good((int)(prev = signal(SIGINT, SIG_IGN)), "1d")) {
    TEST_ASSERT(prev == handler, TEST_SERIOUS,
		("%s, 1d (&handler() expected)\n", testname));
    glo_caught_sig = 0;
    if (test_good(kill(glo_child, SIGUSR1), "1d")) {
      TEST_ASSERT(sleep(2) == 0, TEST_SERIOUS,
		("%s, 1d (didn't sleep well)\n", testname));
      TEST_ASSERT(glo_caught_sig == 0, TEST_SERIOUS,
		("%s, 1d (no signal expected but %d caught)\n",
						testname, glo_caught_sig));
    }
  }

}  /* test_signal() */




PRIVATE void	test_bad_signal()

{
  (void)strcpy(testname, "test_bad_signal()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  (void)test_bad((int)signal(SIGKILL,handler), EINVAL, "2a", "signal SIGKILL");
  (void)test_bad((int)signal(SIGKILL,SIG_IGN), EINVAL, "2b", "signal SIGKILL");
  (void)test_bad((int)signal(SIGSTOP,handler), EINVAL, "2c", "signal SIGSTOP");
  (void)test_bad((int)signal(SIGSTOP,SIG_IGN), EINVAL, "2d", "signal SIGSTOP");
  (void)test_bad((int)signal(32,handler), EINVAL, "2e", "non_existing signal");

}  /* test_bad_signal() */




PRIVATE	int	usage(progname)
/* Print a message about how to use the test. */

char	progname[];
{
  fprintf(stderr, "Usage: %s [-vgb]\n", progname);
  exit(1);

}  /* usage() */




PRIVATE bool	test_init()

{
  void	(*prev)();


  (void)strcpy(testname, "test_init()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  glo_child = fork();
  if (test_ok(glo_child, "1a"))
    if (glo_child == 0) {
      /* printf(" --- child: catching signal\n"); */
      prev = signal(SIGUSR1, handler);
      if ((int)prev != ERR) {
        glo_parent = getppid();
        while (TRUE) {
	  printf(" --- child: pausing\n");
	  (void)pause();	/* Wait for signal from parent. */
          (void)sleep(1);	/* Give parent some extra time. */
	  printf(" --- child: signalling parent\n");
          if (kill(glo_parent, SIGINT) == ERR)
            break;
        }
      }
      exit(0);
      /*NOTREACHED*/
    }
    else 
      return TRUE;
  else
    return FALSE;

}  /* test_init() */




PRIVATE void	test_cleanup()

{
  (void)strcpy(testname, "test_cleanup()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  (void)test_ok(kill(glo_child, SIGKILL), "1a");
 
}  /* test_cleanup() */




main(argc, argv)

int	argc;
char	*argv[];
{
  char		c;
  extern int	optind;


  /* Check the command-line  options. */
  while((c = getopt(argc, argv, "vgb")) != EOF) {
    switch(c) {
      case 'v': verbose   = TRUE;  break;
      case 'g': goodf     = TRUE;  break;
      case 'b': badf      = TRUE;  break;

      default:  usage(argv[0]);
    }
  }

  /* Do all the tests by default. */
  if (!goodf && !badf) 
    goodf = badf = TRUE;


  TEST_BEGIN(argv[0]);
  if (test_init()) {
    if (goodf) {
      test_signal();
    }

    if (badf) {
      test_bad_signal();
    }

    test_cleanup();
  }
  TEST_END();
  /*NOTREACHED*/
}  /* main() */

