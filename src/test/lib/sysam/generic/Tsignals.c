/*	@(#)Tsignals.c	1.4	94/04/06 17:43:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tsignals.c
 *
 * Original	: Feb 26, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Test suite for light-weight signals between threads.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	<signal.h>
#include	<unistd.h>
#include	<amtools.h>
#include	<test.h>
#include	<thread.h>
#include	<fault.h>
#include	<module/signals.h>


/* General stuff. Should go into a separate header file sometime. */
#define	PRIVATE	static
#define PUBLIC
#define EXTERN	extern

#define TRUE	1
#define FALSE	0


typedef	int		bool;
typedef	unsigned char	byte;

typedef void (*catcher_t) _ARGS((signum, thread_ustate *, void *));
#define NIL_CATCHER	((catcher_t) NULL)


/* Some global flags controlling the output and tests. */
PRIVATE	bool	verbose   = FALSE;	/* Give lots of info on tests. */
PRIVATE	bool	goodf	  = FALSE;	/* Only test with valid arguments. */
PRIVATE bool	badf	  = FALSE;	/* Only test with invalid arguments. */

/* The name of the current test_procedure. */
PRIVATE char	testname[32] = "";


/* Some user defined signals. */
#define	SIG_TEST	(signum)0x10000
#define	SIG_SIG		(signum)0x10001
#define	SIG_STOP	(signum)0x10002

#define SSIZE		1024	/* Stack size for sender thread. */

/* Signal to be raised by sender thread. */
PRIVATE	signum	glo_send_sig = (signum)0;

/* Signal caught by main thread and extra parameter passed to catcher. */
PRIVATE	signum	glo_caught_sig1 = (signum)0;
PRIVATE	char	*glo_caught_extra1 = NULL;

/* Signal caught by sender thread and extra parameter passed to catcher. */
PRIVATE	signum	glo_caught_sig2 = (signum)0;
PRIVATE	char	*glo_caught_extra2 = NULL;

/* Flags to control actions of sender thread. */
PRIVATE	bool	glo_raise;
PRIVATE	bool	glo_cont;

EXTERN	void	threadswitch();




PRIVATE	bool	test_good(err, testcode)
/* Checks if a tested function call returned no error.
 * Returns TRUE if 'err' == STD_OK.
 */

errstat	err;
char	testcode[];
{
  TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
		("%s, %s (%s)\n", testname, testcode, err_why(err)));
  if (err == STD_OK) {
    if (verbose)
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else
    return FALSE;

}  /* test_good() */




PRIVATE	bool	test_bad(err, exp, testcode, argname)
/* Checks if a tested function call returned an error.
 * Returns TRUE if 'err' != STD_OK.
 */

errstat	err, exp;
char	testcode[], argname[];
{
  TEST_ASSERT(err != STD_OK, TEST_SERIOUS,
	("%s, %s (no error for %s)\n", testname, testcode, argname));
  if (err != STD_OK) {
    if (exp != STD_OK)
      TEST_ASSERT(err == exp, TEST_SERIOUS,
	("%s, %s (wrong error for %s)\n", testname, testcode, argname));

    if (verbose) {
      if (err != exp && exp != STD_OK)
	printf("expected error: %s    returned error: %s\n",
						err_why(exp), err_why(err));
      else
	printf("passed: %s, %s (%s)\n", testname, testcode, err_why(err));
    }

    return TRUE;
  }
  else
    return FALSE;

}  /* test_bad() */




/*ARGSUSED*/
PRIVATE void	send_catcher(sig, us, extra)
/* Catches signals for the sender. */

signum		sig;
thread_ustate	*us;
_VOIDSTAR	extra;
{
  glo_caught_sig2 = sig;
  glo_caught_extra2 = extra;

  if (sig == SIG_SIG)
    /* Set flag to raise a signal. */
    glo_raise = TRUE;
  else {
    /* Don't raise any signal. */
    glo_raise = FALSE;

    if (sig == SIG_STOP)
      /* Exit loop and thread. */
      glo_cont = FALSE;
  }

}  /* send_catcher() */




/*ARGSUSED*/
PRIVATE void	test_catcher(sig, us, extra)
/* Catches signals for test-functions. */

signum		sig;
thread_ustate	*us;
_VOIDSTAR	extra;
{
  glo_caught_sig1 = sig;
  glo_caught_extra1 = extra;

}  /* test_catcher() */




