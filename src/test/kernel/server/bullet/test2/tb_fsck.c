/*	@(#)tb_fsck.c	1.1	94/04/06 17:38:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_fsck.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_fsck
 *
 */

#include "Tginit.h"

PUBLIC bool test_b_fsck(server, abuf)
char *server;
ABUFPRINT abuf;
{
    errstat err;
    bool ok, ok1;
    capability svr_cap, file_cap, tmp_cap;
    char * write_buf;

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf) {
	bufprint(abuf,"not enough memory \n");
	return FALSE;
    }
    
    if (verbose) 
	bufprint(abuf,"--- test b_fsck() ---\n");
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, AVG_SIZE) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &file_cap,
		      VERIFY_NEW_DIF_OLD, abuf)))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf); 
	return FALSE;
    }
    ok = TRUE;
    
    /* test with server capability */
    if (!(ok1 = ((err = b_fsck(&svr_cap)) == STD_OK))) {
	bufprint(abuf, "!Wrong result for b_fsck with server cap, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    /* test with all rights restricted */
    ok &= execute_restrict(&svr_cap, &tmp_cap, ZERO, "ZERO", 
			   GOOD_CAPABILITY,  abuf);
    if (!(ok1 = (err = b_fsck(&tmp_cap)) == STD_DENIED)) {
	bufprint(abuf, "!Wrong result for b_fsck with rights restricted, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with bad capabilities */
    make_cap_rights_corrupt(&tmp_cap, &svr_cap);
    if (!(ok1 = (err = b_fsck(&tmp_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!Wrong result for b_fsck with corrupted rights, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_check_corrupt(&tmp_cap, &svr_cap);
    if (!(ok1 = (err = b_fsck(&tmp_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!Wrong result for b_fsck with corrupted checkfield, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with capability of a file */
    if (!(ok1 = (err = b_fsck(&file_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!Wrong result for b_fsck of file cap, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    /* tests with bad capabilities for files */
    make_cap_rights_corrupt(&tmp_cap, &file_cap);
    if (!(ok1 = (err = b_fsck(&tmp_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!Wrong result for b_fsck with corrupted rights, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    make_cap_check_corrupt(&tmp_cap, &file_cap);
    if (!(ok1 = (err = b_fsck(&tmp_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!Wrong result for b_fsck with corrupted checkfield, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1 && std_destroy(&file_cap) == STD_OK;
    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf); 
    return ok;
} /* test_b_fsck() */


