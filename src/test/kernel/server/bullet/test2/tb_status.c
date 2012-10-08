/*	@(#)tb_status.c	1.1	94/04/06 17:39:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_status
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * description	: tests for std_status
 *
 */

#include "Tginit.h"

PRIVATE void clean(a_file_cap, abuf)
capability * a_file_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_file_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_std_status() for ");
	bufprint(abuf, "a null file, err = %s\n", err_why(err));
    }
}

PUBLIC bool test_std_status(server, abuf)
char *server;
ABUFPRINT abuf;

{
    errstat err;
    bool ok, ok1;
    capability svr_cap, file_cap, new_cap;
    int size, new_size, tmp_size;
    char * write_buf, * read_buf;

    if (verbose) 
	bufprint(abuf,"--- test std_status() ---\n");

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory \n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!read_buf){
	bufprint(abuf,"not enough memory \n");
	free((_VOIDSTAR) write_buf);
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
	free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    ok = TRUE;
    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
    
    /* test with capability for a file */
    size = NEG_ONE;
    if (!(ok1 = (err = std_status(&file_cap, read_buf,
				  (int) K, &size))== STD_CAPBAD))
    {
	bufprint(abuf, "!!!Wrong result for std_status  with file ");
	bufprint(abuf, "capability , err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!!!Size is modified by std_status() for a file cap\n");
    }
    ok &= ok1;
    
    /* the file is no longer needed */
    clean(&file_cap, abuf);
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &svr_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_status(&new_cap, read_buf,
				  (int) K, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "!!!Wrong result for std_status() on corrupt ");
	bufprint(abuf, "check field, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!!!Size is modified by std_status() on ");
	bufprint(abuf, "corrupt check field\n");
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &svr_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_status(&new_cap, read_buf,
				  (int) K, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "!!!Wrong result for std_status() on corrupt ");
	bufprint(abuf, "rights field, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!!!Size is modified by std_status() on ");
	bufprint(abuf, "corrupt rights field\n");
    }
    ok &= ok1;
    
    /* tests with restricted rights */
    ok1 = execute_restrict(&svr_cap, &new_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			 "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY,  abuf);
    size = NEG_ONE;
    if (ok1 && !(ok1 &= (err = std_status(&new_cap, read_buf,
					  (int) K, &size)) == STD_DENIED))
    {
	bufprint(abuf, "!!!Wrong result for std_status() for capability ");
	bufprint(abuf, "resticted , err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!!!Size is modified by std_status for ");
	bufprint(abuf, "capability restricted\n"); 
    }
    ok &= ok1;
    
    /* test with good parameters */
    if (!(ok1 = (err = std_status(&svr_cap, read_buf,
				  (int) K, &size)) == STD_OK))
    {
	bufprint(abuf, "std_status() failed, err = %s\n", err_why(err));
    }
    ok &= ok1;
    if (ok1){
	if (verbose) {
	    bufprint(abuf, "buffer size = %d\n",size);
	    bufprint(abuf, "status for %s is : %s\n", server, read_buf);
	}
	(void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
	tmp_size = size - 800;
	if (!(ok1 = (err = std_status(&svr_cap, read_buf, tmp_size, 
				     &new_size)) == STD_OVERFLOW))
	{
	    bufprint(abuf, "std_status() failed, err = %s, size = %d\n",
		     err_why(err), tmp_size);
	} else {
	    if (new_size != size) {
		bufprint(abuf, "old size = %d, new size = %d\n",
			 size, new_size);
	    }
	}
	if (verbose) {
	    bufprint(abuf, "%ld characters from status for %s is %s\n",
		     tmp_size, server, read_buf);
	}
	
	ok &= ok1;
    }
    /* test with an invalid pointer for capability */
    
    abuf -> aident = &identl;
    if (!setjmp(*((jmp_buf *)thread_alloc(&identl, sizeof(jmp_buf))))) {
	if (!(ok1 = ((err = std_status((capability *) NULL, read_buf,
				       (int) K, &size)) == RPC_BADADDRESS)))
	{
	    bufprint(abuf, "!!!Wrong result for std_status  with ");
	    bufprint(abuf, "invalid pointer, err = %s", err_why(err));
	} else if (abuf -> flag != EXC_SYS) {
	    bufprint(abuf, "!!!Wrong result for std_status  with ");
	    bufprint(abuf, "invalid pointer, flag = %d",  abuf->flag);
	}
    }
    abuf -> flag = 0;
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!!!Size is modified by std_status with invalid ptr\n");
    }
    ok &= ok1;
    
    abuf -> aident = &identg;
    
    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
    return ok;
} /* test_std_staus.c() */