/*ARGSUSED*/
PRIVATE void sig_sender(param, size)
/* (Thread) Send a signal when told to do so. */

char	*param;
int	size;
{
  errstat	err;


  glo_cont = FALSE;

  /* Set up catchers for some signals and exceptions. */
  err = sig_catch(SIG_SIG, send_catcher, (_VOIDSTAR) testname);
  if (err == STD_OK) {
    err = sig_catch(SIG_STOP, send_catcher, (_VOIDSTAR) testname);
    if (err == STD_OK) {
      err = sig_catch((signum)-1, send_catcher, (_VOIDSTAR) testname);
      if (err == STD_OK) {
        err = sig_catch(SIG_TEST, send_catcher, (_VOIDSTAR) testname);
        if (err == STD_OK)
          err = sig_catch(SIG_TEST, NIL_CATCHER, (_VOIDSTAR) NULL);
      }
    }
  }

  if (err == STD_OK) {
    glo_cont = TRUE;
    while (glo_cont) {
      /* Wait for signal from other thread. */
      (void)pause();

      /* Other thread woke me up. */
      if (glo_raise && glo_send_sig != (signum)0)
        /* Have to raise signal for main thread. */
        sig_raise(glo_send_sig);
    } 
  }
}  /* sig_sender() */




PRIVATE void	test_sig_uniq()

