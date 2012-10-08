/*	@(#)tb_delete.c	1.3	96/02/27 10:55:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_delete.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Source of the test for `b_delete()'.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_b_delete()

{
  errstat	err;
  bool		ok, ok1;
  capability	new_cap, tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_b_delete()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Delete an entire committed file. */
  err = b_delete(&commit_cap, ZERO, AVG_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1a")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));

    /* Check the size of the file. */
    err = b_size(&new_cap, &size);
    if (test_good(err, "1a, b_size()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	    ("%s, 1a: size should be 0 but is %ld\n", testname, (long)size));

    /* Check the contents of the file. */
    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1a, b_read()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1a: bytes read should be 0 but is %ld\n", testname, (long)size));

    /* Clean up. */
    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1a, std_destroy()");
    }
  }

  /* Delete one byte in a committed file. */
  err = b_delete(&commit_cap, AVG_OFFS, ONE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1b")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1b: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1b, b_size()"))
      TEST_ASSERT(size == AVG_SIZE-ONE, TEST_SERIOUS,
			("%s, 1b: size should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE-ONE), (long)size));

    memset(read_buf, 0, (size_t)AVG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    if (test_good(err, "1b, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE-ONE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE-ONE), (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_OFFS)==0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (size_t)AVG_OFFS,
		write_buf + (int)(AVG_OFFS+ONE), (int)(size-AVG_OFFS)) == 0,
  TEST_SERIOUS, ("%s, 1b: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1b, std_destroy()");
    }
  }

  /* Delete in an uncommitted file. The result should be committed. */
  err = b_delete(&uncommit_cap, ZERO, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1c")) {

    /* The capability should be an exact copy of the original one. */
    TEST_ASSERT(ok = cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1c: original capability expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1c, b_size()"))
      TEST_ASSERT(size == AVG_SIZE-AUX_SIZE, TEST_SERIOUS,
		("%s, 1c: size should be %ld but is %ld\n",
			testname, (long)(AVG_SIZE-AUX_SIZE), (long)size));

    memset(read_buf, 0, (size_t)AVG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    if (test_good(err, "1c, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE-AUX_SIZE, TEST_SERIOUS,
		("%s, 1c: bytes read should be %ld but is %ld\n",
			testname, (long)(AVG_SIZE-AUX_SIZE), (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf + (size_t)AUX_SIZE, (int)size) == 0,
    TEST_SERIOUS, ("%s, 1c: data read don't match original data\n", testname));
    }

    if (ok) {
      /* Restore the original contents. */
      err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
      (void)test_good(err, "1c, b_create()");
    }

    /* Destroy the new file. */
    err = std_destroy(&new_cap);
    (void)test_good(err, "1c, std_destroy()");
  }

  /* Delete in an empty file. */
  err = b_delete(&empty_cap, ZERO, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1d")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1d: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1d, b_size()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	    ("%s, 1d: size should be 0 but is %ld\n", testname, (long)size));

    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1d, b_read()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1d: bytes read should be 0 but is %ld\n", testname, (long)size));

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1d, std_destroy()");
    }
  }

  /* Delete at the end of a file. */
  err = b_delete(&commit_cap, AVG_SIZE, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1e")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1e: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1e, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1e: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1e, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1e, std_destroy()");
    }
  }

  /* Delete without committing in a committed file with one byte. */
  err = b_delete(&one_cap, ZERO, ONE, 0, &new_cap);
  if (test_good(err, "1f")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &one_cap), TEST_SERIOUS,
			("%s, 1f: new object expected\n", testname));

    /* You can't get the size or read from an uncommitted file. */
    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1f, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1f, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_delete(&new_cap, ZERO, ONE, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1f")) {

      /* The capability shouldn't change. */
      TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1f: previous capability expected\n", testname));
      TEST_ASSERT(ok1 = !obj_cmp(&tmp_cap, &one_cap), TEST_SERIOUS,
			("%s, 1f: new object expected\n", testname));

      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1f, b_size()"))
	TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	    ("%s, 1f: size should be 0 but is %ld\n", testname, (long)size));

      err = b_read(&tmp_cap, ZERO, read_buf, ONE, &size);
      if (test_good(err, "1f, b_read()"))
	TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1f: bytes read should be 0 but is %ld\n", testname, (long)size));

      if (ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1f, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1f, std_destroy()");
      }
    }
  }

  /* Delete in a file with only the minimum required rights. */
  err = std_restrict(&uncommit_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1g, std_restrict()")) {
    err = b_delete(&tmp_cap, AVG_OFFS, AVG_SIZE, 0, &new_cap);
    if (test_good(err, "1g")) {

      /* The capability should be identical to the original. */
      TEST_ASSERT(ok = cap_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1g: original capability expected\n", testname));

      /* You can't get the size or read from an uncommitted file. */
      err = b_size(&new_cap, &size);
      (void)test_bad(err, STD_CAPBAD, "1g, b_size()", "uncommitted file");

      err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
      (void)test_bad(err, STD_CAPBAD, "1g, b_read()", "uncommitted file");

      /* Now commit the file. */
      err = b_delete(&new_cap, AVG_OFFS, AUX_SIZE, SAFE_COMMIT, &tmp_cap);
      if (test_good(err, "1g")) {
	/* The capability shouldn't change. */
	TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1g: previous capability expected\n", testname));

	err = b_size(&new_cap, &size);
	if (test_good(err, "1g, b_size()"))
	  TEST_ASSERT(size == AVG_OFFS, TEST_SERIOUS,
			("%s, 1g: size should be %ld but is %ld\n",
				testname, (long)AVG_OFFS, (long)size));

	memset(read_buf, 0, (size_t)AVG_SIZE);
	err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
	if (test_good(err, "1g, b_read()")) {
	  TEST_ASSERT(size == AVG_OFFS, TEST_SERIOUS,
			("%s, 1g: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_OFFS, (long)size));
	  TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1g: data read don't match original data\n", testname));
	}

	/* The current rights should't permit destroying. */
	err = std_destroy(&tmp_cap);
	(void)test_bad(err, STD_DENIED, "1g, std_destroy()", "restricted cap");
      }

      /* Clean up. */
      if (ok || !ok1) {
	err = std_destroy(&uncommit_cap);
	(void)test_good(err, "1g, std_destroy()");

	/* Restore the original contents. */
	err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
	(void)test_good(err, "1g, b_create()");
      }
    }
  }

}  /* test_b_delete() */

