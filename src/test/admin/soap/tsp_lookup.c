/*	@(#)tsp_lookup.c	1.2	94/04/06 17:33:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_lookup.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_lookup()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_lookup()

{
  errstat	err;
  capset	new_capset;


  (void)strcpy(testname, "test_sp_lookup()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Look up an entry with a very long name in the default directory. */
  err = sp_lookup(SP_DEFAULT, long_name, &new_capset);
  if (test_good(err, "1a")) {
    check_capsets("1a", &aux_capset, &new_capset);
    cs_free(&new_capset);
  }

  /* Look up an entry in a regular directory with minimal rights. */
  err = sp_lookup(&rgt_capsets[READ], def_name, &new_capset);
  if (test_good(err, "1b")) {
    check_capsets("1b", &rgt_capsets[DEF], &new_capset);
    cs_free(&new_capset);
  }

}  /* test_sp_lookup() */

