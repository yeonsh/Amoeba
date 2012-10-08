/*	@(#)tsp_masks.c	1.2	94/04/06 17:33:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_masks.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_getmasks()' and `sp_chmod()' stubs.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_masks()
/* Tests sp_chmod() and sp_getmasks(). */

{
  long		tmp_masks[MAX_COLS+1], test_masks[MAX_COLS+1];
  errstat	err;


  (void)strcpy(testname, "test_sp_masks()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Get and change masks in the default directory. */
  err = sp_getmasks(SP_DEFAULT, long_name, 1, tmp_masks);
  if (test_good(err, "1a, sp_getmasks(1)")) {
    check_masks("1a, sp_getmasks(1)", tmp_masks, def_masks, 1);
    err = sp_chmod(SP_DEFAULT, long_name, 1, full_masks);
    if (test_good(err, "1a, sp_chmod(1)")) {
      err = sp_getmasks(SP_DEFAULT, long_name, 1, test_masks);
      if (test_good(err, "1a, sp_getmasks(2)"))
	check_masks("1a, sp_getmasks(2)", test_masks, full_masks, 1);
      err = sp_chmod(SP_DEFAULT, long_name, 1, tmp_masks);
      if (test_good(err, "1a, sp_chmod(2)")) {
	err = sp_getmasks(SP_DEFAULT, long_name, 1, test_masks);
	if (test_good(err, "1a, sp_getmasks(3)"))
	  check_masks("1a, sp_getmasks(3)", test_masks, tmp_masks, 1);
      }
    }
  }

  /* Get masks without rights. */
  err = sp_getmasks(&rgt_capsets[READ], def_name, DEF_COLS, tmp_masks);
  if (test_good(err, "1c, sp_getmasks()"))
    check_masks("1c, sp_getmasks()", tmp_masks, def_masks, DEF_COLS);

  /* Get and change masks in a regular directory with minimal rights. */
  err = sp_getmasks(&rgt_capsets[MOD], def_name, DEF_COLS, tmp_masks);
  if (test_good(err, "1d, sp_getmasks(1)")) {
    check_masks("1d, sp_getmasks(1)", tmp_masks, def_masks, DEF_COLS);
    err = sp_chmod(&rgt_capsets[MOD], def_name, DEF_COLS, empty_masks);
    if (test_good(err, "1d, sp_chmod(1)")) {
      err = sp_getmasks(&rgt_capsets[MOD], def_name, DEF_COLS, test_masks);
      if (test_good(err, "1d, sp_getmasks(2)"))
	check_masks("1d, sp_getmasks(2)", test_masks, empty_masks, DEF_COLS);
      err = sp_chmod(&rgt_capsets[MOD], def_name, DEF_COLS, tmp_masks);
      if (test_good(err, "1d, sp_chmod(2)")) {
	err = sp_getmasks(&rgt_capsets[MOD], def_name, DEF_COLS, test_masks);
	if (test_good(err, "1d, sp_getmasks(3)"))
	  check_masks("1d, sp_getmasks(3)", test_masks, tmp_masks, DEF_COLS);
      }
    }
  }

}  /* test_sp_masks() */

