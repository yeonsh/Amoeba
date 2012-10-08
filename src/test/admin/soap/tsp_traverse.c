/*	@(#)tsp_traverse.c	1.2	94/04/06 17:34:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_traverse.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_traverse()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */

#ifndef __STDC__
#define const	/**/
#endif


PUBLIC	void	test_sp_traverse()

{
  errstat	err;
  char		tmp_path[1024], *path;
  capset	new_capset;


  (void)strcpy(testname, "test_sp_traverse()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Traverse absolute path using the default capset. */
  sprintf(tmp_path, "%s/%s/../%s/%s", testpath, def_name, def_name, def_name);
  path = tmp_path;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1a")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1a (directory names don't compare)\n", testname));
    check_capsets("1a", &dir_capset, &new_capset);
    cs_free(&new_capset);
  }

  /* Traverse absolute path using a regular capset. */
  path = tmp_path;
  err = sp_traverse(&rgt_capsets[NORGT], (const char **) &path, &new_capset);
  if (test_good(err, "1b")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1b (directory names don't compare)\n", testname));
    check_capsets("1b", &dir_capset, &new_capset);
    cs_free(&new_capset);
  }

  /* Traverse path relative to a regular directory. */
  sprintf(tmp_path, "%s/../%s/./%s", def_name, def_name, def_name);
  path = tmp_path;
  err = sp_traverse(&rgt_capsets[READ], (const char **) &path, &new_capset);
  if (test_good(err, "1c")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1c (directory names don't compare)\n", testname));
    check_capsets("1c", &rgt_capsets[DEF], &new_capset);
    cs_free(&new_capset);
  }

  /* Traverse empty path relative to the default directory. */
  tmp_path[0] = (char)0;
  path = tmp_path;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1d")) {
    TEST_ASSERT(strcmp(path, "") == 0, TEST_SERIOUS,
		("%s, 1d (directory names don't compare)\n", testname));
    cs_free(&new_capset);
  }

  /* Traverse absolute path to the root directory. */
  tmp_path[0] = '/'; tmp_path[1] = '\0';
  path = tmp_path;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1e")) {
    TEST_ASSERT(strcmp(path, "") == 0, TEST_SERIOUS,
		("%s, 1e (directory names don't compare)\n", testname));
    cs_free(&new_capset);
  }

  /* Traverse path with single directory name. */
  path = def_name;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1f")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1f (directory names don't compare)\n", testname));
    cs_free(&new_capset);
  }

  /* Traverse path with single directory name with minimal rights. */
  path = def_name;
  err = sp_traverse(&rgt_capsets[NORGT], (const char **) &path, &new_capset);
  if (test_good(err, "1g")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1g (directory names don't compare)\n", testname));
    check_capsets("1g", &rgt_capsets[NORGT], &new_capset);
    cs_free(&new_capset);
  }

  /* Traverse path relative to the default directory. */
  sprintf(tmp_path, "%s/../%s/%s", def_name, def_name, def_name);
  path = tmp_path;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1h")) {
    TEST_ASSERT(strcmp(path, def_name) == 0, TEST_SERIOUS,
		("%s, 1h (directory names don't compare)\n", testname));
    check_capsets("1h", &def_capset, &new_capset);
    cs_free(&new_capset);
  }

  /* Traverse path ending with a '/'. */
  sprintf(tmp_path, "%s/%s/", testpath, def_name);
  path = tmp_path;
  err = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (test_good(err, "1i")) {
    TEST_ASSERT(strcmp(path, "") == 0, TEST_SERIOUS,
		("%s, 1i (directory names don't compare)\n", testname));
    check_capsets("1i", &dir_capset, &new_capset);
    cs_free(&new_capset);
  }

}  /* test_sp_traverse() */

