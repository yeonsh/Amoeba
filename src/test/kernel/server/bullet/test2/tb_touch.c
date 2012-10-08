/*	@(#)tb_touch.c	1.1	94/04/06 17:39:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_touch
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for std_touch
 *
 */

#include "Tginit.h"

PUBLIC bool test_std_touch(server, abuf)
char *server;
ABUFPRINT abuf;

{
    errstat err;
    bool  ok, ok1;
    capability svr_cap, new_cap, file_cap;
    char * write_buf;
    
    if (verbose) 
	bufprint(abuf,"--- test std_touch() ---\n");

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory\n");
	return FALSE;
    }
    
    /* initial conditions for test */
    init_exception_flag(abuf);  
    if (!(cap_server(&svr_cap, server, abuf) && 
	 init_buffer(write_buf, abuf, AVG_SIZE) &&
	 create_file(&svr_cap, write_buf, ZERO, SAFE_COMMIT, &file_cap, 
		     VERIFY_NEW_DIF_OLD, abuf)))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    ok = TRUE;
    /* test with capability for server */
    if (!(ok1 = (err = std_touch(&svr_cap)) == STD_COMBAD)) {
	bufprint(abuf, "!!!Wrong result for std_touch() with server ");
	bufprint(abuf, "capability,  err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &file_cap);
    if (!(ok1 = (err = std_touch(&new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!!!Wrong result for std_touch() on corrupt ");
	bufprint(abuf, "check field, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &file_cap);
    if (!(ok1 = (err = std_touch(&new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "!!!Wrong result for std_touch() on corrupt ");
	bufprint(abuf, "rights field,  err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* test with capability restricted */
    ok1 = execute_restrict(&file_cap, &new_cap, ZERO, "ZERO", 
			   GOOD_CAPABILITY,  abuf);
    if (ok1 && !(ok1 = (err = std_touch(&new_cap)) == STD_OK)) {
	bufprint(abuf, "std_touch() failed with all rights restricted\n");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    
    /* test with good parameters */
    if (!(ok1 = ((err = std_touch(&file_cap)) == STD_OK))) {
	bufprint(abuf, "std_touch() failed with all rights permited, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    ok &= std_destroy(&file_cap) == STD_OK;
    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf);
    return ok;
}  /* test_std_touch() */
