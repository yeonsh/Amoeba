/*	@(#)tsp_create.c	1.2	94/04/06 17:33:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tsp_create.c
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests for the `sp_create()' stub.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tsoap.h"	/* Takes care of all other includes. */




PUBLIC	void	test_sp_create()

{
  capset	new_capset;
  errstat	err;


  (void)strcpy(testname, "test_sp_create()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Create a directory with one column in the default directory. */
  err = sp_create(SP_DEFAULT, one_colname, &new_capset);
  if (test_good(err, "1a")) {
    err = sp_discard(&new_capset);
    cs_free(&new_capset);
    if (!test_ok(err, "1a, sp_discard()"))
      return;
  }

  /* Create a directory in a regular directory with minimal rights.
   * Note: the original soap documentation suggested that one should
   * be able to create a directory without having any rights at all.
   * This has been changed: now access to at least one column is required.
   */
  err = sp_create(&rgt_capsets[READ], all_colnames, &new_capset);
  if (test_good(err, "1b")) {
    err = sp_discard(&new_capset);
    cs_free(&new_capset);
    if (!test_ok(err, "1b, sp_discard()"))
      return;
  }

}  /* test_sp_create() */

