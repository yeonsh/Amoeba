/*	@(#)Tbullet.c	1.2	94/04/06 17:35:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tbullet.c
 *
 * Original	: Nov 22, 1990.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Test suite for the Bullet file server.
 *
 * Update 1	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Rearranged over several source files.
 *
 * Update 2	:
 * Author(s)	:
 * Description	:
 */




/* __TBULLET_C__ should be defined before including "Tbullet.h". */
#define	__TBULLET_C__
#include	"Tbullet.h"
#include	"random/random.h"
#include	"module/rnd.h"


/* Some global flags controlling the output and tests. */
PUBLIC	bool	verbose	= FALSE;	/* Give lots of info on tests. */
/* These are not needed outside this file. */
PRIVATE	bool	goodf	= FALSE;	/* Only test with valid arguments. */
PRIVATE bool	badf	= FALSE;	/* Only test with invalid arguments. */
PRIVATE bool	quick	= FALSE;	/* Skip tests taking much time. */

/* The name of the current test_procedure. */
PUBLIC char	testname[32] = "";

/* Global stuff to be used by all, if everybody cleans up after himself. */
#define MAXBADCAPS	6

PUBLIC	int	nbadcaps = 0;		/* Actual nr. of bad capabilities. */
PUBLIC	char	read_buf[BIG_SIZE];	/* Buffer for reading data. */
PUBLIC	char	write_buf[AVG_SIZE+1];	/* Buffer for writing data. */
PUBLIC	errstat	cap_err[MAXBADCAPS];	/* Expected errors for bad caps. */

PUBLIC	capability	svr_cap, empty_cap, one_cap, commit_cap, uncommit_cap,
			bad_caps[MAXBADCAPS];




PUBLIC	bool	test_ok(err, testcode)
/* Checks the error code returned by supporting function calls.
 * Returns FALSE if 'err' != STD_OK and (sub)test has to be aborted.
 */

errstat	err;
char	testcode[];
{
  TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
    ("%s, %s (%s, test aborted)\n", testname, testcode, err_why(err)));
  if (err == STD_OK) {
    if (verbose) 
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else 
    return FALSE;

}  /* test_ok() */




PUBLIC	bool	test_good(err, testcode)
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




PUBLIC	bool	test_bad(err, exp, testcode, argname)
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




PUBLIC	bool	obj_cmp(cap1, cap2)
/* Checks if two capabilities refer to the same object. */

capability	*cap1, *cap2;
{
  return PORTCMP(&cap1->cap_port, &cap2->cap_port) &&
		  prv_number(&cap1->cap_priv) == prv_number(&cap2->cap_priv);

}  /* obj_cmp() */




PRIVATE	int	usage(progname)
/* Print a message about how to use the test. */

char	progname[];
{
  fprintf(stderr, "Usage: %s [-vgbq] [servername]\n", progname);
  exit(1);

}  /* usage() */




PRIVATE int	make_badcaps()

{
  int		i, n = 0;
  errstat	err;
  capability	tmp_cap;


  (void)strcpy(testname, "make_badcaps()");
  if (verbose)  printf("\n----  %s  ----\n", testname);


  /* Generate a few bad capabilities from the server capability. */
  for (i = 0; i < MAXBADCAPS; i++)
    bad_caps[i] = commit_cap;

  /* A capability for a non-existing file. */
  err = b_create(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &tmp_cap);
  if (test_good(err, "b_create()")) {
    if (test_good(std_destroy(&tmp_cap), "std_destroy()")) {
      bad_caps[n] = tmp_cap;
      cap_err[n++] = STD_CAPBAD;
    }
  }

  /* A capability with corrupted rights. */ 
  bad_caps[n].cap_priv.prv_rights ^= BS_RGT_READ;
  cap_err[n++] = STD_CAPBAD;

   /* A capability with a corrupted protection field. */
  err = std_restrict(&commit_cap, BS_RGT_ALL^BS_RGT_READ, &tmp_cap);
  if (test_good(err, "std_restrict()")) {
    bad_caps[n] = tmp_cap;
    bad_caps[n].cap_priv.prv_rights = one_cap.cap_priv.prv_rights;
    bad_caps[n].cap_priv.prv_random = one_cap.cap_priv.prv_random;

    cap_err[n++] = STD_CAPBAD;
  }

  /* A capability for a Soap directory. */
  err = name_lookup(".", &tmp_cap);
  if (test_good(err, "name_lookup()")) {
    bad_caps[n] = tmp_cap;
    cap_err[n++] = STD_COMBAD;
  }

  /* A capability with a NULL port. */
  for (i = 0; i < PORTSIZE; i++)
    bad_caps[n].cap_port._portbytes[i] = 0x00;
  cap_err[n++] = RPC_BADPORT;

  /* A capability with the wrong port-number. */
  if (!quick) {
    /* RPCs from bad borts have a long time-out. */
    for (i = 0; i < PORTSIZE; i++)
      bad_caps[n].cap_port._portbytes[i] =
				~commit_cap.cap_port._portbytes[PORTSIZE-i-1];
    cap_err[n++] = RPC_NOTFOUND;
  }

  return n;

}  /* make_badcaps() */




