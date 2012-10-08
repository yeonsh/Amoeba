/*	@(#)tb_combi.c	1.3	96/02/27 10:54:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_combi.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Combination tests.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_combi()

{
  errstat	err;
  bool		ok, ok1;
  capability	new_cap, tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_many()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Create an uncommitted file with the same contents as the original. */
  err = b_create(&commit_cap, write_buf, AVG_SIZE, 0, &new_cap);
  if (test_good(err, "1a, b_create()")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));

    /* Modify it. */
    err = b_modify(&new_cap, AVG_OFFS, write_buf, AUX_SIZE, 0, &tmp_cap);
    if (test_good(err, "1a, b_modify()")) {

      /* The capability shouldn't change. */
      TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1a: previous capability expected\n", testname));

      /* Clean up possible debris. */
      if (!ok1) {
	if (ok) {
	  err = std_destroy(&new_cap);
	  (void)test_good(err, "std_destroy()");
	}

	new_cap = tmp_cap;
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "std_destroy()");
	TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));
      }

      /* Delete some. */
      err = b_delete(&new_cap, AVG_OFFS, AUX_SIZE, 0, &tmp_cap);
      if (test_good(err, "1a, b_delete()")) {
	TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1a: previous capability expected\n", testname));

	if (!ok1) {
	  if (ok) {
	    err = std_destroy(&new_cap);
	    (void)test_good(err, "std_destroy()");
	  }

	  new_cap = tmp_cap;
	  err = std_destroy(&tmp_cap);
	  (void)test_good(err, "std_destroy()");
	  TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));
	}

	/* Insert some (restore original contents) and commit. */
	err = b_insert(&new_cap, AVG_OFFS, write_buf + (int)AVG_OFFS,
					AUX_SIZE, SAFE_COMMIT, &tmp_cap);
	if (test_good(err, "1a, b_insert()")) {

          /* The capability should be the same as the original one. */
	  TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: original capability expected\n", testname));
	  TEST_ASSERT(!obj_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));

	  err = b_size(&tmp_cap, &size);
	  if (test_good(err, "1a, b_size()"))
		TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

	  memset(read_buf, 0, (size_t)BIG_SIZE);
	  err = b_read(&tmp_cap, ZERO, read_buf, BIG_SIZE, &size);
	  if (test_good(err, "1a, b_read()")) {
	    TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
	    TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size)==0,TEST_SERIOUS,
		("%s, 1a: data read do not match original data\n", testname));
	  }

	  if (!ok1) {
	    err = std_destroy(&tmp_cap);
	    (void)test_good(err, "1a, std_destroy()");
	  }

	  ok = FALSE;	/* Nothing left to destroy. */
	}
      }
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1a, std_destroy()");
    }
  }

}  /* test_many() */

