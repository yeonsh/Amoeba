/*	@(#)tb_size.c	1.1	94/04/06 17:39:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_size.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_size
 *
 */

#include "Tginit.h"


PUBLIC bool test_b_size(server, abuf)
char *server;
ABUFPRINT abuf;

{
    errstat err;
    bool ok, ok1;
    b_fsize size;
    capability svr_cap, new_cap, null_cap, commit_cap, uncommit_cap;
    char * write_buf;

    if (verbose) 
	bufprint(abuf,"--- test b_size() ---\n");
    
    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory\n");
	return FALSE;
    }
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, AVG_SIZE) &&
	  create_file(&svr_cap, write_buf, ZERO, SAFE_COMMIT, &null_cap, 
		      VERIFY_NEW_DIF_OLD, abuf) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &commit_cap,
		      VERIFY_NEW_DIF_OLD, abuf) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT, &uncommit_cap,
		      VERIFY_NEW_DIF_OLD, abuf))) 
    {
	bufprint(abuf, "could not create initial files\n");
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    ok = TRUE;
    
    /* test with capability for server */
    size = NEG_ONE;
    if (!(ok1 = ((err = b_size(&svr_cap, &size)) == STD_COMBAD) &&
	 (size == NEG_ONE)))
    {
	bufprint(abuf, "wrong result for b_size() with server cap");
	bufprint(abuf, "err = %s, old size = %d, new size = %d\n",
		 err_why(err), NEG_ONE, size); 
    }
    ok &= ok1;
    
    /* tests with bad capabilities */
    size = NEG_ONE;
    make_cap_check_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = ((err = b_size(&new_cap, &size)) == STD_CAPBAD) &&
	 (size == NEG_ONE)))
    {
	bufprint(abuf, "wrong result for b_size() on corrupt check field, ");
	bufprint(abuf, "err = %s, old size = %d, new size = %d\n",
		 err_why(err), NEG_ONE, size);
    }
    ok &= ok1;
    
    size = NEG_ONE;
    make_cap_rights_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = ((err = b_size(&new_cap, &size)) == STD_CAPBAD) &&
	 (size == NEG_ONE)))
    {
	bufprint(abuf, "wrong result for b_size() on corrupt rights ");
	bufprint(abuf, "field, err = %s, ", err_why(err));
	bufprint(abuf, "old size = %d, new size = %d\n", NEG_ONE, size);
    }
    ok &= ok1;
    
    /* test on an uncommit file */
    size = NEG_ONE;
    if (!(ok1 = ((err = b_size(&uncommit_cap, &size)) == STD_CAPBAD)&&
	 (size == NEG_ONE)))
    {
	bufprint(abuf, "wrong result for b_size() on an uncommited ");
	bufprint(abuf, "file, err = %s\n", err_why(err));
	bufprint(abuf, "old size = %d, new size = %d\n", NEG_ONE, size);
    }
    ok &= ok1;
    
    /* test on a restricted file */
    size = NEG_ONE;
    ok1 = execute_restrict(&commit_cap, &new_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			  "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY, abuf);
    
    size = NEG_ONE;
    if (ok1 && !(ok1 = ((err = b_size(&new_cap, &size)) == STD_DENIED) &&
		(size == NEG_ONE)))
    {
	bufprint(abuf, "b_size() failed for a commited file restricted\n");
	bufprint(abuf, "err = %s,size = %d\n",err_why(err), size);
	bufprint(abuf, "old size = %d, new size = %d\n", NEG_ONE, size);
    }
    ok &= ok1;
    
    /* test with good arguments */
    size = NEG_ONE;
    if (!(ok1 = ((err = b_size(&null_cap, &size)) == STD_OK) &&(size == ZERO)))
    {
	bufprint(abuf, "b_size() failed for a commited file with ");
	bufprint(abuf, "%d bytes, err = %s, size = %d\n",
		 ZERO, err_why(err), size);
    }
    ok &= ok1;
    
    size = NEG_ONE;
    if (!(ok1 = ((err = b_size(&commit_cap, &size)) == STD_OK) && 
	  (size == AVG_SIZE)))
    {
	bufprint(abuf, "b_size() failed for a commited file with ");
	bufprint(abuf, "%d bytes, err = %s, size = %d\n",
		 AVG_SIZE, err_why(err), size);
    }
    ok &= ok1;
    
    /* destroy files created for test */
    if (! ((err = std_destroy(&null_cap)) == STD_OK) &&
	  ((err = std_destroy(&commit_cap)) == STD_OK) &&
	  ((err = std_destroy(&uncommit_cap)) == STD_OK))
    {
	bufprint(abuf, "could not destroy files (%s)\n", err_why(err));
    }

    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf); 
    return ok;
} /* test_b_size() */



