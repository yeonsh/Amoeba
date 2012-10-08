/*	@(#)tb_read.c	1.3	96/02/27 10:55:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_read.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Source of the tests for `b_size()' and `b_read()'.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_b_size()
/* The name says it all. */

{
  b_fsize	size;
  errstat	err;
  capability	tmp_cap;


  (void)strcpy(testname, "test_b_size()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Get size of empty file. */
  err = b_size(&empty_cap, &size);
  if (test_good(err, "1a"))
    TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1a: size should be 0 but is %ld\n", testname, (long)size));

  /* Get size of file with one byte. */
  err = b_size(&one_cap, &size);
  if (test_good(err, "1b"))
    TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1b: size should be 1 but is %ld\n", testname, (long)size));

  /* Get size of normal committed file. */
  err = b_size(&commit_cap, &size);
  if (test_good(err, "1c"))
    TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1c: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));

  /* Get size with minimum required rights. */
  err = std_restrict(&commit_cap, BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1d, std_restrict()")) {
    err = b_size(&tmp_cap, &size);
    if (test_good(err, "1d"))
	TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1d: size should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
  }

}/* test_b_size() */




PUBLIC	void	test_b_read()

{
  errstat	err;
  capability	tmp_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_b_read()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Read some bytes from the beginning of an empty file. */
  err = b_read(&empty_cap, ZERO, read_buf, AVG_SIZE, &size);
  if (test_good(err, "1a"))
    TEST_ASSERT(size == ZERO, TEST_SERIOUS,
    ("%s, 1a: bytes read should be 0 but is %ld\n", testname, (long)size));

  /* Read some bytes from the beginning of a full file. */
  memset(read_buf, 0, (size_t)AUX_SIZE);
  err = b_read(&commit_cap, ZERO, read_buf, AUX_SIZE, &size);
  if (test_good(err, "1b")) {

    /* Check the size and contents of the file. */
    TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read do not match original data\n", testname));
  }

  /* Read all bytes from the middle of a full file. */
  memset(read_buf, 0, (size_t)AUX_SIZE);
  err = b_read(&commit_cap, AVG_OFFS, read_buf, AUX_SIZE, &size);
  if (test_good(err, "1c")) {
    TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1c: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf + (int)AVG_OFFS, (size_t)size) == 0,
  TEST_SERIOUS,("%s, 1c: data read do not match original data\n", testname));
  }

  /* Read 0 bytes from the middle of a full file. */
  err = b_read(&commit_cap, AVG_OFFS, read_buf, ZERO, &size);
  if (test_good(err, "1d"))
    TEST_ASSERT(size == ZERO, TEST_SERIOUS,
        ("%s, 1d: bytes read should be 0 but is %ld\n", testname, (long)size));

  /* Read some bytes from the end of a full file. */
  err = b_read(&commit_cap, AVG_SIZE, read_buf, AUX_SIZE, &size);
  if (test_good(err, "1e"))
    TEST_ASSERT(size == ZERO, TEST_SERIOUS,
        ("%s, 1e: bytes read should be 0 but is %ld\n", testname, (long)size));

  /* Read beyond the end from the middle of a full file. */
  memset(read_buf, 0, (size_t)AVG_SIZE);
  err = b_read(&commit_cap, AVG_OFFS, read_buf, AVG_SIZE, &size);
  if (test_good(err, "1f")) {
    TEST_ASSERT(size == AVG_SIZE-AVG_OFFS, TEST_SERIOUS,
			("%s, 1f: bytes read should be %ld but is %ld\n",
			    testname, (long)(AVG_SIZE-AVG_OFFS), (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf + (int)AVG_OFFS, (size_t)size) == 0,
  TEST_SERIOUS,("%s, 1f: data read do not match original data\n", testname));
  }

  /* Read an entire full file plus one extra byte. */
  memset(read_buf, 0, (size_t)(AVG_SIZE+ONE));
  err = b_read(&commit_cap, ZERO, read_buf, AVG_SIZE+ONE, &size);
  if (test_good(err, "1g")) {
    TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1g: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1g: data read do not match original data\n", testname));
  }

  /* Read one byte in the middle of a file. */
  memset(read_buf, 0, (size_t)AVG_SIZE);
  read_buf[0] = ~write_buf[(int)AVG_OFFS];
  err = b_read(&commit_cap, AVG_OFFS, read_buf, ONE, &size);
  if (test_good(err, "1h")) {
    TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1h: bytes read should be 1 but is %ld\n", testname, (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf + (int)AVG_OFFS, (size_t)size) == 0,
  TEST_SERIOUS,("%s, 1h: data read do not match original data\n", testname));
  }

  /* Read some bytes from a file with one byte. */
  memset(read_buf, 0, (size_t)AVG_SIZE);
  read_buf[0] = ~write_buf[0];
  err = b_read(&one_cap, ZERO, read_buf, AUX_SIZE, &size);
  if (test_good(err, "1i")) {
    TEST_ASSERT(size == ONE, TEST_SERIOUS,
	("%s, 1i: bytes read should be 1 but is %ld\n", testname, (long)size));
    TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1i: data read do not match original data\n", testname));
  }

  /* Read 0 bytes from an empty file. */
  err = b_read(&empty_cap, ZERO, read_buf, ZERO , &size);
  if (test_good(err, "1j"))
    TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	("%s, 1j: bytes read should be 0 but is %ld\n", testname, (long)size));

  /* Read an entire full file with the minimum required rights. */
  err = std_restrict(&commit_cap, BS_RGT_READ, &tmp_cap);
  if (test_good(err, "1k, std_restrict()")) {
    memset(read_buf, 0, (size_t)AVG_SIZE);
    err = b_read(&tmp_cap, ZERO, read_buf, AVG_SIZE, &size);
    if (test_good(err, "1k")) {
      TEST_ASSERT(size == AVG_SIZE, TEST_SERIOUS,
			("%s, 1k: bytes read should be %ld but is %ld\n",
				testname, (long)AVG_SIZE, (long)size));
      TEST_ASSERT(memcmp(read_buf, write_buf, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1k: data read do not match original data\n", testname));
    }
  }

}  /* test_b_read() */

