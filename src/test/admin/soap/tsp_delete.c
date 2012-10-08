/*	@(#)tsp_delete.c	1.2	94/04/06 17:33:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_delete.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_delete()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_delete()

{
  errstat	err;


  (void)strcpy(testname, "test_sp_delete()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Delete an entry with a long name in the default directory. */
  err = sp_delete(SP_DEFAULT, long_name);
  if (test_good(err, "1a")) {
    err = sp_append(SP_DEFAULT, long_name, &aux_capset, 1, def_masks);
    if (!test_ok(err, "1a, sp_append()"))
      return;
  }

  /* Delete an entry in a regular directory with minimal rights. */
  err = sp_delete(&rgt_capsets[MOD], def_name);
  if (test_good(err, "1b")) {
    err = sp_append(&rgt_capsets[MOD], def_name, &def_capset, DEF_COLS,
								def_masks);
    if (!test_ok(err, "1b, sp_append()"))
      return;
  }

}  /* test_sp_delete() */

