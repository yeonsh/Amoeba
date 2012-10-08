/*	@(#)tsp_replace.c	1.2	94/04/06 17:34:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_replace.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_replace()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_replace()

{
  errstat	err;
  capset	new_capset;


  (void)strcpy(testname, "test_sp_replace()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Replace an entry with a very long name in the default directory. */
  err = sp_replace(SP_DEFAULT, long_name, &def_capset);
  if (test_good(err, "1ax")) {
    err = sp_lookup(SP_DEFAULT, long_name, &new_capset);
    if (test_good(err, "1a, sp_lookup()")) {
      check_capsets("1a", &def_capset, &new_capset);
      cs_free(&new_capset);
    }

    err = sp_replace(SP_DEFAULT, long_name, &aux_capset);
    if (!test_ok(err, "1ay"))
      return;
  }

  /* Replace an entry in a regular directory with minimal rights. */
  err = sp_replace(&rgt_capsets[MOD], def_name, &aux_capset);
  if (test_good(err, "1bx")) {
    err = sp_lookup(&def_capset, def_name, &new_capset);
    if (test_good(err, "1b, sp_lookup()")) {
      check_capsets("1b", &aux_capset, &new_capset);
      cs_free(&new_capset);
    }

    err = sp_replace(&rgt_capsets[MOD], def_name, &def_capset);
    if (!test_ok(err, "1by"))
      return;
  }

}  /* test_sp_replace() */

