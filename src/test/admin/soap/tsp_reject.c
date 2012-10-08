/*	@(#)tsp_reject.c	1.2	94/04/06 17:33:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_reject.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for all the stubs with invalid parameters.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */

#ifndef __STDC__
#define const /**/
#endif

PUBLIC	void	test_bad_capsets()
/* Checks error codes returned by functions called with bad capsets. */

{
  errstat	err, err1, err2;
  int		i;
  bool		ok, eq;
  char		argname[32];
  capset	new_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_bad_capsets()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with bad capsets. */
  sprintf(tmp_path, "%s/%s", def_name, def_name);
  for (i = 0; i < nbadcaps; i++) {
    sprintf(argname, "bad_capsets[%d]", i);

    err1 = sp_append(&bad_capsets[i], aux_name, &aux_capset, DEF_COLS,
								def_masks);
    if (!test_bad(err1, cap_err[i], "2a, sp_append()", argname)) {
      err = sp_delete(&bad_capsets[i], aux_name);
      if (!test_ok(err, "2a, sp_delete()"))
	return;
    }

    err2 = sp_getmasks(&bad_capsets[i], def_name, DEF_COLS, tmp_masks);
    ok = test_bad(err2, cap_err[i], "2b, sp_getmasks()", argname);

    eq = err1 == err2;

    err1 = sp_chmod(&bad_capsets[i], def_name, DEF_COLS, full_masks);
    if (!test_bad(err1, cap_err[i], "2c, sp_chmod(1)", argname) && ok) {
      err = sp_chmod(&bad_capsets[i], def_name, DEF_COLS, tmp_masks);
      (void)test_bad(err, cap_err[i], "2c, sp_chmod(2)", argname);
    }

    eq = eq && (err1 == err2);

    err2 = sp_create(&bad_capsets[i], def_colnames, &new_capset);
    if (!test_bad(err2, cap_err[i], "2d, sp_create()", argname)) {
      err = sp_discard(&new_capset);
      cs_free(&new_capset);
      if (!test_ok(err, "2d, sp_discard()"))
	return;
    }

    eq = eq && (err1 == err2);

    err1 = sp_delete(&bad_capsets[i], def_name);
    (void)test_bad(err1, cap_err[i], "2e, sp_delete()", argname);

    eq = eq && (err1 == err2);

    err2 = sp_lookup(&bad_capsets[i], def_name, &new_capset);
    if (!test_bad(err2, cap_err[i], "2f, sp_lookup()", argname))
      cs_free(&new_capset);

    eq = eq && (err1 == err2);

    err1 = sp_replace(&bad_capsets[i], def_name, &aux_capset);
    (void)test_bad(err1, cap_err[i], "2g, sp_replace()", argname);

    eq = eq && (err1 == err2);

    path = tmp_path;
    err2 = sp_traverse(&bad_capsets[i], (const char **) &path, &new_capset);
    if (!test_bad(err2, cap_err[i], "2h, sp_traverse()", argname))
      cs_free(&new_capset);

    eq = eq && (err1 == err2);

    err1 = sp_discard(&bad_capsets[i]);
    (void)test_bad(err1, cap_err[i], "2i, sp_discard()", argname);

    TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));
  }

}  /* test_bad_capsets() */




PUBLIC	void	test_empty_dir()
/* Checks error codes returned by functions called with an empty directory. */

