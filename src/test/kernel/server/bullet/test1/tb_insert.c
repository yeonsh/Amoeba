/*	@(#)tb_insert.c	1.3	96/02/27 10:55:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_insert.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Source of the test for `b_insert()'.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_b_insert()

{
  errstat	err;
  bool		ok, ok1;
  capability	new_cap, tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_b_insert()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Insert one byte in the middle of a committed file. */
  err = b_insert(&commit_cap, AVG_OFFS, write_buf, ONE, BS_COMMIT, &new_cap);
  if (test_good(err, "1a")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));

    /* Verify the size and contents of the new file. */
    err = b_size(&new_cap, &size);
    if (test_good(err, "1a, b_size()"))
      TEST_ASSERT(size == AVG_SIZE+ONE, TEST_SERIOUS,
			("%s, 1a: size should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE+ONE), (long)size));

    memset(read_buf, 0, (size_t)(BIG_SIZE));
    read_buf[(int)AVG_OFFS] = ~write_buf[1];
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1a, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE+ONE, TEST_SERIOUS,
			("%s, 1a: bytes read should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE+ONE), (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)(AVG_OFFS)) == 0,TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
      TEST_ASSERT(read_buf[(int)AVG_OFFS] == write_buf[0], TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (int)(AVG_OFFS+ONE),
	write_buf + (int)AVG_OFFS, (size_t)(size-AVG_OFFS-ONE)) == 0,TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1a, std_destroy()");
    }
  }

  /* Insert without committing one byte in a committed file with one byte. */
  err = b_modify(&one_cap, ONE, write_buf+2, ONE, 0, &new_cap);
  if (test_good(err, "1b")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &one_cap), TEST_SERIOUS,
			("%s, 1b: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1b, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1b, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_insert(&new_cap, ONE, write_buf+1, ONE, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1b")) {

      /* The capability shouldn't change. */
      TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1b:  previous capability expected\n", testname));
      TEST_ASSERT(ok1 = !obj_cmp(&tmp_cap, &one_cap), TEST_SERIOUS,
			("%s, 1b: new object expected\n", testname));

      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1b, b_size()"))
	TEST_ASSERT(size == (b_fsize)3, TEST_SERIOUS,
	    ("%s, 1b: size should be 3 but is %ld\n", testname, (long)size));

      memset(read_buf, 0, (size_t)AUX_SIZE);
      err = b_read(&tmp_cap, ZERO, read_buf, AUX_SIZE, &size);
      if (test_good(err, "1b, b_read()")) {
	TEST_ASSERT(size == (b_fsize)3, TEST_SERIOUS,
	("%s, 1b: bytes read should be 3 but is %ld\n", testname, (long)size));
	TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
      }

      if (ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1b, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1b, std_destroy()");
      }
    }
  }

  /* Insert in an uncommitted file. The result should be committed. */
  err = b_insert(&uncommit_cap, ZERO, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (test_good(err, "1c")) {

    /* The capability should be an exact copy of the original one. */
    TEST_ASSERT(ok = cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1c: original capability expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1c, b_size()"))
      TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1c: size should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1c, b_read()")) {
      TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1c: bytes read should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AUX_SIZE) == 0, TEST_SERIOUS,
		("%s, 1c: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (int)AUX_SIZE, write_buf,
				(size_t)(size-AUX_SIZE)) == 0, TEST_SERIOUS,
		("%s, 1c: data read don't match original data\n", testname));
    }

    /* Clean up. */
    if (ok) {
      /* Restore the original contents. */
      err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
      (void)test_good(err, "1c, b_create()");
    }

    /* Destroy the new file. */
    err = std_destroy(&new_cap);
    (void)test_good(err, "1c, std_destroy()");
  }

  /* Insert in an empty file. */
  err = b_insert(&empty_cap, ZERO, write_buf, AUX_SIZE, BS_COMMIT, &new_cap);
  if (test_good(err, "1d")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1d: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1d, b_size()"))
      TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: size should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));

    memset(read_buf, 0, (size_t)AVG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1d, b_read()")) {
      TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1d: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1d, std_destroy()");
    }
  }

  /* Insert at the end of a committed file. */
  err = b_insert(&commit_cap, AVG_SIZE, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (test_good(err, "1e")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1e: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1e, b_size()"))
      TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1e: size should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1e, b_read()")) {
      TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_SIZE) == 0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (int)AVG_SIZE, write_buf,
				(size_t)(size-AVG_SIZE)) == 0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1e, std_destroy()");
    }
  }

  /* Insert in a file with only the minimum required rights. */
  err = std_restrict(&uncommit_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1f, std_restrict()")) {
    err = b_insert(&tmp_cap, AVG_OFFS, write_buf, AUX_SIZE, 0, &new_cap);
    if (test_good(err, "1f")) {

      /* The capability should be identical to the original. */
      TEST_ASSERT(ok = cap_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1f: original capability expected\n", testname));

      /* You can't read or get the size from an uncommitted file. */
      err = b_size(&new_cap, &size);
      (void)test_bad(err, STD_CAPBAD, "1f, b_size()", "uncommitted file");

      err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
      (void)test_bad(err, STD_CAPBAD, "1f, b_read()", "uncommitted file");

      /* Now commit the file. */
      err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
      if (test_good(err, "1f")) {

	/* The capability shouldn't change. */
	TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1f: previous capability expected\n", testname));

	err = b_size(&new_cap, &size);
	if (test_good(err, "1f, b_size()"))
	  TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1f: size should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));

	memset(read_buf, 0, (size_t)BIG_SIZE);
	err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
	if (test_good(err, "1f, b_read()")) {
	  TEST_ASSERT(size == BIG_SIZE, TEST_SERIOUS,
			("%s, 1f: bytes read should be %ld but is %ld\n",
				testname, (long)BIG_SIZE, (long)size));
	  TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_OFFS) == 0,
  TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
	  TEST_ASSERT(memcmp(read_buf + (int)AVG_OFFS, write_buf,
					(size_t)AUX_SIZE) == 0, TEST_SERIOUS,
		("%s, 1f: data read don't match original data\n", testname));
	  TEST_ASSERT(memcmp(read_buf + (int)(AVG_OFFS+AUX_SIZE),
	    write_buf + (int)AVG_OFFS, (size_t)(size-AVG_OFFS-AUX_SIZE)) == 0,
  TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
	}

	/* The current rights should't permit destroying. */
	err = std_destroy(&tmp_cap);
	(void)test_bad(err, STD_DENIED, "1f, std_destroy()", "restricted cap");
      }

      /* Clean up. */
      if (ok || !ok1) {
	err = std_destroy(&uncommit_cap);
	(void)test_good(err, "1f, std_destroy()");

	/* Restore the original contents. */
	err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
	(void)test_good(err, "1f, b_create()");
      }
    }
  }

}  /* test_b_insert() */