PRIVATE	bool	test_init()
/* Initialize the global variables. */

{
  b_fsize	s, j, n;
  errstat	err = STD_OK;
  bool		ok;


  (void)strcpy(testname, "test_init()");
  if (verbose)  printf("\n----  %s  ----\n", testname);


  /* Fill the write buffer with every possible byte-value. */
  j = 0;
  s = AVG_SIZE+ONE;
  while (s > 0 && err == STD_OK) {
    n = MIN((b_fsize)MAX_RANDOM, s);
    err = rnd_getrandom(write_buf + (int)j, (int)n);
    j += n; s -= n;
  }
  if (!test_ok(err, "rnd_getrandom()"))
    return FALSE;

  /* Create an empty file. */
  err = b_create(&svr_cap, write_buf, ZERO, SAFE_COMMIT, &empty_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&empty_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok)
    return FALSE;

 /* Create a file with one byte. */
  err = b_create(&svr_cap, write_buf, ONE, SAFE_COMMIT, &one_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&one_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok) {
    err = std_destroy(&empty_cap);
    (void)test_ok(err, "std_destroy()");
    return FALSE;
  }

  /* Create an average sized committed file. */
  err = b_create(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &commit_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&commit_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok) {
    err = std_destroy(&one_cap);
    (void)test_ok(err, "std_destroy()");
    err = std_destroy(&empty_cap);
    (void)test_ok(err, "std_destroy()");
    return FALSE;
  }

  /* Create an average sized uncommitted file. */
  err = b_create(&svr_cap, write_buf, AVG_SIZE, BS_SAFETY, &uncommit_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&uncommit_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok) {
    err = std_destroy(&commit_cap);
    (void)test_ok(err, "std_destroy()");
    err = std_destroy(&one_cap);
    (void)test_ok(err, "std_destroy()");
    err = std_destroy(&empty_cap);
    (void)test_ok(err, "std_destroy()");
    return FALSE;
  }

  if (badf)
    nbadcaps = make_badcaps();

  return TRUE;

}  /* test_init() */




PRIVATE	void	test_cleanup()

{
  bool	ok;


  (void)strcpy(testname, "test_cleanup()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Get rid of all the temporary files. */
  ok = (std_destroy(&uncommit_cap) == STD_OK);
  ok = (std_destroy(&commit_cap) == STD_OK) && ok;
  ok = (std_destroy(&one_cap) == STD_OK) && ok;
  ok = (std_destroy(&empty_cap) == STD_OK) && ok;

  TEST_ASSERT(ok, TEST_WARNING,
                ("%s (couldn't clean up properly)\n", testname));

}  /* test_cleanup() */




PUBLIC	int	main(argc, argv)

int	argc;
char	*argv[];
{
  char		c, *server;
  errstat	err;
  extern int	optind;


  /* Check the command-line  options. */
  while((c = getopt(argc, argv, "vgbq")) != EOF) {
    switch(c) {
      case 'v':	verbose	= TRUE;	break;
      case 'g':	goodf	= TRUE;	break;
      case 'b':	badf	= TRUE;	break;
      case 'q':	quick	= TRUE;	break;

      default:	usage(argv[0]);
    }
  }

  /* Do all the tests by default. */
  if (!goodf && !badf) 
    goodf = badf = TRUE;

  /* Decide which Bullet server to use. */
  if (optind == argc)
    server = DEF_BULLETSVR;
  else if (optind+1 == argc)
    server = argv[optind];
  else
    usage(argv[0]);

  /* Get the capability of the Bullet server. */
  if ((err = name_lookup(server, &svr_cap)) != STD_OK) {
    fprintf(stderr, "Couldn't get capability for server %s (%s).\n",
							server, err_why(err));
    exit(-1);
  }

  printf("Bullet server to be tested is %s.\n\n", server);

  TEST_BEGIN(argv[0]);
  if (test_init()) {
    if (goodf) {
      test_b_size();
      test_b_read();
      test_b_create();
      test_b_modify();
      test_b_delete();
      test_b_insert();
      test_combi();
    }

    if (badf) {
      test_bad_caps();
      test_uncomm_file();
      test_bad_rights();
      test_bad_size();
      test_bad_offsets();
      test_bad_many();
    }

    test_cleanup();
  }
  TEST_END();

}  /* main() */
