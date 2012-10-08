/*	@(#)tb_reject.c	1.2	94/04/06 17:37:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: tb_reject.c
 *
 * Original	: July 22, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Tests with invalid parameters that should be rejected.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"Tbullet.h"	/* Does all the other necessary includes. */




PUBLIC	void	test_bad_caps()

{
  int		i;
  errstat	err, err1, err2;
  bool		eq;
  capability	new_cap;
  b_fsize	size;
  char		argname[32];


  (void)strcpy(testname, "test_bad_caps()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Test with bad capabilities. */
  for (i = 0; i < nbadcaps; i++) {
    sprintf(argname, "bad_caps[%d]", i);

    err1 = b_size(&bad_caps[i], &size);
    (void)test_bad(err1, cap_err[i], "2a, b_size()", argname);

    err2 = b_read(&bad_caps[i], ZERO, read_buf, AUX_SIZE, &size);
    (void)test_bad(err2, cap_err[i], "2b, b_read()", argname);

    eq = err1 == err2;

    err1 = b_create(&bad_caps[i], write_buf, AUX_SIZE, SAFE_COMMIT, &new_cap);
    if (!test_bad(err1, cap_err[i], "2c, b_create()", argname)) {
      if (!obj_cmp(&new_cap, &bad_caps[i])) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2c, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err2 = b_modify(&bad_caps[i], AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err2, cap_err[i], "2d, b_modify()", argname)) {
      if (!obj_cmp(&new_cap, &bad_caps[i])) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2d, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err1 = b_delete(&bad_caps[i], AVG_OFFS, AUX_SIZE, SAFE_COMMIT, &new_cap);
    if (!test_bad(err1, cap_err[i], "2e, b_delete()", argname)) {
      if (!obj_cmp(&new_cap, &bad_caps[i])) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2e, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err2 = b_insert(&bad_caps[i], AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err2, cap_err[i], "2f, b_insert()", argname)) {
      if (!obj_cmp(&new_cap, &bad_caps[i])) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2f, std_destroy()");
      }
    }

    TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));
  }

}  /* test_bad_caps() */




PUBLIC	void	test_uncomm_file()