{
  errstat	err, err1, err2;
  bool		eq;
  capset	new_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_empty_dir()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with an empty directory. */
  err1 = sp_getmasks(&aux_capset, def_name, DEF_COLS, tmp_masks);
  if (!test_bad(err1, STD_NOTFOUND, "2a, sp_getmasks()", "empty directory")) {
    err = sp_delete(&aux_capset, def_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = sp_chmod(&aux_capset, def_name, DEF_COLS, full_masks);
  if (!test_bad(err2, STD_NOTFOUND, "2b, sp_chmod()", "empty directory")) {
    err = sp_delete(&aux_capset, def_name);
    if (!test_ok(err, "2b, sp_delete()"))
      return;
  }

  eq = err1 == err2;

  err1 = sp_delete(&aux_capset, def_name);
  (void)test_bad(err1, STD_NOTFOUND, "2c, sp_delete()", "empty directory");

  eq = eq && (err1 == err2);

  err2 = sp_lookup(&aux_capset, def_name, &new_capset);
  if (!test_bad(err2, STD_NOTFOUND, "2d, sp_lookup()","empty directory")) {
    cs_free(&new_capset);
    err = sp_delete(&aux_capset, def_name);
    if (!test_ok(err, "2d, sp_delete()"))
      return;
  }

  eq = eq && (err1 == err2);

  err1 = sp_replace(&aux_capset, def_name, &aux_capset);
  if (!test_bad(err1, STD_NOTFOUND, "2e, sp_replace()", "empty directory")) {
    err = sp_delete(&aux_capset, def_name);
    if (!test_ok(err, "2e, sp_delete()"))
      return;
  }

  eq = eq && (err1 == err2);

  sprintf(tmp_path, "%s/%s", def_name, def_name);
  path = tmp_path;
  err2 = sp_traverse(&aux_capset, (const char **) &path, &new_capset);
  if (!test_bad(err2, STD_NOTFOUND, "2f, sp_traverse()", "empty directory")) {
    cs_free(&new_capset);
    if (!test_ok(err, "2f, sp_delete()"))
      return;
  }

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_empty_dir() */




PUBLIC	void	test_empty_name()
/* Checks error codes returned by functions called with an empty name. */

{
  errstat	err, err1, err2;
  bool		ok, eq;
  capset	new_capset;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_empty_name()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with an empty name. */
  err1 = sp_append(SP_DEFAULT, "", &aux_capset, 1, def_masks);
  (void)test_bad(err1, STD_ARGBAD, "2a, sp_append()", "empty name");

  err2 = sp_getmasks(SP_DEFAULT, "", 1, tmp_masks);
  ok = test_bad(err2, STD_ARGBAD, "2b, sp_getmasks()", "empty name");

  eq = err1 == err2;

  err1 = sp_chmod(SP_DEFAULT, "", 1, full_masks);
  if (!test_bad(err1, STD_ARGBAD, "2c, sp_chmod(1)", "empty name") && ok) {
    err = sp_chmod(SP_DEFAULT, "", 1, tmp_masks);
    (void)test_bad(err, STD_ARGBAD, "2c, sp_chmod(2)", "empty name");
  }

  eq = eq && (err1 == err2);

  err2 = sp_delete(SP_DEFAULT, "");
  (void)test_bad(err2, STD_ARGBAD, "2d, sp_delete()", "empty name");

  eq = eq && (err1 == err2);

#ifdef notyet
  /* The sp_lookup stub currently allows looking up an empty name
   * (returning the directory itself) because some other sources depend
   * on this.  We should fix this some day.
   */
  err1 = sp_lookup(SP_DEFAULT, "", &new_capset);
  if (!test_bad(err1, STD_ARGBAD, "2e, sp_lookup()", "empty name"))
    cs_free(&new_capset);

  eq = eq && (err1 == err2);
#endif

  err2 = sp_replace(SP_DEFAULT, "", &aux_capset);
  (void)test_bad(err2, STD_ARGBAD, "2f, sp_replace()", "empty name");

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_empty_name() */




PUBLIC	void	test_bad_name()
/* Checks error codes returned by functions called with a too long name. */

{
  errstat	err, err1, err2;
  bool		ok, eq;
  capset	new_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_bad_name()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with a name that's too long. */
  err1 = sp_append(SP_DEFAULT, bad_name, &aux_capset, 1, def_masks);
  if (!test_bad(err1, STD_NOSPACE, "2a, sp_append()", "long name")) {
    err = sp_delete(SP_DEFAULT, bad_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = sp_getmasks(SP_DEFAULT, bad_name, 1, tmp_masks);
  ok = test_bad(err2, STD_NOSPACE, "2b, sp_getmasks()", "long name");

  eq = err1 == err2;

  err1 = sp_chmod(SP_DEFAULT, bad_name, 1, full_masks);
  if (!test_bad(err1, STD_NOSPACE, "2c, sp_chmod(1)", "long name") && ok) {
    err = sp_chmod(SP_DEFAULT, bad_name, 1, tmp_masks);
    (void)test_bad(err, STD_NOSPACE, "2c, sp_chmod(2)", "long name");
  }

  eq = eq && (err1 == err2);

  err2 = sp_delete(SP_DEFAULT, bad_name);
  if (!test_bad(err2, STD_NOSPACE, "2d, sp_delete()", "long name")) {
    err = sp_append(SP_DEFAULT, bad_name, &def_capset, 1, def_masks);
    if (!test_ok(err, "2d, sp_append()"))
      return;
  }

  eq = eq && (err1 == err2);

  err1 = sp_lookup(SP_DEFAULT, bad_name, &new_capset);
  if (!test_bad(err1, STD_NOSPACE, "2e, sp_lookup()", "long name"))
    cs_free(&new_capset);

  eq = eq && (err1 == err2);

  err2 = sp_replace(SP_DEFAULT, bad_name, &aux_capset);
  if (!test_bad(err2, STD_NOSPACE, "2f, sp_replace(1)", "long name")) {
    err = sp_replace(SP_DEFAULT, bad_name, &def_capset);
    if (!test_ok(err, "2f, sp_replace(2)"))
      return;
  }

  eq = eq && (err1 == err2);

  sprintf(tmp_path, "%s/%s", bad_name, def_name);
  path = tmp_path;
  err1 = sp_traverse(SP_DEFAULT, (const char **) &path, &new_capset);
  if (!test_bad(err1, STD_NOSPACE, "2g, sp_traverse()", "long name"))
    cs_free(&new_capset);

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_bad_name() */




PUBLIC	void	test_no_entry()
/* Checks error codes returned by functions called with a non-existing entry. */

{
  errstat	err, err1, err2;
  bool		eq;
  capset	new_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_no_entry()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with a non-existing entry. */
  err1 = sp_getmasks(&def_capset, aux_name, DEF_COLS, tmp_masks);
  if (!test_bad(err1, STD_NOTFOUND, "2a, sp_getmasks()", "non-existing entry"))
  {
    err = sp_delete(&def_capset, aux_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = sp_chmod(&def_capset, aux_name, DEF_COLS, full_masks);
  if (!test_bad(err2, STD_NOTFOUND, "2b, sp_chmod()", "non-existing entry")) {
    err = sp_delete(&def_capset, aux_name);
    if (!test_ok(err, "2b, sp_delete()"))
      return;
  }

  eq = err1 == err2;

  err1 = sp_delete(&def_capset, aux_name);
  (void)test_bad(err1, STD_NOTFOUND, "2c, sp_delete()", "non-existing entry");

  eq = eq && (err1 == err2);

  err2 = sp_lookup(&def_capset, aux_name, &new_capset);
  if (!test_bad(err2, STD_NOTFOUND, "2d, sp_lookup()", "non-existing entry")) {
    cs_free(&new_capset);
    err = sp_delete(&def_capset, aux_name);
    if (!test_ok(err, "2d, sp_delete()"))
      return;
  }

  eq = eq && (err1 == err2);

  err1 = sp_replace(&def_capset, aux_name, &aux_capset);
  if (!test_bad(err1, STD_NOTFOUND, "2e, sp_replace()", "non-existing entry"))
  {
    err = sp_delete(&def_capset, aux_name);
    if (!test_ok(err, "2e, sp_delete()"))
      return;
  }

  eq = eq && (err1 == err2);

  sprintf(tmp_path, "%s/%s", aux_name, def_name);
  path = tmp_path;
  err2 = sp_traverse(&def_capset, (const char **) &path, &new_capset);
  if (!test_bad(err2, STD_NOTFOUND, "2f, sp_traverse()", "non-existing entry"))
  {
    cs_free(&new_capset);
    err = sp_delete(&def_capset, aux_name);
    if (!test_ok(err, "2f, sp_delete()"))
      return;
  }

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_no_entry() */




PUBLIC	void	test_bad_columns()
/* Checks error codes returned by functions called with a different
 * number of cols than the directory has.
 */

{
  errstat	err, err1, err2;
  int		i;
  bool		ok, eq;
  long		tmp_masks[MAX_COLS+1];
  char		argname[32];


  (void)strcpy(testname, "test_bad_columns()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with a different number of columns. */
  for (i = 0; i <= MAX_COLS+1; i++) {
    int	should_fail;
    int undo;

    if (i == DEF_COLS) continue;

    should_fail = (i < 1) || (i > MAX_COLS);
    sprintf(argname, "%d masks", i);

    err1 = sp_append(&def_capset, aux_name, &aux_capset, i, def_masks);
    undo = 0;
    if (should_fail) {
        if (!test_bad(err1, STD_ARGBAD, "2a, sp_append()", argname)) {
	    undo = (err1 == STD_OK);
	}
    } else {
	if (test_good(err1, "2b, sp_getmasks()")) {
	    undo = 1;
	}
    }
    if (undo) {
	err = sp_delete(&def_capset, aux_name);
	if (!test_ok(err, "2a, sp_delete()"))
	    return;
    }

    err2 = sp_getmasks(&def_capset, def_name, i, tmp_masks);
    if (should_fail) {
	ok = test_bad(err2, STD_ARGBAD, "2b, sp_getmasks()", argname);
    } else {
	ok = test_good(err2, "2b, sp_getmasks()");
    }

    eq = err1 == err2;

    err1 = sp_chmod(&def_capset, def_name, i, full_masks);
    undo = 0;
    if (should_fail) {
	if (!test_bad(err1, STD_ARGBAD, "2c, sp_chmod(1)", argname)) {
	    undo = (err1 == STD_OK);
	}
    } else {
	if (test_good(err1, "2c, sp_chmod(1)")) {
	    undo = 1;
	}
    }
    if (undo && ok) {
	err = sp_chmod(&def_capset, def_name, i, tmp_masks);
	(void) test_good(err, "2c, sp_chmod(2)");
    }

    TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));
  }

}  /* test_bad_columns() */




PUBLIC	void	test_bad_rights()
/* Checks error codes returned by functions called with insufficient rights. */

{
  errstat	err, err1, err2;
  bool		ok, eq = TRUE;
  long		tmp_masks[MAX_COLS+1];
  capset	tmp_capset, disc_capset;


  (void)strcpy(testname, "test_bad_rights()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with insufficient rights. */
  err1 = sp_append(&rgt_capsets[DEL], aux_name, &aux_capset, DEF_COLS,
								def_masks);
  if (!test_bad(err1, STD_DENIED, "2a, sp_append()", "insufficient rights")) {
    err = sp_delete(&rgt_capsets[DEL], aux_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = err1;
  err = sp_getmasks(&rgt_capsets[DEL], def_name, DEF_COLS, tmp_masks);
  if (test_good(err, "2b, sp_getmasks()")) {
    err2 = sp_chmod(&rgt_capsets[DEL], def_name, DEF_COLS, full_masks);
    if (!test_bad(err2, STD_DENIED, "2b, sp_chmod(1)", "insufficient rights")) {
      err = sp_chmod(&rgt_capsets[DEL], def_name, DEF_COLS, tmp_masks);
      (void)test_bad(err, STD_DENIED, "2b, sp_chmod(2)", "insufficient rights");
    }
    eq = eq && (err1 == err2);
  }

  err1 = sp_delete(&rgt_capsets[DEL], def_name);
  if (!test_bad(err1, STD_DENIED, "2c, sp_delete()", "insufficient rights")) {
    err = sp_append(&def_capset, def_name, &def_capset, DEF_COLS, def_masks);
    if (!test_ok(err, "2c, sp_append()"))
      return;
  }

  eq = eq && (err1 == err2);

  err2 = sp_replace(&rgt_capsets[DEL], def_name, &aux_capset);
  if (!test_bad(err2, STD_DENIED, "2d, sp_replace(1)", "insufficient rights")) {
    err = sp_replace(&rgt_capsets[DEL], def_name, &def_capset);
    if (!test_ok(err, "2d, sp_replace(2)"))
      return;
  }

  eq = eq && (err1 == err2);

  /* Create a new directory. */
  err = sp_create(SP_DEFAULT, def_colnames, &tmp_capset);
  if (test_good(err, "2e, sp_create()")) {

    /* Append and look-up to get a restricted capset. */
    err = sp_append(SP_DEFAULT, aux_name, &tmp_capset, 1, rgt_masks[MOD]);
    if (ok = test_good(err, "2e, sp_append()")) {
      err = sp_lookup(SP_DEFAULT, aux_name, &disc_capset);
      if (ok = test_good(err, "2e, sp_lookup()")) {
	/* Try to discard the restricted empty capset. */
	err1 = sp_discard(&disc_capset);
	ok = !test_bad(err1, STD_DENIED, "2e, sp_discard(1)",
							"insufficient rights");
	/* Free the space for the capset. */
	cs_free(&disc_capset);
	eq = eq && (err1 == err2);
      }

      /* Delete the entry for the original capset. */
      err = sp_delete(SP_DEFAULT, aux_name);
      (void)test_good(err, "2e, sp_delete()");
    }

    if (!ok) {
      /* Destroy the directory using the unrestricted capset. */
      err = sp_discard(&tmp_capset);
      (void)test_good(err, "2e, sp_discard(2)");
    }
    cs_free(&tmp_capset);
  }

  TEST_ASSERT(eq, TEST_WARNING, ("%s (inconsistent error codes)\n", testname));

}  /* test_bad_rights() */




PUBLIC	void	test_no_access()
/* Checks error codes returned by functions called without access to cols. */

{
  errstat	err, err1, err2;
  bool		ok, eq;
  capset	tmp_capset, disc_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_no_access()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests without access to any columns. */
  err1 = sp_append(&rgt_capsets[NOCOL], aux_name, &aux_capset, DEF_COLS,
								def_masks);
  if (!test_bad(err1, STD_DENIED, "2a, sp_append()", "insufficient access")) {
    err = sp_delete(&rgt_capsets[NOCOL], aux_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = sp_getmasks(&rgt_capsets[NOCOL], def_name, DEF_COLS, tmp_masks);
  ok = test_bad(err2, STD_DENIED, "2b, sp_getmasks()", "insufficient access");

  eq = err1 == err2;

  err1 = sp_chmod(&rgt_capsets[NOCOL], def_name, DEF_COLS, full_masks);
  if (!test_bad(err1, STD_DENIED, "2c, sp_chmod(1)", "insufficient access") &&
									ok) {
    err = sp_chmod(&rgt_capsets[NOCOL], def_name, DEF_COLS, tmp_masks);
    (void)test_bad(err, STD_DENIED, "2c, sp_chmod(2)", "insufficient access");
  }

  eq = eq && (err1 == err2);

  err2 = sp_delete(&rgt_capsets[NOCOL], def_name);
  if (!test_bad(err2, STD_DENIED, "2d, sp_delete()", "insufficient access")) {
    err = sp_append(&def_capset, def_name, &def_capset, DEF_COLS, def_masks);
    if (!test_ok(err, "2d, sp_append()"))
      return;
  }

  eq = eq && (err1 == err2);

  err1 = sp_lookup(&rgt_capsets[NOCOL], def_name, &tmp_capset);
  if (!test_bad(err1, STD_DENIED, "2e, sp_lookup()", "insufficient access"))
    cs_free(&tmp_capset);

  eq = eq && (err1 == err2);

  err2 = sp_replace(&rgt_capsets[NOCOL], def_name, &aux_capset);
  if (!test_bad(err2, STD_DENIED, "2f, sp_replace(1)", "insufficient access")) {
    err = sp_replace(&rgt_capsets[NOCOL], def_name, &def_capset);
    if (!test_ok(err, "2f, sp_replace(2)"))
      return;
  }

  eq = eq && (err1 == err2);

  sprintf(tmp_path, "%s/%s", def_name, def_name);
  path = tmp_path;
  err1 = sp_traverse(&rgt_capsets[NOCOL], (const char **) &path, &tmp_capset);
  if (!test_bad(err1, STD_DENIED, "2g, sp_traverse()", "insufficient access"))
    cs_free(&tmp_capset);

  eq = eq && (err1 == err2);

  /* Create a new directory. */
  err = sp_create(SP_DEFAULT, def_colnames, &tmp_capset);
  if (test_good(err, "2h, sp_create()")) {

    /* Append and look-up to get a restricted capset. */
    err = sp_append(SP_DEFAULT, aux_name, &tmp_capset, 1, rgt_masks[NOCOL]);
    if (ok = test_good(err, "2h, sp_append()")) {
      err = sp_lookup(SP_DEFAULT, aux_name, &disc_capset);
      if (ok = test_good(err, "2h, sp_lookup()")) {
	/* Try to discard the restricted empty capset. */
	err2 = sp_discard(&disc_capset);
	ok = !test_bad(err2, STD_DENIED, "2h, sp_discard(1)",
							"insufficient access");
	/* Free the space for the capset. */
	cs_free(&disc_capset);
	eq = eq && (err1 == err2);
      }

      /* Delete the entry for the original capset. */
      err = sp_delete(SP_DEFAULT, aux_name);
      (void)test_good(err, "2h, sp_delete()");
    }

    if (!ok) {
      /* Destroy the directory using the unrestricted capset. */
      err = sp_discard(&tmp_capset);
      (void)test_good(err, "2h, sp_discard(2)");
    }
    cs_free(&tmp_capset);
  }

  TEST_ASSERT(eq, TEST_WARNING, ("%s (inconsistent error codes)\n", testname));

}  /* test_no_access() */




PUBLIC	void	test_bad_access()
/* Checks error codes returned by functions called with access to a bad col. */

{
  errstat	err, err1, err2;
  bool		ok, eq;
  capset	tmp_capset, disc_capset;
  char		tmp_path[1024], *path;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_bad_access()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with access to non-existing columns. */
  err1 = sp_append(&rgt_capsets[BADCOL], aux_name, &aux_capset, DEF_COLS,
								def_masks);
  if (!test_bad(err1, STD_DENIED, "2a, sp_append()", "wrong access")) {
    err = sp_delete(&rgt_capsets[BADCOL], aux_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  err2 = sp_getmasks(&rgt_capsets[BADCOL], def_name, DEF_COLS, tmp_masks);
  ok = test_bad(err2, STD_DENIED, "2b, sp_getmasks()", "wrong access");

  eq = err1 == err2;

  err1 = sp_chmod(&rgt_capsets[BADCOL], def_name, DEF_COLS, full_masks);
  if (!test_bad(err1, STD_DENIED, "2c, sp_chmod(1)", "wrong access") && ok) {
    err = sp_chmod(&rgt_capsets[BADCOL], def_name, DEF_COLS, tmp_masks);
    (void)test_bad(err, STD_DENIED, "2c, sp_chmod(2)", "wrong access");
  }

  eq = eq && (err1 == err2);

  err2 = sp_delete(&rgt_capsets[BADCOL], def_name);
  if (!test_bad(err2, STD_DENIED, "2d, sp_delete()", "wrong access")) {
    err = sp_append(&def_capset, def_name, &def_capset, DEF_COLS, def_masks);
    if (!test_ok(err, "2d, sp_append()"))
      return;
  }

  eq = eq && (err1 == err2);

  err1 = sp_lookup(&rgt_capsets[BADCOL], def_name, &tmp_capset);
  if (!test_bad(err1, STD_DENIED, "2e, sp_lookup()", "wrong access"))
    cs_free(&tmp_capset);

  eq = eq && (err1 == err2);

  err2 = sp_replace(&rgt_capsets[BADCOL], def_name, &aux_capset);
  if (!test_bad(err2, STD_DENIED, "2f, sp_replace(1)", "wrong access")) {
    err = sp_replace(&rgt_capsets[BADCOL], def_name, &def_capset);
    if (!test_ok(err, "2f, sp_replace(2)"))
      return;
  }

  eq = eq && (err1 == err2);

  sprintf(tmp_path, "%s/%s", def_name, def_name);
  path = tmp_path;
  err1 = sp_traverse(&rgt_capsets[BADCOL], (const char **) &path, &tmp_capset);
  if (!test_bad(err1, STD_DENIED, "2g, sp_traverse()", "wrong access"))
    cs_free(&tmp_capset);

  eq = eq && (err1 == err2);

  /* Create a new directory. */
  err = sp_create(SP_DEFAULT, def_colnames, &tmp_capset);
  if (test_good(err, "2h, sp_create()")) {

    /* Append and look-up to get a restricted capset. */
    err = sp_append(SP_DEFAULT, aux_name, &tmp_capset, 1, rgt_masks[BADCOL]);
    if (ok = test_good(err, "2h, sp_append()")) {
      err = sp_lookup(SP_DEFAULT, aux_name, &disc_capset);
      if (ok = test_good(err, "2h, sp_lookup()")) {
	/* Try to discard the restricted empty capset. */
	err2 = sp_discard(&disc_capset);
	ok = !test_bad(err2, STD_DENIED, "2h, sp_discard(1)", "wrong access");
	/* Free the space for the capset. */
	cs_free(&disc_capset);
	eq = eq && (err1 == err2);
      }

      /* Delete the entry for the original capset. */
      err = sp_delete(SP_DEFAULT, aux_name);
      (void)test_good(err, "2h, sp_delete()");
    }

    if (!ok) {
      /* Destroy the directory using the unrestricted capset. */
      err = sp_discard(&tmp_capset);
      (void)test_good(err, "2h, sp_discard(2)");
    }
    cs_free(&tmp_capset);
  }

  TEST_ASSERT(eq, TEST_WARNING, ("%s (inconsistent error codes)\n", testname));

}  /* test_bad_access() */




PUBLIC	void	test_bad_many()
/* Checks error codes returned by functions called with multiple bad args. */

{
  errstat	err;
  bool		ok;
  capset	new_capset;
  long		tmp_masks[MAX_COLS+1];


  (void)strcpy(testname, "test_bad_many()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with multiple bad arguments. */
  err = sp_append(bad_capsets, bad_name, &aux_capset, MAX_COLS+1, def_masks); 
  if (!test_bad(err, STD_OK, "2a, sp_append()", "multiple arguments")) {
    err = sp_delete(bad_capsets, bad_name);
    if (!test_ok(err, "2b, sp_delete()"))
      return;
  }

  err = sp_getmasks(bad_capsets, bad_name, MAX_COLS+1, tmp_masks);
  ok = test_bad(err, STD_OK, "2b, sp_getmasks()", "multiple arguments");

  err = sp_chmod(bad_capsets, bad_name, MAX_COLS+1, full_masks);
  if (!test_bad(err, STD_OK, "2c, sp_chmod(1)", "multiple arguments") && ok) {
    err = sp_chmod(bad_capsets, bad_name, MAX_COLS+1, tmp_masks);
    (void)test_bad(err, STD_OK, "2c, sp_chmod(2)", "multiple arguments");
  }

  err = sp_create(&bad_capsets[nbadcaps], many_colnames, &new_capset);
  if (!test_bad(err, STD_OK, "2d, sp_create()", "multiple arguments")) {
    err = sp_discard(&new_capset);
    cs_free(&new_capset);
    if (!test_ok(err, "2d, sp_discard()"))
      return;
  }

  err = sp_delete(bad_capsets, bad_name);
  (void)test_bad(err, STD_OK, "2e, sp_delete()", "multiple arguments");

  err = sp_lookup(bad_capsets, bad_name, &new_capset);
  if (!test_bad(err, STD_OK, "2f, sp_lookup()", "multiple arguments"))
    cs_free(&new_capset);

  err = sp_replace(bad_capsets, bad_name, &aux_capset);
  (void)test_bad(err, STD_OK, "2g, sp_replace()", "multiple arguments");

}  /* test_bad_many() */




PUBLIC	void	test_bad_misc()
/* Checks error codes returned by functions called with misc. bad args. */

{
  errstat	err;
  capset	new_capset;


  (void)strcpy(testname, "test_bad_misc()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Tests with bad arguments. */

  /* Try to append an entry with an existing name. */
  err = sp_append(SP_DEFAULT, def_name, &aux_capset, 1, def_masks);
  if (!test_bad(err, STD_EXISTS, "2a, sp_append()", "existing name")) {
    err = sp_delete(SP_DEFAULT, def_name);
    if (!test_ok(err, "2a, sp_delete()"))
      return;
  }

  /* Try to create a directory without any columns. */
  err = sp_create(SP_DEFAULT, no_colnames, &new_capset);
  if (!test_bad(err, STD_ARGBAD, "2b, sp_create()", "zero columns")) {
    err = sp_discard(&new_capset);
    cs_free(&new_capset);
    if (!test_ok(err, "2b, sp_discard()"))
      return;
  }

  /* Try to create a directory with too many columns. */
  err = sp_create(&def_capset, many_colnames, &new_capset);
  if (!test_bad(err, STD_ARGBAD, "2c, sp_create()", "too many columns")) {
    err = sp_discard(&new_capset);
    cs_free(&new_capset);
    if (!test_ok(err, "2c, sp_discard()"))
      return;
  }

  /* Try to discard a filled directory. */
  err = sp_discard(&def_capset);
  (void)test_bad(err, SP_NOTEMPTY, "2d, sp_discard()", "filled directory");

}  /* test_bad_misc() */

