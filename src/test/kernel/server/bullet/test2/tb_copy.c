/*	@(#)tb_copy.c	1.1	94/04/06 17:38:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_copy.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for std_copy
 *
 */

#include "Tginit.h"

extern char *Alternate_server;

PRIVATE void clean(a_file_cap, abuf)
capability * a_file_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_file_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_std_copy() for a ");
	bufprint(abuf, "not null file, err = %s\n", err_why(err));
    }
}

PUBLIC bool test_std_copy(server, abuf)
char *server;
ABUFPRINT abuf;

{
    errstat err;
    bool ok, ok1;
    capability svr_cap, alt_svr_cap, new_cap, tmp_cap, file_cap;
    char * write_buf, * read_buf;

    write_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!write_buf){
	bufprint(abuf, "not enough memory\n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!read_buf){
	bufprint(abuf, "not enough memory\n");
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    if (verbose) 
	bufprint(abuf, "--- test std_copy() ---\n");
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, BIG_SIZE) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &file_cap,
		      VERIFY_NEW_DIF_OLD, abuf)))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    ok = TRUE;
    
    /* tests with capability for server corrupted */
    make_cap_check_corrupt(&tmp_cap, &svr_cap);
    if (!(ok1 = (err = std_copy(&tmp_cap, &file_cap, &new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_copy() with check field ");
	bufprint(abuf, "corrupted for the server, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&tmp_cap, &svr_cap);
    if (!(ok1 = (err = std_copy(&tmp_cap, &file_cap,&new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_copy with rights corrupted ");
	bufprint(abuf, "for the server, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* server capability restricted without CREATE rights */
    ok1 = execute_restrict(&svr_cap, &tmp_cap, ALL_BS_RIGHT ^ BS_RGT_CREATE,
			"ALL_BS_RIGHT ^ BS_RGT_CREATE", GOOD_CAPABILITY, abuf);
    if (ok1 && !(ok1 = ((err = std_copy(&tmp_cap, &file_cap,
					&new_cap)) == STD_DENIED)))
    {
	bufprint(abuf, "wrong result for std_copy() without CREATE ");
	bufprint(abuf, "capability for the server, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* test with capability of the server for file */
    if (!(ok1 = (err = std_copy(&svr_cap, &svr_cap, &new_cap)) == STD_COMBAD))
    {
	bufprint(abuf, "wrong result for std_copy() with capability ");
	bufprint(abuf, "of the sever for file, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with capability of the file corrupted */
    make_cap_check_corrupt(&tmp_cap, &file_cap);
    if (!(ok1 = (err = std_copy(&svr_cap,&tmp_cap, &new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "wrong result for std_copy() with check of ");
	bufprint(abuf, "the file corrupted, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&tmp_cap, &file_cap);
    if (!(ok1 = (err = std_copy(&svr_cap,&tmp_cap, &new_cap)) == STD_CAPBAD)) {
	bufprint(abuf, "wrong result for std_copy() with rights of ");
	bufprint(abuf, "the file corrupted, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* test with file capability restricted */
    ok1 = execute_restrict(&file_cap, &tmp_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			  "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY, abuf);
    
    if (ok1 && !(ok1 = ((err = std_copy(&svr_cap, &tmp_cap,
					&new_cap)) == STD_DENIED)))
    {
	bufprint(abuf, "wrong result for std_copy() with file capability ");
	bufprint(abuf, "restricted, err = %s\n", err_why(err));
    }
    ok &= ok1; 
    
    /* tests with good parameters */
    if (!(ok1 = (err = std_copy(&svr_cap, &file_cap, &new_cap)) == STD_OK)) {
	bufprint(abuf, "std_copy() failed, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    ok &= verify_file(write_buf, read_buf, AVG_SIZE, ZERO, &new_cap, 
		      EQUAL_SIZE, AVG_SIZE, abuf);
    
    if (!(ok1 = !cap_cmp(&file_cap, &new_cap))) {
	bufprint(abuf, "std_copy() failed it did not produce a new cap\n");
    }
    ok &= ok1;
    
    if (!(ok1 = !obj_cmp(&file_cap, &new_cap, abuf))) {
	bufprint(abuf, "std_copy() failed it did not produce a new object\n");
    }
    ok &= ok1;
    
    /* tests with an different server */
    if (Alternate_server == NULL) {
	bufprint(abuf, "No alternate bullet server specified; ");
	bufprint(abuf, "skipping std_copy test\n");
    } else {
	if (!cap_server(&alt_svr_cap, Alternate_server, abuf)) {
	    clean(&file_cap, abuf);
	    clean( &new_cap, abuf);
	    test_for_exception(abuf);
	    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
	    return FALSE;
	}
	
	if (!(ok1 = (err = std_copy(&alt_svr_cap, &file_cap,
				    &tmp_cap)) == STD_OK))
	{
	    bufprint(abuf, "std_copy() failed for the second server, ");
	    bufprint(abuf, "err = %s\n", err_why(err));
	}
	if (ok1) {
	    ok &= ok1 & verify_file(write_buf,read_buf, AVG_SIZE, ZERO,
				    &tmp_cap, EQUAL_SIZE, AVG_SIZE, abuf); 
	    if (!(ok1 = !obj_cmp(&file_cap, &tmp_cap, abuf))) {
		bufprint(abuf, "std_copy() did not produce a new object\n");
	    }
	    ok &= ok1;
	    if (!(ok1 = !cap_cmp(&file_cap, &tmp_cap))) {
		bufprint(abuf, "std_copy() did not produce a new cap\n");
	    }
	    ok &= ok1; 
	    if (!(ok1 = !PORTCMP(&file_cap.cap_port, &tmp_cap.cap_port))) {
		bufprint(abuf, "std_copy() failed, new file on same server\n");
	    }
	    ok &= ok1;
	    clean(&tmp_cap, abuf);
	}
    }
    
    clean(&file_cap, abuf);
    clean(&new_cap, abuf);
    test_for_exception(abuf);
    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
    return ok;
}  /* test_std_copy */