{
  errstat	err1, err2;
  b_fsize	size;


  (void)strcpy(testname, "test_uncomm_file()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Test with an uncommitted file. */
  err1 = b_size(&uncommit_cap, &size);
  (void)test_bad(err1, STD_CAPBAD, "2a, b_size()", "uncommitted file");

  err2 = b_read(&uncommit_cap, AVG_OFFS, read_buf, AUX_SIZE, &size);
  (void)test_bad(err2, STD_CAPBAD, "2b, b_read()", "uncommitted file");

  TEST_ASSERT(err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_uncomm_file() */




PUBLIC	void	test_bad_rights()

{
  errstat	err, err1, err2;
  bool		eq = TRUE;
  capability	tmp_cap, new_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_bad_rights()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Test with insufficient rights. */
  err = std_restrict(&commit_cap, ~BS_RGT_READ, &tmp_cap);
  if (test_good(err, "2a, std_restrict()")) {
    err1 = b_size(&tmp_cap, &size);
    (void)test_bad(err1, STD_DENIED, "2a, b_size()", "bad rights");

    err2 = b_read(&tmp_cap, AVG_OFFS, read_buf, AUX_SIZE, &size);
    (void)test_bad(err2, STD_DENIED, "2b, b_read()", "bad rights");

    eq = err1 == err2;

    err1 = b_modify(&tmp_cap, AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err1, STD_DENIED, "2c, b_modify()", "bad rights")){
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2c, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err2 = b_delete(&tmp_cap, AVG_OFFS, AUX_SIZE, SAFE_COMMIT, &new_cap);
    if (!test_bad(err2, STD_DENIED, "2d, b_delete()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2d, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err1 = b_insert(&tmp_cap, AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err1, STD_DENIED, "2e, b_insert()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2e, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);
  }

  err = std_restrict(&commit_cap, ~BS_RGT_CREATE, &tmp_cap);
  if (test_good(err, "2f, std_restrict()")) {
    err2 = b_create(&tmp_cap, write_buf, AUX_SIZE, SAFE_COMMIT, &new_cap);
    if (!test_bad(err2, STD_DENIED, "2f, b_create()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2f, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);
  }

  err = std_restrict(&commit_cap, ~BS_RGT_MODIFY, &tmp_cap);
  if (test_good(err, "2g, std_restrict()")) {
    err1 = b_modify(&tmp_cap, AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err1, STD_DENIED, "2g, b_modify()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2g, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err2 = b_delete(&tmp_cap, AVG_OFFS, AUX_SIZE, SAFE_COMMIT, &new_cap);
    if (!test_bad(err2, STD_DENIED, "2h, b_delete()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2h, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    err1 = b_insert(&tmp_cap, AVG_OFFS, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
    if (!test_bad(err1, STD_DENIED, "2i, b_insert()", "bad rights")) {
      if (!obj_cmp(&new_cap, &tmp_cap)) {
	err = std_destroy(&new_cap);
	(void)test_good(err, "2i, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);
  }

  TEST_ASSERT(eq, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_bad_rights() */




PUBLIC	void	test_bad_size()

{

  errstat     err, err1, err2;
  bool	eq;
  capability  new_cap;
  b_fsize     size;


  (void)strcpy(testname, "test_bad_size()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Test with bad filesize -1. */
  err1 = b_read(&commit_cap, AVG_OFFS, read_buf, NEG_ONE, &size);
  (void)test_bad(err1, STD_ARGBAD, "2a, b_read()", "size -1");

  err2 = b_create(&commit_cap, write_buf, NEG_ONE, SAFE_COMMIT, &new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2b, b_create()", "size -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2b, std_destroy()");
    }
  }

  eq = err1 == err2;

  err1 = b_modify(&commit_cap, AVG_OFFS, write_buf, NEG_ONE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2c, b_modify()", "size -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2c, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = b_delete(&commit_cap, AVG_OFFS, NEG_ONE, SAFE_COMMIT, &new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2d, b_delete()", "size -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2d, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_insert(&commit_cap, AVG_OFFS, write_buf, NEG_ONE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2e, b_insert()", "size -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2e, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  /* Test with bad filesize 0. */
  err2 = b_delete(&commit_cap, AVG_OFFS, ZERO, SAFE_COMMIT, &new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2f, b_delete()", "size 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2f, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_insert(&commit_cap, AVG_OFFS, write_buf, ZERO, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2g, b_insert()", "size 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2g, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = err1;

  /* Test with big negative filesizes. */
  err1 = b_read(&commit_cap, AVG_OFFS, read_buf, NEG_AVG, &size);
  (void)test_bad(err1, STD_ARGBAD, "2h, b_read()", "size < 0");

  eq = eq && (err1 == err2);

  err2 = b_create(&commit_cap, write_buf, NEG_AVG, SAFE_COMMIT, &new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2i, b_create()", "size < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2i, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_modify(&commit_cap, AVG_OFFS, write_buf, NEG_AVG, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2j, b_modify()", "size < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2j, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = b_delete(&commit_cap, AVG_OFFS, NEG_AVG, SAFE_COMMIT, &new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2k, b_delete()", "size < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2k, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_insert(&commit_cap, AVG_OFFS, write_buf, NEG_AVG, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2l, b_insert()", "size < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2l, std_destroy()");
    }
  }

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_bad_size() */




PUBLIC	void	test_bad_offsets()

{

  errstat     err, err1, err2;
  bool	eq;
  capability  new_cap;
  b_fsize     size;


  (void)strcpy(testname, "test_bad_offsets()");
  if (verbose)  printf("\n----  %s  ----\n", testname);


  /* Test with bad offset -1. */
  err1 = b_read(&commit_cap, NEG_ONE, read_buf, AUX_SIZE, &size);
  (void)test_bad(err1, STD_ARGBAD, "2a, b_read()", "offset -1");

  err2 = b_modify(&commit_cap, NEG_ONE, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2b, b_modify()", "offset -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2b, std_destroy()");
    }
  }

  eq = err1 == err2;

  err1 = b_delete(&commit_cap, NEG_ONE, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2c, b_delete()", "offset -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2c, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = b_insert(&commit_cap, NEG_ONE, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2d, b_insert()", "offset -1")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2d, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  /* Test with bad offsets bigger than filesize. */
  err1 = b_read(&commit_cap, AVG_SIZE+ONE, read_buf, AUX_SIZE, &size);
  (void)test_bad(err1, STD_ARGBAD, "2e, b_read()", "offset > filesize");

  eq = eq && (err1 == err2);

  err2 = b_modify(&commit_cap, AVG_SIZE+ONE, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2f, b_modify()", "offset > filesize")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2f, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_delete(&commit_cap, AVG_SIZE+ONE, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2g, b_delete()", "offset > filesize")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2g, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = b_insert(&commit_cap, AVG_SIZE+ONE, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2h, b_insert()", "offset > filesize")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2h, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  /* Test with big negative offsets. */
  err1 = b_read(&commit_cap, NEG_AVG, read_buf, AUX_SIZE, &size);
  (void)test_bad(err1, STD_ARGBAD, "2i, b_read()", "offset < 0");

  eq = eq && (err1 == err2);

  err2 = b_modify(&commit_cap, NEG_AVG, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2j, b_modify()", "offset < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2j, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err1 = b_delete(&commit_cap, NEG_AVG, AUX_SIZE, SAFE_COMMIT, &new_cap);
  if (!test_bad(err1, STD_ARGBAD, "2k, b_delete()", "offset < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2k, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  err2 = b_insert(&commit_cap, NEG_AVG, write_buf, AUX_SIZE, SAFE_COMMIT,
								&new_cap);
  if (!test_bad(err2, STD_ARGBAD, "2l, b_insert()", "offset < 0")) {
    if (!obj_cmp(&new_cap, &commit_cap)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2l, std_destroy()");
    }
  }

  eq = eq && (err1 == err2);

  TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
			("%s (inconsistent error codes)\n", testname));

}  /* test_bad_offsets() */




PUBLIC	void	test_bad_many()

{
  errstat     err;
  capability  new_cap;
  b_fsize     size;


  (void)strcpy(testname, "test_bad_many()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Test with multiple bad arguments. */
  err = b_read(bad_caps, NEG_ONE, read_buf, NEG_ONE, &size);
  (void)test_bad(err, STD_OK, "2a, b_read()", "many arguments");

  err = b_create(bad_caps, write_buf, NEG_ONE, 0x00, &new_cap);
  if (!test_bad(err, STD_OK, "2b, b_create()", "many arguments")) {
    if (!obj_cmp(&new_cap, bad_caps)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2b, std_destroy()");
    }
  }

  err = b_modify(bad_caps, NEG_ONE, write_buf, NEG_ONE, 0x00, &new_cap);
  if (!test_bad(err, STD_OK, "2c, b_modify()", "many arguments")) {
    if (!obj_cmp(&new_cap, bad_caps)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2c, std_destroy()");
    }
  }

  err = b_delete(bad_caps, NEG_ONE, NEG_ONE, 0x00, &new_cap);
  if (!test_bad(err, STD_OK, "2d, b_delete()", "many arguments")) {
    if (!obj_cmp(&new_cap, bad_caps)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2d, std_destroy()");
    }
  }

  err = b_insert(bad_caps, NEG_ONE, write_buf, NEG_ONE, 0x00, &new_cap);
  if (!test_bad(err, STD_OK, "2e, b_insert()", "many arguments")) {
    if (!obj_cmp(&new_cap, bad_caps)) {
      err = std_destroy(&new_cap);
      (void)test_good(err, "2e, std_destroy()");
    }
  }

}  /* test_bad_many() */

