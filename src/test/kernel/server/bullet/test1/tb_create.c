/*	@(#)tb_create.c	1.3	96/02/27 10:54:52 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_create.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Source of the test for `b_create()'.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_b_create()

{
  errstat	err;
  bool		ok, ok1;
  capability	new_cap, tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_b_create()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Create a new file. */
  err = b_create(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1a")) {
    /* Capability should be for a new object. */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &svr_cap), TEST_SERIOUS,
			("%s, 1a: new object expected\n", testname));

    /* Verify size and contents. */
    err = b_size(&new_cap, &size);
    if (test_good(err, "1a, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);	/* Clear read buffer. */
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1a, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1a: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1a: data read do not match original data\n", testname));
    }

    /* Clean up possible debris. */
    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1a, std_destroy()");
    }
  }

  /* Create a new file with allmost the same size as the original. */
  err = b_create(&commit_cap, write_buf, AVG_SIZE-ONE, BS_COMMIT, &new_cap);
  if (test_good(err, "1b")) {
    /* New object. */
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
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read do not match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1b, std_destroy()");
    }
  }

  /* Create a new file with allmost the same size as the original. */
  err = b_create(&commit_cap, write_buf, AVG_SIZE+ONE, BS_COMMIT, &new_cap);
  if (test_good(err, "1c")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1c: new capability expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1c, b_size()"))
      TEST_ASSERT(size == AVG_SIZE+ONE, TEST_SERIOUS,
			("%s, 1c: size should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE+ONE), (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1c, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE+ONE, TEST_SERIOUS,
			("%s, 1c: bytes read should be %ld but is %ld\n",
				testname, (long)(AVG_SIZE+ONE), (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1c: data read do not match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1c, std_destroy()");
    }
  }

  /* Create a new file with allmost the same contents as the original. */
  write_buf[(int)AVG_OFFS] ^= (byte)0x01;	/* Change one bit. */
  err = b_create(&commit_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1d")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1d: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1d, b_size()"))
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1d: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

    memset(read_buf, 0, (size_t)BIG_SIZE);
    err = b_read(&new_cap, ZERO, read_buf, BIG_SIZE, &size);
    if (test_good(err, "1d, b_read()")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1d: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1d: data read do not match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1d, std_destroy()");
    }
  }
  write_buf[(int)AVG_OFFS] ^= (byte)0x01;	/* Restore bit. */

  /* Create a new file with exactly the same contents as the original. */
  err = b_create(&uncommit_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1e")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
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
		("%s, 1e: data read do not match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1e, std_destroy()");
    }
  }

  /* Create an empty file. */
  err = b_create(&commit_cap, write_buf, ZERO, SAFE_COMMIT, &new_cap);
  if (test_good(err, "1f")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1f: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1f, b_size()"))
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1f: size should be 0 but is %ld\n", testname, (long)size));

    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1f, b_read()")) {
      TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1f: bytes read should be 0 but is %ld\n", testname, (long)size));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1f, std_destroy()");
    }
  }

  /* Create an allmost empty file. */
  err = b_create(&uncommit_cap, write_buf, ONE, BS_COMMIT, &new_cap);
  if (test_good(err, "1g")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1g: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    if (test_good(err, "1g, b_size()"))
      TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1g: size should be 1 but is %ld\n", testname, (long)size));

    memset(read_buf, 0, (size_t)AUX_SIZE);
    read_buf[0] = ~write_buf[0];
    err = b_read(&new_cap, ZERO, read_buf, AUX_SIZE, &size);
    if (test_good(err, "1g, b_read()")) {
      TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1g: bytes read should be 1 but is %ld\n", testname, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1g: data read do not match original data\n", testname));
    }

    if (ok) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "1g, std_destroy()");
    }
  }

  /* Create an existing committed file. */
  err = std_restrict(&commit_cap, BS_RGT_CREATE | BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1h, std_restrict()")) {
    err = b_create(&tmp_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (test_good(err, "1h")) {

      /* The new capability should be the same as the restricted one and
       * refer to the same object as the unrestricted original one.
       */
      TEST_ASSERT(cap_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1h: previous capability expected\n", testname));
      TEST_ASSERT(ok = obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1h: original object expected\n", testname));

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
	TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1h: data read do not match original data\n", testname));
      }

      if (!ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1h, std_destroy()");
      }
    }
  }

  /* Create an uncommitted file with the same contents as the original. */
  err = b_create(&commit_cap, write_buf, AVG_SIZE, 0, &new_cap);
  if (test_good(err, "1i")) {

    /* The capability must be for a completely new object (file). */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1i: new object expected\n", testname));

    /* You can't get the size or read from an uncommitted file. */
    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1i, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1i, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1i, b_modify()")) {

      /* The capability should be the same as the original committed one and
       * refer to a different object than the previous uncommitted one.
       */
      TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &commit_cap), TEST_SERIOUS,
			("%s, 1i: original capability expected\n", testname));
      TEST_ASSERT(!obj_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1i: different object expected\n", testname));

      /* Verify the size and the contents. */
      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1i, b_size()"))
	TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1i: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

      memset(read_buf, 0, (size_t)BIG_SIZE);
      err = b_read(&tmp_cap, ZERO, read_buf, BIG_SIZE, &size);
      if (test_good(err, "1i, b_read()")) {
	TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1i: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
	TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1i: data read do not match original data\n", testname));
      }

      if (!ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1i, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1i, std_destroy()");
      }
    }
  }

  /* Create an empty uncommitted file. */
  err = b_create(&empty_cap, write_buf, ZERO, BS_SAFETY, &new_cap);
  if (test_good(err, "1j")) {

    /* The capability must be for a completely new object (file). */
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &empty_cap), TEST_SERIOUS,
			("%s, 1j: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1j, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1j, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1j")) {

      /* The capability should be the same as the original committed one and
       * refer to a different object than the previous uncommitted one.
       */
      TEST_ASSERT(ok1 = cap_cmp(&tmp_cap, &empty_cap), TEST_SERIOUS,
			("%s, 1i: original capability expected\n", testname));
      TEST_ASSERT(!obj_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1i: different object expected\n", testname));

      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1j, b_size()"))
	TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	    ("%s, 1j: size should be 0 but is %ld\n", testname, (long)size));

      err = b_read(&tmp_cap, ZERO, read_buf, AUX_SIZE, &size);
      if (test_good(err, "1j, b_read()")) {
	TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1j: bytes read should be 0 but is %ld\n", testname, (long)size));
      }

      if (!ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1j, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1j, std_destroy()");
      }
    }
  }

  /* Create an allmost empty uncommitted file. */
  err = b_create(&uncommit_cap, write_buf, ONE, 0, &new_cap);
  if (test_good(err, "1k")) {
    TEST_ASSERT(ok = !obj_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1k: new object expected\n", testname));

    err = b_size(&new_cap, &size);
    (void)test_bad(err, STD_CAPBAD, "1k, b_size()", "uncommitted file");

    err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
    (void)test_bad(err, STD_CAPBAD, "1k, b_read()", "uncommitted file");

    /* Now commit the file. */
    err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (test_good(err, "1k")) {

      /* The capability should be the same as the previous uncommitted one and
       * refer to a different object than the original uncommitted one.
       */
      TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1k: previous capability expected\n", testname));
      TEST_ASSERT(ok1 = !obj_cmp(&tmp_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1k: new object expected\n", testname));

      err = b_size(&tmp_cap, &size);
      if (test_good(err, "1k, b_size()"))
	TEST_ASSERT(size == ONE, TEST_SERIOUS,
	    ("%s, 1k: size should be 1 but is %ld\n", testname, (long)size));

      memset(read_buf, 0, (size_t)AUX_SIZE);
      read_buf[0] = ~write_buf[0];
      err = b_read(&tmp_cap, ZERO, read_buf, AUX_SIZE, &size);
      if (test_good(err, "1k, b_read()")) {
	TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1k: bytes read should be 1 but is %ld\n", testname, (long)size));
	TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1k: data read do not match original data\n", testname));
      }

      if (ok1) {
	err = std_destroy(&tmp_cap);
	(void)test_good(err, "1k, std_destroy()");
      }
    }
    else {
      if (ok) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "1k, std_destroy()");
      }
    }
  }

  /* Create an uncommitted file with restricted rights. */
  err = std_restrict(&uncommit_cap, BS_RGT_CREATE | BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1l, std_restrict()")) {
    err = b_create(&tmp_cap, write_buf, AVG_SIZE, 0, &new_cap);
    if (test_good(err, "1l")) {
      TEST_ASSERT(ok = !obj_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1l: new object expected\n", testname));

      err = b_size(&new_cap, &size);
      (void)test_bad(err, STD_CAPBAD, "1l, b_size()", "uncommitted file");

      err = b_read(&new_cap, ZERO, read_buf, AVG_SIZE, &size);
      (void)test_bad(err, STD_CAPBAD, "1l, b_read()", "uncommitted file");

      /* Now commit the file. */
      err = b_modify(&new_cap, ZERO, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
      if (test_good(err, "1l, b_modify()")) {

	/* The capability should be the same as the previous uncommitted one
	 * and refer to a different object than the original uncommitted one.
	 */
	TEST_ASSERT(cap_cmp(&tmp_cap, &new_cap), TEST_SERIOUS,
			("%s, 1l: previous capability expected\n", testname));
	TEST_ASSERT(ok1 = !obj_cmp(&tmp_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1l: new object expected\n", testname));

	err = b_size(&tmp_cap, &size);
	if (test_good(err, "1l, b_size()"))
	  TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1l: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

	memset(read_buf, 0, (size_t)BIG_SIZE);
	err = b_read(&tmp_cap, ZERO, read_buf, BIG_SIZE, &size);
	if (test_good(err, "1l, b_read()")) {
	  TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1l: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
	  TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1l: data read do not match original data\n", testname));
	}

	if (ok1) {
	  err = std_destroy(&tmp_cap);
	  (void)test_good(err, "1l, std_destroy()");
	}
      }
      else {
	if (ok) {
	  err = std_destroy(&new_cap);
	  (void)test_good(err, "1j, std_destroy()");
	}
      }
    }
  }

}  /* test_b_create() */

