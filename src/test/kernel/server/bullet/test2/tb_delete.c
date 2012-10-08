/*	@(#)tb_delete.c	1.1	94/04/06 17:38:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_delete
 *
 * Original	: March 9, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_delete
 *
 */

#include "Tginit.h"

#ifdef notused
PRIVATE void clean(a_null_cap, a_commit_cap, a_uncommit_cap, abuf)
capability * a_null_cap, * a_commit_cap, * a_uncommit_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_null_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_delete() ");
	bufprint(abuf, "for a null file, err = %s\n", err_why(err));
    }
    if ((err = std_destroy(a_commit_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_delete() ");
	bufprint(abuf, "for a commited file, err = %s\n", err_why(err));
    }
    if ((err = std_destroy(a_uncommit_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_delete() ");
	bufprint(abuf, "for a uncommited file\n err = %s\n", err_why(err));
    }
}
#endif

PUBLIC bool test_b_delete(server, abuf)
char *server;
ABUFPRINT abuf;
{
    errstat err;
    b_fsize size;
    bool ok,ok1;
    capability svr_cap, new_cap, null_cap, commit_cap, uncommit_cap, tmp_cap;
    char * write_buf, * read_buf;

    write_buf = (char *) malloc((size_t) AVG_SIZE);
    if(!write_buf){
	bufprint(abuf,"not enough memory \n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) AVG_SIZE);
    if(!read_buf){
	bufprint(abuf,"not enough memory \n");
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    if (verbose) 
	bufprint(abuf, "--- test b_delete() --- \n");
    
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
	test_for_exception(abuf);
	free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    ok = TRUE;
    
    /* test with capability for server */
    err = b_delete(&svr_cap, ZERO, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_COMBAD))) {
	bufprint(abuf, "b_delete() failed on server cap with err %s\n",
		 err_why(err)); 
    }
    ok &= ok1;
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &commit_cap);
    err = b_delete(&new_cap, ZERO, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_CAPBAD))) {
	bufprint(abuf, "b_delete failed on corrupt check field, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &commit_cap);
    err = b_delete(&new_cap, ZERO,  AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_CAPBAD))) {
	bufprint(abuf, "b_delete failed on corrupt rights field, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;  
    
    /* tests with wrong arguments on a commited file*/
    err = b_delete(&commit_cap, NEG_ONE,  AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed on a commited file, off<0, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    err = b_delete(&commit_cap, AVG_SIZE+1, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed on a commited file, ");
	bufprint(abuf, "offset > file size, err = %s\n", err_why(err));
    } 
    ok &= ok1; 
    
    err = b_delete(&commit_cap, ZERO, NEG_ONE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed with offset < 0, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    /* tests with wrong arguments on an uncommited file */
    err = b_delete(&uncommit_cap, NEG_ONE,  AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed with offset < 0, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;

    err = b_delete(&uncommit_cap, AVG_SIZE+1, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed with offset > file size, err = %s\n",
		 err_why(err)); 
    }
    ok &= ok1; 

    err = b_delete(&uncommit_cap, ZERO, NEG_ONE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_ARGBAD))) {
	bufprint(abuf, "b_delete failed with offset < 0, err = %s\n",
		 err_why(err));
    }
    ok &= ok1;
    
    /* test delete without rights to delete */
    ok &= execute_restrict(&commit_cap, &new_cap, ALL_BS_RIGHT ^BS_RGT_MODIFY,
			  "ALL_BS_RIGHT^BS_RGT_MODIFY", GOOD_CAPABILITY, abuf);
    err = b_delete(&new_cap, ZERO,  AVG_SIZE, SAFE_COMMIT, &tmp_cap);
    if (!(ok1 = (err == STD_DENIED))) {
	bufprint(abuf, "b_delete failed without rights, err = %s\n",
		 err_why(err));
	if (err == STD_OK) {
	    ok &= (err = std_destroy(&tmp_cap)) == STD_OK;
	    ok &= create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT,
			      &uncommit_cap, VERIFY_NEW_DIF_OLD, abuf);
	}
    }
    
    
    ok &= ok1;
    
    /* tests with good parametrs for an uncommited file */
    
    err = b_delete(&uncommit_cap, (b_fsize) ((AVG_SIZE + 1)/2), (b_fsize) 1,
		   NO_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_delete failed for an uncommited file, err = %s\n",
		 err_why(err));
	ok &= ok1 && (std_destroy(&uncommit_cap) == STD_OK);
    }
    else {  /* test b_delete on an uncommited file */
	if (!(ok1 = cap_cmp(&uncommit_cap, &new_cap))) {
	    bufprint(abuf, "capability for a modified uncommited file "),
	    bufprint(abuf, "must be the same as original\n");
	}
	/* commit the file */
	err = b_modify(&new_cap, ZERO, read_buf, ZERO, SAFE_COMMIT, &tmp_cap);
	if (!(ok1 = (err == STD_OK))) {
	    bufprint(abuf, "b_modify failed for an uncommited file ");
	    bufprint(abuf, "when commited, err = %s\n", err_why(err));
	    ok &= ok1;
	}
	else {  /* test the commit was succesfull */
	    if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		bufprint(abuf, "b_modify failed after commit gave a ");
		bufprint(abuf, "different capability\n");
	    }
	    ok &= ok1;
	    
	    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
	    err = b_read(&tmp_cap, ZERO, read_buf, AVG_SIZE, &size);
	    if (!(ok1 = (err == STD_OK))) {
		bufprint(abuf, "b_read failed after commit, err = %s\n",
			 err_why(err));
		ok &= ok1; 
	    } else { /* b_read was succesfull */
		if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
				     (_VOIDSTAR) write_buf,
				     (size_t) ((AVG_SIZE+1)/2 - 1)) &&
			     !memcmp((_VOIDSTAR) (read_buf + (AVG_SIZE + 1)/2),
				     (_VOIDSTAR) (write_buf+(AVG_SIZE+1)/2 +1),
				     (size_t) ((AVG_SIZE+ 1) / 2 - 1)))))
		{
		    bufprint(abuf, "wrong data after b_delete on file\n");
		    ok &= ok1;
		}
	    }
	    if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK)){
		bufprint(abuf, "b_size failed after commit, err = %s\n",
			 err_why(err));  
		ok &= ok1;
	    }
	    else if (!(ok1 = (size == (AVG_SIZE  - 1)))) {
		bufprint(abuf, "the new file has a wrong size\n"),
		bufprint(abuf, "size = %ld and must be %ld\n", size, AVG_SIZE);
	    }
	    ok &= ok1;
	    if (!(ok1 = (err = std_destroy(&tmp_cap)) == STD_OK)) {
		bufprint(abuf, "std_destroy() failed for a comitted file, ");
		bufprint(abuf, "err = %s\n", err_why(err));
	    }
	    ok &= ok1; 
	}
    }
    
    /* test with good parametrs for an commited file */
    
    err = b_delete(&commit_cap, (b_fsize) ((AVG_SIZE + 1)/2), (b_fsize) 1,
		   NO_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_delete failed for a commited file, err = %s\n",
		 err_why(err));
	ok &= ok1 && (std_destroy(&commit_cap) == STD_OK);
    }     
    else {  /* test b_delete on a commited file */
	if (!(ok1 = !cap_cmp(&commit_cap, &new_cap))) {
	    bufprint(abuf, "capability for a modified commited file ");
	    bufprint(abuf, "must be different from original\n");
	}

	/* commit the file */
	err = b_modify(&new_cap, ZERO, read_buf, ZERO, SAFE_COMMIT, &tmp_cap);
	if (!(ok1 = (err == STD_OK))) {
	    bufprint(abuf, "b_modify failed for an uncommited file ");
	    bufprint(abuf, "at commit, err = %s\n", err_why(err));
	    ok &= ok1;
	}
	else {  /* test the commit was succesfull */
	    if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		bufprint(abuf, "b_modify gave other capability at commit\n");
		ok &= ok1;
	    }
	    
	    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
	    err = b_read(&tmp_cap, ZERO, read_buf, AVG_SIZE, &size);
	    if (!(ok1 = (err == STD_OK))) {
		bufprint(abuf, "b_read failed after commit, err = %s\n",
			 err_why(err));
		ok &= ok1; 
	    } 
	    else { /* b_read was succesfull */
		if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
				     (_VOIDSTAR) write_buf,
				     (size_t) ((AVG_SIZE+1)/2 - 1)) &&
			     !memcmp((_VOIDSTAR)(read_buf + (AVG_SIZE + 1)/2),
				     (_VOIDSTAR)(write_buf+(AVG_SIZE+1)/2 + 1),
				     (size_t) ((AVG_SIZE+ 1) / 2 - 1)))))
		{
		    bufprint(abuf, "wrong data after b_delete of file()\n");
		    ok &= ok1;
		}
	    }
	    if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK)) {
		bufprint(abuf, "b_size() failed after commit, err = %s\n",
			 err_why(err));  
		ok &= ok1;
	    }
	    else if (!(ok1 = (size == AVG_SIZE  - 1))) {
		bufprint(abuf, "new file has wrong size %ld instead of %ld\n",
			 size, AVG_SIZE);
		ok &= ok1;
	    }
	    if (!(ok1 = (err = std_destroy(&tmp_cap)) == STD_OK)) {
		bufprint(abuf, "std_destroy() failed for a comitted file, ");
		bufprint(abuf, "err = %s\n", err_why(err));
	    }
	    ok &= ok1; 
	}
    }
    
    ok &= ok1;
    test_for_exception(abuf);
    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
    return ok;
} /* test_b_delete() */
