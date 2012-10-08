/*	@(#)tb_modify.c	1.1	94/04/06 17:39:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_modify.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_modify
 *
 */

#include "Tginit.h"

PRIVATE void clean(a_null_cap, a_commit_cap, abuf)
capability * a_null_cap, * a_commit_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_null_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_modify() for a ");
	bufprint(abuf, "null file, err = %s\n", err_why(err));
    }
    if ((err = std_destroy(a_commit_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_modify() for a ");
	bufprint(abuf, "committed file, err = %s\n", err_why(err));
    }
}

PUBLIC bool test_b_modify(server, abuf)
char *server;
ABUFPRINT abuf;
{
    errstat err;
    b_fsize size;
    bool ok,ok1;
    capability svr_cap, new_cap, null_cap, commit_cap, uncommit_cap, tmp_cap;
    char * write_buf, * read_buf;

    write_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory \n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!read_buf){
	bufprint(abuf,"not enough memory \n");
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    if (verbose)
	bufprint(abuf,"--- test b_modify() --- \n");
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, BIG_SIZE) &&
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
    if (!(ok1 = (err = b_modify(&svr_cap, ZERO, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_COMBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with server ");
	bufprint(abuf, "capability, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for b_modify() on corrupt ");
	bufprint(abuf, "check field, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, AVG_SIZE, 
			       SAFE_COMMIT, &new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for b_modify() on corrupt ");
	bufprint(abuf, "rights field, err = %s\n", err_why(err));
    }
    ok &= ok1; 
    
    /* tests with bad arguments */
    if (!(ok1 = (err = b_modify(&commit_cap, NEG_ONE, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_modify(&commit_cap, AVG_SIZE+1, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset > ");
	bufprint(abuf, "file size, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_modify(&commit_cap, ZERO, read_buf, 
			       NEG_ONE, SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with wrong arguments on an uncommited file */
    if (!(ok1 = (err = b_modify(&uncommit_cap, NEG_ONE, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_modify(&uncommit_cap, AVG_SIZE+1, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset > file ");
	bufprint(abuf, "size, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_modify(&uncommit_cap, ZERO, read_buf, 
			       NEG_ONE, SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_modify() with offset < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* test modify without rights to modify */
    ok &= execute_restrict(&commit_cap, &new_cap, ALL_BS_RIGHT ^BS_RGT_MODIFY,
			   "ALL_BS_RIGHT^BS_RGT_MODIFY", GOOD_CAPABILITY,  abuf);
    if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, AVG_SIZE,
				SAFE_COMMIT, &tmp_cap)) == STD_DENIED))
    {
	bufprint(abuf, "wrong result for b_modify() without rights, ");
	bufprint(abuf, "err = %s\n", err_why(err));
	if (err == STD_OK) {
	    ok &= (err = std_destroy(&tmp_cap)) == STD_OK;
	    ok &= create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT,
			      &uncommit_cap, VERIFY_NEW_DIF_OLD, abuf);
	}
    }
    ok &= ok1;
    
    /* tests with good parametrs */
    
    read_buf[0] = write_buf[(AVG_SIZE + 1) / 2] ^1;
    
    if (!(ok1 = (err = b_modify(&uncommit_cap, (AVG_SIZE + 1)/2, read_buf,
				(b_fsize) 1, NO_COMMIT, &new_cap)) == STD_OK))
    {
	bufprint(abuf, "b_modify() failed for a uncommited file, ");
	bufprint(abuf, "err = %s", err_why(err));
	ok &= ok1 & (std_destroy(&uncommit_cap) == STD_OK);
    }
    else { 
	if (!(ok1 = cap_cmp(&uncommit_cap, &new_cap))) {
	    bufprint(abuf, "capability for a modified uncommited ");
	    bufprint(abuf, "file must be the same as original\n");
	}
	/* commit the file */
	if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, ZERO,
				    SAFE_COMMIT, &tmp_cap)) == STD_OK))
	{
	    bufprint(abuf, "b_modify() failed for an uncommited file ");
	    bufprint(abuf, "when committed \n");
	    ok &= ok1;
	}
	else {
	    if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		bufprint(abuf, "b_modify() failed after commited gave a ");
		bufprint(abuf, "different capability\n");
	    }
	    ok &= ok1;
	    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) BIG_SIZE);
	    if (!(ok1 = (err = b_read(&tmp_cap, ZERO, read_buf,
				      AVG_SIZE, &size)) == STD_OK))
	    {
		bufprint(abuf, "b_read() failed after commiting a file, ");
		bufprint(abuf, "err = %s\n", err_why(err));
		ok &= ok1;
	    } 
	    else {
		if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
				     (_VOIDSTAR) write_buf,
				     (size_t) ((AVG_SIZE + 1)/2 - 1)) &&
			     (read_buf[(AVG_SIZE + 1) / 2] ^ 1 == 
			      write_buf[(AVG_SIZE + 1) / 2]) &&
			     !memcmp((_VOIDSTAR) (read_buf + (AVG_SIZE+1)/2+1),
				     (_VOIDSTAR) (write_buf +(AVG_SIZE+1)/2+1),
				     (size_t) ((AVG_SIZE+ 1) / 2 - 1)))))
		{
		    bufprint(abuf, "wrong data after b_modify with ");
		    bufprint(abuf, "normal file()\n");
		    ok &= ok1;
		}
		if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK)) {
		    bufprint(abuf, "b_size() failed after commiting a file, ");
		    bufprint(abuf, "err =\n", err_why(err));
		    ok &= ok1;
		}
		else if (!(ok1 = (size == AVG_SIZE))) {
		    bufprint(abuf, "the new file has a wrong size, ");
		    bufprint(abuf, "size = %ld and must be %ld\n",
			     size, AVG_SIZE);
		}
	    }
	    ok &= ok1;
	    if (!(ok1 = (err = std_destroy(&tmp_cap)) == STD_OK)) {
		bufprint(abuf, "std_destroy() failed for a comited file, ");
		bufprint(abuf,"err = %s\n", err_why(err));
	    }
	    ok &= ok1;
	}   
    }
    
    
    ok &= ok1;
    
    /* test for the case when the new file is greater */
    read_buf[0] = write_buf[AVG_SIZE - 1] ^1;
    read_buf[1] = 5;
    
    /* the file must be recreate */
    
    ok1 = create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT, &uncommit_cap,
		      VERIFY_NEW_DIF_OLD, abuf);
    if (ok1) {
	if (!(ok1 = (err = b_modify(&uncommit_cap, AVG_SIZE - 1, read_buf,
				 (b_fsize) 2, NO_COMMIT, &new_cap)) == STD_OK))
	{
	    bufprint(abuf, "b_modify() failed for a uncommited file, err = %s",
		     err_why(err));
	    ok &= ok1;
	}
	else { 
	    if (!(ok1 = cap_cmp(&uncommit_cap, &new_cap))) {
		bufprint(abuf, "the capability for a modified uncommited ");
		bufprint(abuf, "file must be the same as original one\n");
	    }
	    ok &= ok1;
	    
	    /* commit the file */
	    if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, ZERO,
					SAFE_COMMIT, &tmp_cap)) == STD_OK))
	    {
		bufprint(abuf, "b_modify() failed for an uncommited file ");
		bufprint(abuf, "when commited \n");
		ok &= ok1 & (std_destroy(&new_cap) == STD_OK);
	    }
	    else {
		if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		    bufprint(abuf, "b_modify failed, commited file must ");
		    bufprint(abuf, "have the same capability as before\n");
		}
		ok &= ok1;
		(void) memset((_VOIDSTAR) read_buf, 0, (size_t) BIG_SIZE);
		if (!(ok1 = (err = b_read(&tmp_cap, ZERO, read_buf,
					  AVG_SIZE+1, &size)) == STD_OK))
		{
		    bufprint(abuf, "b_read() failed after commited a file, ");
		    bufprint(abuf, "err = %s\n", err_why(err));
		    ok &= ok1;
		}
		else {
		    if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
					 (_VOIDSTAR) write_buf,
					 (size_t) (AVG_SIZE - 1)) &&
				 (read_buf[AVG_SIZE - 1] ^ 1 ==
				  write_buf[AVG_SIZE - 1]) &&
				 (read_buf[AVG_SIZE] == 5))))
		    {
			bufprint(abuf, "wrong data after b_modify() ");
			bufprint(abuf, "with big file\n");
		    }
		    ok &= ok1;
		}
		if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK)) {
		    bufprint(abuf, "b_size() failed after commiting a file, ");
		    bufprint(abuf, "err = %s\n", err_why(err));
		    ok &= ok1;
		}
		else if (!(ok1 = (size == AVG_SIZE + 1))) {
		    bufprint(abuf, "the new file has a wrong size, ");
		    bufprint(abuf, "size = %ld and must be %ld\n",
			     size, AVG_SIZE + 1);
		}
		ok &= ok1;
	    }
	}
	ok &= std_destroy(&new_cap) == STD_OK;
    }
    
    ok &= ok1; 
    clean (&null_cap, &commit_cap, abuf);
    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf); free((_VOIDSTAR) read_buf);
    return ok;
    
} /* test_b_modify */

