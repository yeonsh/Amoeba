/*	@(#)tsp_append.c	1.2	94/04/06 17:33:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_append.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_append()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_append()
/* What's in a name? */

{
  errstat	err;
  capset	tmp_capset;
  long		tmp_masks[MAX_COLS];


  (void)strcpy(testname, "test_sp_append()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Append an entry in the default directory with one full right-mask. */
  err = sp_append(SP_DEFAULT, aux_name, &aux_capset, 1, full_masks);
  if (test_good(err, "1a")) {
    err = sp_getmasks(SP_DEFAULT, aux_name, 1, tmp_masks);
    if (test_good(err, "1a, sp_getmasks()"))
      check_masks("1a", tmp_masks, full_masks, 1);

    err = sp_lookup(SP_DEFAULT, aux_name, &tmp_capset);
    if (test_good(err, "1a, sp_lookup()")) {
      check_capsets("1a", &aux_capset, &tmp_capset);
      cs_free(&tmp_capset);
    }

    err = sp_delete(SP_DEFAULT, aux_name);
    if (!test_ok(err, "1a, sp_delete()"))
      return;
  }

  /* Append an entry with a very long name in a regular directory. */
  err = sp_append(&rgt_capsets[MOD], long_name, &aux_capset, DEF_COLS,
								def_masks);
  if (test_good(err, "1b")) {
    err = sp_getmasks(&rgt_capsets[MOD], long_name, DEF_COLS, tmp_masks);
    if (test_good(err, "1b, sp_getmasks()"))
      check_masks("1b", tmp_masks, def_masks, DEF_COLS);

    err = sp_lookup(&def_capset, long_name, &tmp_capset);
    if (test_good(err, "1b, sp_lookup()")) {
      check_capsets("1b", &aux_capset, &tmp_capset);
      cs_free(&tmp_capset);
    }

    err = sp_delete(&rgt_capsets[MOD], long_name);
    if (!test_ok(err, "1b, sp_delete()"))
      return;
  }

  /* Append an entry in a directory with many columns. */
  err = sp_append(&aux_capset, aux_name, &aux_capset, MAX_COLS, def_masks);
  if (test_good(err, "1c")) {
    err = sp_getmasks(&aux_capset, aux_name, MAX_COLS, tmp_masks);
    if (test_good(err, "1c, sp_getmasks()"))
      check_masks("1c", tmp_masks, def_masks, MAX_COLS);

    err = sp_lookup(&aux_capset, aux_name, &tmp_capset);
    if (test_good(err, "1c, sp_lookup()")) {
      check_capsets("1c", &aux_capset, &tmp_capset);
      cs_free(&tmp_capset);
    }

    err = sp_delete(&aux_capset, aux_name);
    if (!test_ok(err, "1c, sp_delete()"))
      return;
  }

}  /* test_sp_append() */

