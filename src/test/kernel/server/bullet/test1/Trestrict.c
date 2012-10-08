/*	@(#)Trestrict.c	1.2	94/04/06 17:36:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File         : Trestrict.c
 *
 * Original     : July 19, 1991.
 * Author(s)    : Berend Jan Beugel (beugel@cs.vu.nl).
 * Description  : Special purpose test for funny right bits.
 *		  Makes three restricted capabilities for a file and
 *		  stores them on the Soap server for further inspection
 *		  with "dir -k".
 *
 * Update 1     :
 * Author(s)    :
 * Description  :
 */


#include        "test.h"
#include        "stdlib.h"
#include        "amtools.h"
#include        "amoeba.h"
#include        "ampolicy.h"
#include        "bullet/bullet.h"
#include        "cmdreg.h"
#include        "random/random.h"
#include        "stderr.h"

/* General stuff. Should go into a separate header file sometime. */
#define PRIVATE static
#define PUBLIC
#define EXTERN  extern

#define TRUE    1
#define FALSE   0


typedef int             bool;
typedef unsigned char   byte;


#define SAFE_COMMIT     (BS_COMMIT | BS_SAFETY)


/* The name of the current test_procedure. */
PRIVATE char    testname[32] = "";


PRIVATE	bool		verbose = FALSE;
PRIVATE	char		*fname;
PRIVATE	capability      svr_cap;




PRIVATE bool    test_ok(err, testcode)
/* Checks the error code returned by supporting function calls.
 * Returns FALSE if 'err' != STD_OK and (sub)test has to be aborted.
 */

errstat err;
char    testcode[];
{
  TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
    ("%s, %s (%s): test aborted\n", testname, testcode, err_why(err)));
  if (err == STD_OK) {
    if (verbose)
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else
    return FALSE;

}  /* test_ok() */




PRIVATE bool    test_good(err, testcode)
/* Checks if a tested function call returned no error.
 * Returns TRUE if 'err' == STD_OK.
 */

errstat err;
char    testcode[];
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




PRIVATE bool obj_cmp(cap1, cap2)
/* Checks if two capabilities refer to the same object. */

capability      *cap1, *cap2;
{
  int   i;


  for (i = 0; i < PORTSIZE; i++)
    if (cap1->cap_port._portbytes[i] != cap2->cap_port._portbytes[i])
      return FALSE;

  if (prv_number(&cap1->cap_priv) == prv_number(&cap2->cap_priv))
    return TRUE;
  else
    return FALSE;

}  /* obj_cmp() */





PRIVATE bool    test()
/* Initialize the global variables. */

{
  errstat       err;
  bool          ok, ok1;
  capability	commit_cap, new_cap;
  char		s[32];


  (void)strcpy(testname, "test()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Create a committed file. */
  err = b_create(&svr_cap, "\nTest.\n", (b_fsize)6, SAFE_COMMIT, &commit_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&commit_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));

    if (ok) {
      /* Restrict and store it in the current directory. */
      err = std_restrict(&commit_cap, ~BS_RGT_READ, &new_cap);
      if (test_good(err, "std_restrict()")) {
	sprintf(s, "%s1", fname);
        err = name_append(s, &new_cap);
        ok = test_good(err, "name_append()");
      }

      /* Restrict and store it in the current directory. */
      err = std_restrict(&commit_cap, (rights_bits)~BS_RGT_READ, &new_cap);
      if (test_good(err, "std_restrict()")) {
	sprintf(s, "%s2", fname);
        err = name_append(s, &new_cap);
        ok1 = test_good(err, "name_append()");
        ok = ok || ok1;
      }

      /* Restrict and store it in the current directory. */
      err = std_restrict(&commit_cap, (rights_bits)(BS_RGT_ALL^BS_RGT_READ),
								&new_cap);
      if (test_good(err, "std_restrict()")) {
	sprintf(s, "%s3", fname);
        err = name_append(s, &new_cap);
        ok1 = test_good(err, "name_append()");
        ok = ok || ok1;
      }
    }

    if (!ok) {
      err = std_destroy(&commit_cap);
      (void)test_ok(err, "std_destroy()");
    }
  }

}  /* test() */




PRIVATE int     usage(progname)
/* Print a message about how to use the test. */

char    progname[];
{
  fprintf(stderr, "Usage: %s [-v] [-s servername] filename \n", progname);
  exit(1);

}  /* usage() */




PUBLIC  int     main(argc, argv)

int     argc;
char    *argv[];
{
  char          c, *server;
  errstat       err;
  extern int    optind;


  server = DEF_BULLETSVR;

  /* Check the command-line  options. */
  while((c = getopt(argc, argv, "vs:")) != EOF) {
    switch(c) {
      case 'v': verbose	= TRUE;    break;
      case 's': server	= optarg;  break;

      default:  usage(argv[0]);
    }
  }


  /* Get the filename to be used. */
  if (optind == argc)
    usage(argv[0]);
  else if (optind+1 == argc)
    fname = argv[optind];
  else
    usage(argv[0]);

  /* Get the capability of the Bullet server. */
  if ((err = name_lookup(server, &svr_cap)) != STD_OK) {
    fprintf(stderr, "Couldn't get capability for server %s (%s).\n",
                                                        server, err_why(err));
    exit(-1);
  }
  if (verbose)
    printf("\nBullet server to be tested is %s.\n\n", server);

  TEST_BEGIN(argv[0]);
  test();
  TEST_END();

}  /* main() */

