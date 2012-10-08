/*	@(#)Tsoap.c	1.3	96/02/27 10:53:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tsoap.c
 *
 * Original	: Oct 19, 1990.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Test suite for the Soap directory server.
 *
 * Update 1	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Rearranged over multiple files.
 *
 * Update 2	:
 * Author(s)	:
 * Description	:
 */


/* __TSOAP_C__ should be defined before including "Tbullet.h". */
#define	__TSOAP_C__
#include	"Tsoap.h"

#include	"sp_dir.h"
#include	"bullet/bullet.h"
#include	"module/cap.h"


/* Some local definitions used during intialisation. */
#define	MAXBADCAPS	6
#define MAXRGTCAPS	7

#define DEF_MASK	(SP_DELRGT | SP_MODRGT | 0x0F)
#define DEL_MASK	(SP_DELRGT | 0x02)
#define MOD_MASK	(SP_MODRGT | 0x02)
#define READ_MASK	0x02
#define NOCOL_MASK	(SP_DELRGT | SP_MODRGT)
#define BADCOL_MASK	(SP_DELRGT | SP_MODRGT | 0x0C)


/* Some global flags controlling the output and tests. */
PUBLIC	bool    verbose = FALSE;      /* Give lots of info on tests. */

/* These are not needed outside this file. */
PRIVATE bool    goodf   = FALSE;      /* Only test with valid arguments. */
PRIVATE bool    badf    = FALSE;      /* Only test with invalid arguments. */
PRIVATE bool    quick   = FALSE;      /* Skip tests taking much time. */

/* The name of the current test_procedure. */
PUBLIC	char    testname[32] = "";

/* The path to the CWD. */
PUBLIC	char	*testpath;

/* A few column headers. */
PUBLIC	char	*no_colnames[]   = {NULL};
PUBLIC	char	*one_colname[]	 = {"one", NULL};
PUBLIC	char	*def_colnames[]  = {"one", "two", NULL};
PUBLIC	char	*all_colnames[]  = {"1", "2", "3", "4", NULL};
PUBLIC	char	*many_colnames[] = {"1", "2", "3", "4", "5", NULL};

/* Column masks arrays. */
PUBLIC	long	empty_masks[]	= {0x00, 0x00, 0x00, 0x00};
PUBLIC	long	full_masks[]	= {0xFF, 0xFF, 0xFF, 0xFF};
PUBLIC	long	def_masks[]	= {0xFF, DEF_MASK, 0x55, 0xAA, 0x00};
PUBLIC	long	rgt_masks[MAXRGTCAPS][1] =
  {
    {DEL_MASK}, {MOD_MASK}, {READ_MASK},
    {NOCOL_MASK}, {BADCOL_MASK}, {0x00}, {DEF_MASK}
  };

/* Directory names. */
PUBLIC	char	def_name[]  = "TeStDiReCtOrY";
PUBLIC	char	aux_name[]  = "PrLwYtZkOfSkY";
PUBLIC	char	long_name[] = "AbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnO";
PUBLIC	char	bad_name[]  = "AbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOpAbCdEfGhIjKlMnOp";


/* A few capability sets. */
PUBLIC	capset		dir_capset, def_capset, aux_capset;
PUBLIC	capset		bad_capsets[MAXBADCAPS], rgt_capsets[MAXRGTCAPS];
PUBLIC	errstat		cap_err[MAXBADCAPS];
PUBLIC	int		nbadcaps = 0;
PRIVATE	capability	bullet_cap;




PUBLIC	bool    test_ok(err, testcode)
/* Checks the error code returned by supporting function calls.
 * Returns FALSE if 'err' != STD_OK and (sub)test should be aborted.
 */

errstat err;
char    testcode[];
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




PUBLIC	bool    test_good(err, testcode)
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




PUBLIC	bool    test_bad(err, exp, testcode, argname)
/* Checks if a tested function call returned an error.
 * Returns TRUE if 'err' != STD_OK.
 */

errstat err, exp;
char    testcode[], argname[];
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




