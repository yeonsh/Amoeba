/*	@(#)tsp_discard.c	1.2	94/04/06 17:33:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_discard.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_discard()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_discard()
/* This should be the final test because it can mess up the test directory
 * structure severely.
 */

{
  errstat	err;
  bool		ok;
  capset	tmp_capset, disc_capset;


  (void)strcpy(testname, "test_sp_discard()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Discard an empty directory with minimal rights. */

  /* Create a new directory. */
  err = sp_create(SP_DEFAULT, def_colnames, &tmp_capset);
  if (test_good(err, "1a, sp_create()")) {

    /* Append and look-up to get a restricted capset. */
    err = sp_append(SP_DEFAULT, aux_name, &tmp_capset, 1, rgt_masks[DEL]);
    if (ok = test_good(err, "1a, sp_append()")) {
      err = sp_lookup(SP_DEFAULT, aux_name, &disc_capset);
      if (ok = test_good(err, "1a, sp_lookup()")) {
	/* Try to discard the restricted empty capset. */
        err = sp_discard(&disc_capset);
	ok = test_good(err, "1a");
	/* Free the space for the capset. */
        cs_free(&disc_capset);
      }

      /* Delete the entry for the original capset. */
      err = sp_delete(SP_DEFAULT, aux_name);
      (void)test_good(err, "1a, sp_delete()");
    }

    if (!ok) {
      /* Destroy the directory using the unrestricted capset. */
      err = sp_discard(&tmp_capset);
      (void)test_good(err, "1a, sp_discard()");
    }
    cs_free(&tmp_capset);
  }

}  /* test_sp_discard() */