{
  long		l;
  signum	sig;


  (void)strcpy(testname, "test_sig_uniq()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Check first 2^16 calls to 'sig_uniq()'. */
  for (l = 0x10000L; l < 0x20000L; l++)
    if ((sig = sig_uniq()) != (signum)l)
      break;

  TEST_ASSERT(l == 0x20000L, TEST_WARNING,
	("%s (%ld expected but %ld returned)\n", testname, l, (long)sig));

}  /* test_sig_uniq() */




PRIVATE void	test_sig_catch()

{
  errstat	err;


  (void)strcpy(testname, "test_sig_catch()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Try catching a user defined signal. */
  glo_send_sig = SIG_TEST;

  /* Try 'sig_catch()' with catcher and extra argument. */
  err = sig_catch(SIG_TEST, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1a")) {
    /* Clear global variables. */
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = NULL;

    /* Wake up sender thread. */
    sig_raise(SIG_SIG);

    /* Allow sender thread to raise signal. */
    (void)sleep(2);

    TEST_ASSERT(glo_caught_sig1 == SIG_TEST, TEST_SERIOUS,
		("%s, 1a (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_TEST, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1a (wrong extra parameter passed)\n", testname));
  }

  /* Try 'sig_catch()' with catcher and no extra argument. */
  err = sig_catch(SIG_TEST, test_catcher, (_VOIDSTAR) NULL);
  if (test_good(err, "1b")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = testname;
    sig_raise(SIG_SIG);
    (void)sleep(2);
    TEST_ASSERT(glo_caught_sig1 == SIG_TEST, TEST_SERIOUS,
		("%s, 1b (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_TEST, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == NULL, TEST_SERIOUS,
		("%s, 1b (wrong extra parameter passed)\n", testname));
  }

  /* Try 'sig_catch()' with no catcher and no extra argument (i.e. ignore). */
  err = sig_catch(SIG_TEST, NIL_CATCHER, (_VOIDSTAR) NULL);
  if (test_good(err, "1c")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = testname;
    sig_raise(SIG_SIG);
    (void)sleep(2);

    /* The signal sent by the sender thread should have been ignored. */
    TEST_ASSERT(glo_caught_sig1 == (signum)0, TEST_SERIOUS,
		("%s, 1c (no signal expected but %ld caught)\n",
				testname, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1c (no extra parameter expected)\n", testname));
  }

  /* Try catching an exception. */
  glo_send_sig = (signum)-1;

  err = sig_catch((signum)-1, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1e")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_sig2 = (signum)0;
    glo_caught_extra1 = NULL;
    glo_caught_extra2 = NULL;
    sig_raise(SIG_SIG);
    (void)sleep(2);

    /* User generated exceptions are broadcast like any other signal but
     * they will kill any process if not caught by all threads.
     */
    TEST_ASSERT(glo_caught_sig1 == (signum)-1, TEST_SERIOUS,
		("%s, 1e (signal -1 expected but %ld caught)\n",
				testname, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1e (wrong extra parameter passed)\n", testname));
    TEST_ASSERT(glo_caught_sig2 == (signum)-1, TEST_SERIOUS,
		("%s, 1e (signal -1 expected but %ld caught)\n",
				testname, (long)glo_caught_sig2));
    TEST_ASSERT(glo_caught_extra2 == testname, TEST_SERIOUS,
		("%s, 1e (wrong extra parameter passed)\n", testname));

    /* Ignore exception. */
    err = sig_catch((signum)-1, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1ea");
  }

  /* Try catching the Unix signal 'SIGKILL'. Real Unix won't allow that. */
  glo_send_sig = SIGKILL;

  err = sig_catch((signum) SIGKILL, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1h")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = NULL;
    sig_raise(SIG_SIG);
    (void)sleep(2);
    TEST_ASSERT(glo_caught_sig1 == SIGKILL, TEST_SERIOUS,
		("%s, 1h (signal %ld expected but %ld caught)\n",
			testname, (long)SIGKILL, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1h (wrong extra parameter passed)\n", testname));

    /* Ignore signal. */
    err = sig_catch((signum) SIGKILL, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1ha");
  }

}  /* test_sig_catch() */




PRIVATE void	test_sig_raise()

{
  errstat	err;


  (void)strcpy(testname, "test_sig_raise()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Sender thread shouldn't generate a signal; just catch one. */
  glo_send_sig = (signum)0;

  /* Try raising a signal for both the main and sender thread. */
  err = sig_catch(SIG_SIG, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1a")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_sig2 = (signum)0;
    glo_caught_extra1 = NULL;
    glo_caught_extra2 = NULL;
    sig_raise(SIG_SIG);

    /* All threads should have received the signal. */
    TEST_ASSERT(glo_caught_sig1 == SIG_SIG, TEST_SERIOUS,
		("%s, 1aa (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_SIG, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1aa (wrong extra parameter passed)\n", testname));

    /* Allow sender thread to catch signal. */
    threadswitch();

    TEST_ASSERT(glo_caught_sig2 == SIG_SIG, TEST_SERIOUS,
		("%s, 1ab (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_SIG, (long)glo_caught_sig2));
    TEST_ASSERT(glo_caught_extra2 == testname, TEST_SERIOUS,
		("%s, 1ab (wrong extra parameter passed)\n", testname));

    /* Ignore signal meant for sender thread. */
    err = sig_catch(SIG_SIG, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1ac");
  }

  /* Some distiguishable signal. */
  glo_send_sig = SIGKILL;

  /* Try raising a signal just for the main thread. */
  err = sig_catch(SIG_TEST, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1b")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_sig2 = (signum)0;
    glo_caught_extra1 = NULL;
    glo_caught_extra2 = NULL;
    sig_raise(SIG_TEST);
    TEST_ASSERT(glo_caught_sig1 == SIG_TEST, TEST_SERIOUS,
		("%s, 1b (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_TEST, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1b (wrong extra parameter passed)\n", testname));

    /* Switch to sender thread. */
    threadswitch();

    TEST_ASSERT(glo_caught_sig2 == (signum)0, TEST_SERIOUS,
		("%s, 1b (no signal expected but %ld caught)\n",
				testname, (long)glo_caught_sig2));
    TEST_ASSERT(glo_caught_extra2 == NULL, TEST_SERIOUS,
		("%s, 1b (no extra parameter expected)\n", testname));

    /* Ignore signal. */
    err = sig_catch(SIG_TEST, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1ba");
  }

  /* Sender thread shouldn't generate a signal; just catch one. */
  glo_send_sig = (signum)0;

  /* Try raising a signal just for the sender thread. */
  glo_caught_sig1 = (signum)0;
  glo_caught_sig2 = (signum)0;
  glo_caught_extra1 = testname;
  glo_caught_extra2 = NULL;
  sig_raise(SIG_SIG);
  TEST_ASSERT(glo_caught_sig1 == (signum)0, TEST_SERIOUS,
		("%s, 1c (no signal expected but %ld caught)\n",
				testname, (long)glo_caught_sig1));
  TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1c (no extra parameter expected)\n", testname));

  /* Allow sender thread to catch signal. */
  threadswitch();

  TEST_ASSERT(glo_caught_sig2 == SIG_SIG, TEST_SERIOUS,
		("%s, 1c (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_SIG, (long)glo_caught_sig2));
  TEST_ASSERT(glo_caught_extra2 == testname, TEST_SERIOUS,
		("%s, 1c (wrong extra parameter passed)\n", testname));

}  /* test_sig_raise() */




PRIVATE void	test_sig_block()

{
  errstat	err;


  (void)strcpy(testname, "test_sig_block()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Try blocking a normal signal. */
  glo_send_sig = SIG_TEST;

  err = sig_catch(SIG_TEST, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1c")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = NULL;
    sig_block();
      sig_raise(SIG_SIG);
      (void)sleep(2);

      /* Signals are still blocked. */
      TEST_ASSERT(glo_caught_sig1 == (signum)0, TEST_SERIOUS,
		("%s, 1c (no signal expected but %ld caught)\n",
				testname, (long)glo_caught_sig1));
      TEST_ASSERT(glo_caught_extra1 == NULL, TEST_SERIOUS,
		("%s, 1c (no extra parameter expected)\n", testname));
    sig_unblock();

    /* The unblocked signal should have been caught now. */
    TEST_ASSERT(glo_caught_sig1 == SIG_TEST, TEST_SERIOUS,
		("%s, 1c (signal %ld expected but %ld caught)\n",
			testname, (long)SIG_TEST, (long)glo_caught_sig1));
    TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1c (wrong extra parameter passed)\n", testname));

    /* Ignore signal. */
    err = sig_catch(SIG_TEST, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1ca");
  }

  /* Try to block an exception. Shouldn't be possible. */
  glo_send_sig = (signum)-1;

  err = sig_catch((signum)-1, test_catcher, (_VOIDSTAR) testname);
  if (test_good(err, "1d")) {
    glo_caught_sig1 = (signum)0;
    glo_caught_extra1 = NULL;
    sig_block();
      sig_raise(SIG_SIG);
      (void)sleep(2);

      /* Exceptions cannot be blocked. */
      TEST_ASSERT(glo_caught_sig1 == (signum)-1, TEST_SERIOUS,
		("%s, 1d (signal -1 expected but %ld caught)\n",
			testname, (long)glo_caught_sig1));
      TEST_ASSERT(glo_caught_extra1 == testname, TEST_SERIOUS,
		("%s, 1d (wrong extra parameter passed)\n", testname));
    sig_unblock();

    err = sig_catch((signum)-1, NIL_CATCHER, (_VOIDSTAR) NULL);
    (void)test_good(err, "1da");
  }

}  /* test_sig_block() */




PRIVATE void	test_bad_signum()

{
  errstat	err;


  (void)strcpy(testname, "test_bad_signum()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  glo_send_sig = (signum)0;

  glo_caught_sig1 = SIG_TEST;
  glo_caught_extra1 = NULL;
  err = sig_catch((signum)0, test_catcher, (_VOIDSTAR) testname);
  (void)test_bad(err, STD_ARGBAD, "2a", "signal number 0");


}  /* test_bad_signum() */




PRIVATE	int	usage(progname)
/* Print a message about how to use the test. */

char	progname[];
{
  fprintf(stderr, "Usage: %s [-vgb]\n", progname);
  exit(1);

}  /* usage() */




PRIVATE bool	test_init()

{
  (void)strcpy(testname, "test_init()");
  if (verbose)  printf("\n----  %s  ----\n", testname);


  if (!thread_newthread(sig_sender, SSIZE, (char *) NULL, 0)) {
    TEST_ASSERT(FALSE, TEST_SERIOUS, 
	("%s, 1a (couldn't start thread): test aborted\n", testname));
    return FALSE;
  }

  (void)sleep(2);
  TEST_ASSERT(glo_cont, TEST_SERIOUS,
	("%s, 1b (sender thread stopped): test aborted\n", testname));

  return glo_cont;
 
}  /* test_init() */




PRIVATE void	test_cleanup()

{
  (void)strcpy(testname, "test_cleanup()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  sig_raise(SIG_STOP);
 
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
      test_sig_uniq();
      test_sig_catch();
      test_sig_raise();
      test_sig_block();
    }

    if (badf) {
      test_bad_signum();
    }

    test_cleanup();
  }
  TEST_END();
  /*NOTREACHED*/
}  /* main() */