PUBLIC	void	check_capsets(testcode, cs1, cs2)
/* Compares the two capsets and prints a message if they are unequal. */

char	testcode[];
capset	*cs1, *cs2;
{
  int	i;
  bool	ok;
  suite *s1, *s2;


  ok = cs1->cs_initial == cs2->cs_initial && cs1->cs_final == cs2->cs_final;
  if (ok)
    for (i = 0; i < cs1->cs_final; i++) {
      s1 = &cs1->cs_suite[i];
      s2 = &cs2->cs_suite[i];

      ok = s1->s_current == s2->s_current &&
	   (!s1->s_current || cap_cmp(&s1->s_object, &s2->s_object));
      if (!ok)
	break;
    }

  TEST_ASSERT(ok, TEST_SERIOUS,
		("%s, %s (capsets don't compare)\n", testname, testcode));
      
}  /* check_capsets() */




PUBLIC	void	check_masks(testcode, masks1, masks2, nmasks)
/* Compares 'masks1' with 'masks2' and prints a message if they are unequal. */

char	testcode[];
long	masks1[], masks2[];
int	nmasks;
{
  int	i;


  for (i = 0; i < nmasks; i++) {
    if (masks1[i] != masks2[i]) {
      TEST_ASSERT(FALSE, TEST_SERIOUS,
		("%s, %s (column masks don't compare)\n", testname, testcode));
      return;
    }
  }

}  /* check_masks */




PRIVATE int     usage(progname)
/* Print a message about how to use the test. */

char	progname[];
{
  fprintf(stderr, "Usage: %s [-vgbq] [pathname]\n", progname);
  exit(1);

}  /* usage() */




PRIVATE int	make_badcaps()

