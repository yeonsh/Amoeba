/*	@(#)tb_restrict.c	1.1	94/04/06 17:39:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_restrict.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for std_restrict
 *
 */

#include "Tginit.h"

PRIVATE void clean(a_file_cap, abuf)
capability * a_file_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_file_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_std_restrict() ");
	bufprint(abuf, "for a commited file, err = %s\n",err_why(err));
    }
}

PUBLIC bool test_std_restrict(server, abuf)
char *server;
ABUFPRINT abuf;

{
    capability svr_cap, new_cap, tmp_cap, alt_cap, file_cap;
    bool ok;
    char * write_buf;

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory \n");
	return FALSE;
    }
    
    if (verbose) 
	bufprint(abuf, "--- test std_restrict() ---\n");
    
    /* initial conditions for tests */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) &&
	  init_buffer(write_buf, abuf, AVG_SIZE)))
    {
	free((_VOIDSTAR) write_buf);
	test_for_exception(abuf);
	return FALSE;
    }
    
    /* tests with wrong server capability  */
    ok = TRUE;
    if (svr_cap.cap_priv.prv_rights == BS_RGT_ALL)
	make_cap_rights_corrupt(&tmp_cap,&svr_cap);
    else
	tmp_cap = svr_cap;
    make_cap_check_corrupt(&new_cap, &tmp_cap);
    ok &= execute_restrict(&new_cap, &tmp_cap, BS_RGT_CREATE, 
			  "Server wrong check field", ! GOOD_CAPABILITY, abuf);
    make_cap_rights_corrupt(&new_cap, &svr_cap);
    ok &= execute_restrict(&new_cap, &tmp_cap, BS_RGT_CREATE, 
			 "Server wrong rights field", ! GOOD_CAPABILITY, abuf);
    
    /* tests with good server capability */
    ok &= execute_restrict(&svr_cap, &alt_cap, BS_RGT_CREATE | BS_RGT_READ,
		       "BS_RGT_CREATE | BS_RGT_READ", GOOD_CAPABILITY, abuf) &&
	  execute_restrict(&alt_cap, &new_cap, BS_RGT_READ | BS_RGT_MODIFY,
		        "BS_RGT_READ | BS_RGT_MODIFY", GOOD_CAPABILITY,  abuf);
    
    /* tests with corrupt capabilities for a file */
    if (!create_file(&svr_cap, write_buf, ZERO, SAFE_COMMIT, &file_cap, 
		     VERIFY_NEW_DIF_OLD, abuf))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    /* first we change the rights field, in this way std_restrict will
       check the field check                                          */
    make_cap_rights_corrupt(&tmp_cap, &file_cap);
    make_cap_check_corrupt(&new_cap, &tmp_cap);
    ok &= execute_restrict(&new_cap, &tmp_cap, BS_RGT_CREATE, 
			   "File wrong check field", ! GOOD_CAPABILITY, abuf);
    
    make_cap_rights_corrupt(&new_cap, &file_cap);
    ok &= execute_restrict(&new_cap, &tmp_cap, BS_RGT_CREATE, 
			 "File wrong rights field", ! GOOD_CAPABILITY, abuf) &&
	  execute_restrict(&file_cap, &alt_cap, BS_RGT_CREATE | BS_RGT_READ,
		      "BS_RGT_CREATE | BS_RGT_READ", GOOD_CAPABILITY,  abuf) &&
	  execute_restrict(&alt_cap, &new_cap, BS_RGT_READ | BS_RGT_MODIFY,
		      "BS_RGT_READ | BS_RGT_MODIFY", GOOD_CAPABILITY,  abuf);
    
    clean(&file_cap, abuf);
    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf);
    return ok;       
    
} /* test_std_restrict() */

