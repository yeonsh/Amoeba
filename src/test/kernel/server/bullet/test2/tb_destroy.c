/*	@(#)tb_destroy.c	1.1	94/04/06 17:38:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_destroy.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for std_destroy
 *
 */

#include "Tginit.h"

PUBLIC bool test_std_destroy(server, abuf)
char *server;
ABUFPRINT abuf;

{
    errstat err;
    bool ok, ok1;
    capability svr_cap, file_cap, alt_file_cap, new_cap;
    char * write_buf;

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory\n");
	return FALSE;
    }
    
    ok = TRUE;
    if (verbose) 
	bufprint(abuf, "--- test std_destroy() ---\n");
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, AVG_SIZE) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT,
		      &alt_file_cap, VERIFY_NEW_DIF_OLD, abuf) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &file_cap,
		      VERIFY_NEW_DIF_OLD, abuf)))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf); 
	return FALSE;
    }
    /* test with capability for server */
    if (!(ok1 = (err = std_destroy(&svr_cap)) == STD_COMBAD)) {
	bufprint(abuf, " wrong result for std_destroy() with server ");
	bufprint(abuf, "capability, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with bad capability */
    make_cap_check_corrupt(&new_cap, &file_cap);
    if (!(ok1 = (err = std_destroy(&new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "wrong result for std_destroy on corrupt ");
	bufprint(abuf, "check field, err = %s\n", err_why(err));
    }
    ok &= ok1;
    make_cap_rights_corrupt(&new_cap, &file_cap);
    if (!(ok1 = (err = std_destroy(&new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "wrong result for std_destroy() on corrupt ");
	bufprint(abuf, "rights field, err = %s\n",err_why(err));
    }
    ok &= ok1;
    
    /* test with capability restrcted for a file */
    ok1 = execute_restrict(&file_cap, &new_cap, ALL_BS_RIGHT ^ BS_RGT_DESTROY,
		       "ALL_BS_RIGHT ^ BS_RGT_DESTROY", GOOD_CAPABILITY, abuf);
    if (ok1 && !(ok1 = (err = std_destroy(&new_cap)) == STD_DENIED)) {
	bufprint(abuf, "wrong result for std_destroy() with capability ");
	bufprint(abuf, "restricted,  err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with good capabilities */
    ok1 = execute_restrict(&file_cap, &new_cap, BS_RGT_DESTROY,
			   "BS_RGT_DESTROY", GOOD_CAPABILITY,  abuf);
    if (ok1 && !(ok1 = (err = std_destroy(&new_cap)) == STD_OK)) {
	bufprint(abuf, "std_destroy() failed for good capabilitiy ");
	bufprint(abuf, "restricted,  err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* was the file destroyed ? */
    if (!(ok1 = (err = std_touch(&new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "std_touch() after a std_destroy(), err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = std_destroy(&alt_file_cap)) == STD_OK)) {
	bufprint(abuf, "std_destroy() failed for good capability ");
	bufprint(abuf, "unrestricted, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* was the second file destroyed ? */
    if (!(ok1 = (err = std_destroy(&alt_file_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "std_destroy() after a std_destroy(), err = %s",
		 err_why(err));
    }
    ok &= ok1;
    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf); 
    return ok;
} /* test_std_destroy() */