{
  int		i, n = 0;
  errstat	err;
  capability	good_cap, bad_cap, tmp_cap;


  (void)strcpy(testname, "make_badcaps()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Make a few corrupted capsets. */

  /* Make a capset for a discarded directory. */
  err = sp_create(SP_DEFAULT, def_colnames, &bad_capsets[n]);
  if (test_good(err, "sp_create()")) {
    if (test_good(sp_discard(&bad_capsets[n]), "sp_discard()"))
      cap_err[n++] = SP_UNREACH;
  }

  /* Make a capset for a Bullet file. */
  err = name_lookup(DEF_BULLETSVR, &good_cap);
  if (test_good(err, "name_lookup()")) {
    err = b_create(&good_cap, "Hello world!", (b_fsize)13,
					BS_COMMIT | BS_SAFETY, &bullet_cap);
    if (test_good(err, "b_create()")) {
      if (cs_singleton(&bad_capsets[n], &bullet_cap))
	cap_err[n++] = STD_COMBAD;
      else
	err = std_destroy(&bullet_cap);
    }
  }

  /* Get a valid capability from the default capset. */
  err = cs_goodcap(&def_capset, &good_cap);
  if (test_good(err, "cs_goodcap()")) {

    /* A capset with corrupted rights. */
    bad_cap = good_cap;
    bad_cap.cap_priv.prv_rights ^= SP_MODRGT;
    if (cs_singleton(&bad_capsets[n], &bad_cap))
      cap_err[n++] = STD_CAPBAD;

    /* A capset with corrupted protection fields. */
    err = std_restrict(&good_cap, 0xFF^SP_MODRGT, &bad_cap);
    if (test_good(err, "std_restrict()")) {
      err = cs_goodcap(&aux_capset, &tmp_cap);
      if (test_good(err, "cs_goodcap()")) {
	bad_cap.cap_priv.prv_rights = tmp_cap.cap_priv.prv_rights;
	bad_cap.cap_priv.prv_random = tmp_cap.cap_priv.prv_random;

	if (cs_singleton(&bad_capsets[n], &bad_cap))
	  cap_err[n++] = STD_CAPBAD;
      }
    }

    /* A capset with a NULL port. */
    bad_cap = good_cap;
    for (i = 0; i < PORTSIZE; i++)
      bad_cap.cap_port._portbytes[i] = 0x00;
    if (cs_singleton(&bad_capsets[n], &bad_cap))
      cap_err[n++] = RPC_BADPORT;

    /* A capset with the wrong port-number. */
    if (!quick) {
      /* RPCs from bad ports have a long time-out. */
      bad_cap = good_cap;
      for (i = 0; i < PORTSIZE; i++)
	bad_cap.cap_port._portbytes[i] =
				~good_cap.cap_port._portbytes[PORTSIZE-i-1];
      /* Note that, as it says in the Soap documentation, the Soap stubs
       * will return SP_UNAVAIL (rather than RPC_NOTFOUND) when the server
       * cannot be found.
       */
      if (cs_singleton(&bad_capsets[n], &bad_cap))
	cap_err[n++] = SP_UNAVAIL;
    }
  }

  return n;

}  /* make_badcaps() */




PRIVATE bool    test_init()
/* Initialize the global variables. */

{
  bool		ok;
  errstat	err;
  int		i, j;


  (void)strcpy(testname, "test_init()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Find the path to the current working directory. */
  testpath = getenv("_WORK");
  if (testpath == NULL) {
    TEST_ASSERT(FALSE, TEST_SERIOUS, 
			("%s, getenv(): test aborted\n", testname));
    return FALSE;
  }

  /* Make a few directories and capability sets. */
  err = sp_create(SP_DEFAULT, one_colname, &dir_capset);
  if (!test_ok(err, "sp_create()"))
    return FALSE;

  err = sp_create(&dir_capset, def_colnames, &def_capset);
  if (!test_ok(err, "sp_create()")) {
    (void)sp_discard(&dir_capset);
    cs_free(&dir_capset);
    return FALSE;
  }

  err = sp_append(&dir_capset, def_name, &def_capset, 1, def_masks);
  if (!test_ok(err, "sp_append()")) {
    (void)sp_discard(&def_capset);
    cs_free(&def_capset);
    (void)sp_discard(&dir_capset);
    cs_free(&dir_capset);
    return FALSE;
  }

  for (i = 0; i < MAXRGTCAPS; i++) {
    err = sp_chmod(&dir_capset, def_name, 1, rgt_masks[i]);
    if (ok = test_ok(err, "sp_chmod()")) {
      err = sp_lookup(&dir_capset, def_name, &rgt_capsets[i]);
      ok = test_ok(err, "sp_lookup()");
    }
    if (!ok)
      break;
  }

  if (ok) {
    err = sp_chmod(&dir_capset, def_name, 1, def_masks);
    if (ok = test_ok(err, "sp_chmod()")) {
      err = sp_append(&def_capset, def_name, &def_capset, DEF_COLS, def_masks);
      if (ok = test_ok(err, "sp_append()")) {
	err = sp_create(&dir_capset, all_colnames, &aux_capset);
	if (ok = test_ok(err, "sp_create()")) {
	  err = sp_append(&dir_capset, long_name, &aux_capset, 1, def_masks);
	  if (!(ok = test_ok(err, "sp_append()"))) {
	    (void)sp_discard(&aux_capset);
	    cs_free(&aux_capset);
	    (void)sp_delete(&def_capset, def_name);
	  }
	}
	else
	  (void)sp_delete(&def_capset, def_name);
      }
    }
  }

  if (ok) {
    err = sp_append(SP_DEFAULT, def_name, &dir_capset, 3, full_masks);
    if (ok = test_ok(err, "sp_append()")) {
      err = cwd_set(def_name);
      if (!(ok = test_ok(err, "cwd_set()"))) {
	(void)sp_delete(SP_DEFAULT, def_name);
	(void)sp_delete(&dir_capset, long_name);
	(void)sp_discard(&aux_capset);
	cs_free(&aux_capset);
	(void)sp_delete(&def_capset, def_name);
      }
    }
    else {
      (void)sp_delete(&dir_capset, long_name);
      (void)sp_discard(&aux_capset);
      cs_free(&aux_capset);
      (void)sp_delete(&def_capset, def_name);
    }
  }
    
  if (!ok) {
    for (j = 0; j < i; j++)
      cs_free(&rgt_capsets[j]);
    (void)sp_delete(&dir_capset, def_name);
    (void)sp_discard(&def_capset);
    cs_free(&def_capset);
    (void)sp_discard(&dir_capset);
    cs_free(&dir_capset);
    return FALSE;
  }

  if (badf)
    nbadcaps = make_badcaps();

  return TRUE;

}  /* test_init() */




PRIVATE	void	test_cleanup()
/* Cleans up all the global variables and test directories. */

{
    int		i;
    bool	ok;


    (void)strcpy(testname, "test_cleanup()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    ok = cwd_set("..") == STD_OK;

    for (i = 0; i < nbadcaps; i++)
      cs_free(&bad_capsets[i]);
    for (i = 0; i < MAXRGTCAPS; i++)
      cs_free(&rgt_capsets[i]);
    ok = (sp_delete(&def_capset, def_name) == STD_OK) && ok;
    ok = (sp_delete(&dir_capset, def_name) == STD_OK) && ok;
    ok = (sp_discard(&def_capset) == STD_OK) && ok;
    cs_free(&def_capset);
    ok = (sp_delete(&dir_capset, long_name) == STD_OK) && ok;
    ok = (sp_discard(&aux_capset) == STD_OK) && ok;
    cs_free(&aux_capset);
    ok = (sp_delete(SP_DEFAULT, def_name) == STD_OK) && ok;
    ok = (sp_discard(&dir_capset) == STD_OK) && ok;
    cs_free(&dir_capset);
    if (badf)
      ok = (std_destroy(&bullet_cap) == STD_OK) && ok;

    TEST_ASSERT(ok, TEST_WARNING,
			("%s (couldn't clean up properly)\n", testname));
}  /* test_cleanup() */




PUBLIC	int	main(argc, argv)

int	argc;
char	*argv[];
{
  char		c, *home_dir, *work_dir;
  errstat	err;
  EXTERN int    optind;


  /* Check the command-line  options. */
  while((c = getopt(argc, argv, "vgbq")) != EOF) {
    switch(c) {
      case 'v': verbose = TRUE;  break;
      case 'g': goodf   = TRUE;  break;
      case 'b': badf    = TRUE;  break;
      case 'q': quick   = TRUE;  break;

      default:  usage(argv[0]);
    }
  }

  /* Do all the tests by default. */
  if (!goodf && !badf)
    goodf = badf = TRUE;

  /* Decide which directory to use. */
  if (optind == argc)
    work_dir = NULL;
  else if (optind+1 == argc)
    work_dir = argv[optind];
  else
    usage(argv[0]);

  TEST_BEGIN(argv[0]);

  if (work_dir != NULL) {
    home_dir = getenv("_WORK");
    if (home_dir == NULL) {
      fprintf(stderr, "Couldn't find _WORK environment variable.\n");
      exit(-1);
    }
    if ((err = cwd_set(work_dir)) != STD_OK) {
      fprintf(stderr, "Couldn't change to directory %s (%s).\n",
							work_dir,err_why(err));
      exit(-1);
    }
  }

  if (test_init()) {
    if (goodf) {
      test_sp_append();
      test_sp_masks();
      test_sp_create();
      test_sp_delete();
      test_sp_lookup();
      test_sp_replace();
      test_sp_traverse();
      test_sp_discard();
    }
    if (badf) {
      interval orig_timeout;

      /* avoid long timeouts on non-existing servers: */
      orig_timeout = timeout((interval) 1000);
      test_bad_capsets();
      (void) timeout(orig_timeout);
      test_empty_dir();
      test_empty_name();
      test_bad_name();
      test_no_entry();
      test_bad_columns();
      test_bad_rights();
      test_no_access();
      test_bad_access();
      test_bad_many();
      test_bad_misc();
    }
    test_cleanup();
  }

  if (work_dir != NULL) {
    if ((err = cwd_set(home_dir)) != STD_OK) {
      fprintf(stderr, "Couldn't return to directory %s (%s).",
						home_dir, err_why(err));
    }
  }

  TEST_END();
  /*NOTREACHED*/
}  /* main() */
