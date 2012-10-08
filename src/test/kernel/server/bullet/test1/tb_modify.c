/*	@(#)tb_modify.c	1.3	96/02/27 10:55:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_modify.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Source of the test for `b_modify()'.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_b_modify()

{
  errstat	err;
  bool		ok, ok1;
  capability	new_cap, tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_b_modify()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Don't modify an uncommitted file. The result should be committed. */
  err = b_modify(&uncommit_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1a")) {

    /* The capability should be an exact copy of the original one. */
    TEST_ASSERT(ok = cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1a: original capability expected\n", testname));

    /* Check the size and contents of the unmodified committed file. */
    err = b_size(&new_cap, &size);
    if (test_good(err, "1a, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1a, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
    }

    /* Clean up. */
    if (ok) {
      /* Recreate an uncommitted file. */
      err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
      (void)test_good(err, "1a, b_create()");
    }

    /* Destroy the new file. */
    err = std_destroy(&new_cap);
    (void)test_good(err, "1a, std_destroy()");
  }

  /* Modify one bit in a committed file. */
  write_buf[(int)AVG_OFFS] ^= (char)0x01;
  err = b_modify(&commit_cap, AVG_OFFS, write_buf + (int)AVG_OFFS, ONE,
							SAFE_COMMIT, &new_cap);
  if (test_good(err, "1b")) {

    /* The file should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1b: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1b, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1b: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1b, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
    }

    /* Clean up. */
    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1b, std_destroy()");
    }
  }
  write_buf[(int)AVG_OFFS] ^= (char)0x01;

  /* Modify an uncommitted file. The result should be committed. */
  err = b_modify(&uncommit_cap, AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (test_good(err, "1c")) {

    /* The capability should be an exact copy of the original one. */
    TEST_ASSERT(ok = cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1c: original capability expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1c, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1c: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1c, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1c: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_OFFS) == 0,
		TEST_SERIOUS,
		("%s, 1c: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (int)AVG_OFFS, write_buf,
				(size_t)(size-AVG_OFFS)) == 0, TEST_SERIOUS,
		("%s, 1c: data read don't match original data\n", testname));
    }

    /* Clean up. */
    if (ok) {
      /* Restore the original contents. */
      err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
      (void)test_good(err, "1c");
    }

    /* Destroy the new file. */
    err = std_destroy(&new_cap);
    (void)test_good(err, "1c, std_destroy()");
  }

  /* Modify an empty file. */
  err = b_modify(&empty_cap, ZERO, write_buf, AUX_SIZE, SAFE_COMMIT, &new_cap);
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
    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
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

  /* Modify at the end of a committed file. */
  err = b_modify(&commit_cap, AVG_SIZE, write_buf, AUX_SIZE, SAFE_COMMIT,
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
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_SIZE) == 0,
		TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
      TEST_ASSERT(memcmp(read_buf + (int)AVG_SIZE, write_buf,
				    (size_t)(size-AVG_SIZE))==0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1e, std_destroy()");
    }
  }

  /* Commit but don't modify a committed empty file. */
  err = b_modify(&empty_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1f")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &empty_cap), TEST_SERIOUS,
			("%s, 1f: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1f, b_size()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	    ("%s, 1f: size should be 0 but is %ld\n", testname, (long)size));

    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1f, b_read()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1f: bytes read should be 0 but is %ld\n", testname, (long)size));

    /* Clean up. */
    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1f, std_destroy()");
    }
  }

  /* Modify but don't commit a committed file with one byte. */
  err = b_modify(&one_cap, ZERO, write_buf+1, ONE, 0, &new_cap);
  if (test_good(err, "1g")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &one_cap), TEST_SERIOUS,
			("%s, 1g: new object expected\n", testname));

    /* You can't get the size and contents from an uncommitted file. */
    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1g, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1g, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1g")) {

      /* The capability should be the same as the uncommitted previous one and
       * refer to a different object than the original committed one.
       */
      TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1g: previous capability expected\n", testname));
      TEST_ASSERT(ok1 = !obj_cmp(&tmp_cap, &one_cap), TEST_SERIOUS,
			("%s, 1g: new object expected\n", testname));

      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1g, b_size()"))
	TEST_ASSERT(size == ONE, TEST_SERIOUS,
	    ("%s, 1g: size should be 1 but is %ld\n", testname, (long)size));

      memset(read_buf, 0, (size_t)AUX_SIZE);
      read_buf[0] = ~write_buf[1];
      err = b_read(&tmp_cap, ZERO, read_buf, AUX_SIZE, &size);
      if (test_good(err, "1g, b_read()")) {
	TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1g: bytes read should be 1 but is %ld\n", testname, (long)size));
	TEST_ASSERT(memcmp(read_buf, write_buf+1, (size_t)size) == 0,
		TEST_SERIOUS,
		("%s, 1g: data read don't match original data\n", testname));
      }

      if (ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1g, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1g, std_destroy()");
      }
    }
  }

  /* Modify an entire committed file. */
  err = b_modify(&commit_cap, ZERO, write_buf+1, AVG_SIZE, BS_COMMIT, &new_cap);
  if (test_good(err, "1h")) {

    /* The capability should be completely new. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1h: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1h, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1h: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1h, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1h: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf+1, (size_t)size) == 0,
		TEST_SERIOUS,
		("%s, 1h: data read don't match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1h, std_destroy()");
    }
  }

  /* Modify a file with only the minimum required rights. */
  err = std_restrict(&uncommit_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1i, std_restrict()")) {
    err = b_modify(&tmp_cap, AVG_OFFS, write_buf, AVG_SIZE, 0, &new_cap);
    if (test_good(err, "1i")) {

      /* The capability should be identical to the previous restricted one. */
      TEST_ASSERT(ok = cap_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1i: previous capability expected\n", testname));

      /* You can't get the size or contents from an uncommitted file. */
      err = b_size(&new_cap, &size);
      (void)test_bad(err, STD_CAPBAD, "1i, b_size()", "uncommitted file");

      err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
      (void)test_bad(err, STD_CAPBAD, "1i, b_read()", "uncommitted file");

      /* Now commit the file. */
      err = b_modify(&new_cap, AVG_OFFS, write_buf+1, AVG_SIZE, SAFE_COMMIT,
								&tmp_cap);
      if (test_good(err, "1i")) {
	/* The capability shouldn't change. */
	TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1i: previous capability expected\n", testname));
	TEST_ASSERT(ok1 = obj_cmp(&tmp_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1i: original object expected\n", testname));

	err = b_size(&tmp_cap, &size);
	if (test_good(err, "1i, b_size()"))
	  TEST_ASSERT(size == AVG_OFFS+AVG_SIZE, TEST_SERIOUS,
		("%s, 1i: size should be %ld but is %ld\n",
			testname, (long)(AVG_OFFS+AVG_SIZE), (long)size));

	memset(read_buf, 0, (size_t)BIG_SIZE);
	err = b_read(&tmp_cap, ZERO, read_buf, BIG_SIZE, &size);
	if (test_good(err, "1i, b_read()")) {
	  TEST_ASSERT(size == AVG_OFFS+AVG_SIZE, TEST_SERIOUS,
		("%s, 1i: bytes read should be %ld but is %ld\n",
			testname, (long)(AVG_OFFS+AVG_SIZE), (long)size));
	  TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)AVG_OFFS) == 0,
		TEST_SERIOUS,
		("%s, 1i: data read don't match original data\n", testname));
	  TEST_ASSERT(memcmp(read_buf + (int)AVG_OFFS, write_buf+1,
				(size_t)(size-AVG_OFFS)) == 0, TEST_SERIOUS,
		("%s, 1i: data read don't match original data\n", testname));
	}

	/* The current rights shouldn't permit destroying. */
	err = std_destroy(&tmp_cap);
	(void)test_bad(err, STD_DENIED, "1i, std_destroy()", "restricted cap");
      }

      /* Clean up. */
      if (ok || ok1) {
	err = std_destroy(&uncommit_cap);
	(void)test_good(err, "1i, std_destroy()");

	/* Restore the original contents. */
	err = b_create(&svr_cap, write_buf, AVG_SIZE, 0, &uncommit_cap);
	(void)test_good(err, "1i, b_create()");
      }
    }
  }

}  /* test_b_modify() */

