/*	@(#)tb_insert.c	1.1	94/04/06 17:39:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_insert.c
 *
 * Original	: March 9, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_insert
 *
 */

#include "Tginit.h"

PRIVATE void clean(a_null_cap, a_commit_cap, abuf)
capability * a_null_cap, * a_commit_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_null_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_insert() for a null ");
	bufprint(abuf, "file, err = %s\n", err_why(err));
    }
    if ((err = std_destroy(a_commit_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_b_insert() for a ");
	bufprint(abuf, "commited file, err = %s\n", err_why(err));
    }
}

PUBLIC bool test_b_insert(server, abuf)
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
	bufprint(abuf,"--- test b_insert() --- \n");
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) && 
	  init_buffer(write_buf, abuf, BIG_SIZE) &&
	  create_file(&svr_cap, write_buf, ZERO, SAFE_COMMIT, &null_cap, 
		      VERIFY_NEW_DIF_OLD, abuf) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &commit_cap,
		      VERIFY_NEW_DIF_OLD, abuf) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT, &uncommit_cap,
		      VERIFY_NEW_DIF_OLD, abuf))){
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf);
	free((_VOIDSTAR) read_buf);
	return FALSE;
    }
    
    ok = TRUE;
    
    /* test with capability for server */
    if (!(ok1 = (err = b_insert(&svr_cap, ZERO, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_COMBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with server ");
	bufprint(abuf, "capability, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = (err = b_insert(&new_cap, ZERO, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for b_insert() on corrupt ");
	bufprint(abuf, "check field, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &commit_cap);
    if (!(ok1 = (err = b_insert(&new_cap, ZERO, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for b_insert() on corrupt ");
	bufprint(abuf, "rights field, err = %s\n",err_why(err));
    }
    ok &= ok1;  
    
    /* tests with bad arguments */
    if (!(ok1 = (err = b_insert(&commit_cap, NEG_ONE, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with offset < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_insert(&commit_cap, AVG_SIZE+1, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with offset > ");
	bufprint(abuf, "file size, err = %s\n", err_why(err));
    }
    ok &= ok1; 
    
    if (!(ok1 = (err = b_insert(&commit_cap, ZERO, read_buf, 
			       NEG_ONE, SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with size < 0 ");
	bufprint(abuf, " err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* tests with wrong arguments on an uncommited file */
    
    if (!(ok1 = (err = b_insert(&uncommit_cap, NEG_ONE, read_buf, AVG_SIZE, 
				SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with offset < 0 ");
	bufprint(abuf, " err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_insert(&uncommit_cap, AVG_SIZE+1, read_buf, AVG_SIZE, 
			       SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with offset > ");
	bufprint(abuf, "file size, err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    if (!(ok1 = (err = b_insert(&uncommit_cap, ZERO, read_buf, 
			       NEG_ONE, SAFE_COMMIT, &new_cap)) == STD_ARGBAD))
    {
	bufprint(abuf, "wrong result for b_insert() with size < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    ok &= ok1;
    
    /* test insert without rights to insert */
    ok &= execute_restrict(&commit_cap, &new_cap, ALL_BS_RIGHT ^BS_RGT_MODIFY,
			 "ALL_BS_RIGHT^BS_RGT_MODIFY", GOOD_CAPABILITY,  abuf);
    if (!(ok1 = (err = b_insert(&new_cap, ZERO, read_buf, AVG_SIZE,
				SAFE_COMMIT, &tmp_cap)) == STD_DENIED))
    {
	bufprint(abuf, "wrong result for b_insert() without rights, ");
	bufprint(abuf, "err = %s\n", err_why(err));
	if (err == STD_OK) {
	    ok &= (err = std_destroy(&tmp_cap)) == STD_OK;
	    ok &= create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT,
			      &uncommit_cap, VERIFY_NEW_DIF_OLD, abuf);
	}
    }
    ok &= ok1;
    
    /* tests with good parametrs for an uncommited file */
    
    read_buf[0] = write_buf[(AVG_SIZE + 1) / 2] ^1;
    
    if (!(ok1 = (err = b_insert(&uncommit_cap, (AVG_SIZE + 1)/2, read_buf,
				(b_fsize) 1, NO_COMMIT, &new_cap)) == STD_OK))
    {
	bufprint(abuf, "b_insert() failed for an uncommited file, ");
	bufprint(abuf, "err = %s\n", err_why(err));
	ok &= ok1 && (std_destroy(&uncommit_cap) == STD_OK);
    }
    else {  /* test b_insert on an uncommited file */
	if (!(ok1 = cap_cmp(&uncommit_cap, &new_cap))) {
	    bufprint(abuf, "capability for a modified uncommited file ");
	    bufprint(abuf, "must be the same as original\n");
	}
	/* commit the file */
	if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, ZERO,
				    SAFE_COMMIT, &tmp_cap)) == STD_OK))
	{
	    bufprint(abuf, "b_modify() failed for an uncommited file ");
	    bufprint(abuf, "when commited,  err = %s\n", err_why(err));
	    ok &= ok1;
	}
	else {  /* test the commit was succesfull */
	    if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		bufprint(abuf, "b_modify() failed after commited give a ");
		bufprint(abuf, "different capability\n");
	    }
	    ok &= ok1;
	    
	    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) BIG_SIZE);
	    if (!(ok1 = (err = b_read(&tmp_cap, ZERO, read_buf, AVG_SIZE,
				      &size)) == STD_OK))
	    { 
		bufprint(abuf, "b_read() failed after commiting a file, ");
		bufprint(abuf, "err = %s\n", err_why(err));
		ok &= ok1;
	    } 
	    else { /* b_read was succesfull */
		if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
				     (_VOIDSTAR) write_buf,
				     (size_t) ((AVG_SIZE + 1)/2 -1 )) &&
			     (read_buf[(AVG_SIZE + 1) / 2] ^ 1 == 
			      write_buf[(AVG_SIZE + 1) / 2]) &&
			     !memcmp((_VOIDSTAR) (read_buf + (AVG_SIZE+1)/2+1),
				     (_VOIDSTAR) (write_buf + (AVG_SIZE+1)/2),
				     (size_t) ((AVG_SIZE+ 1) / 2 - 1)))))
		{
		    bufprint(abuf, "wrong data after b_insert with ");
		    bufprint(abuf, "normal file()\n");
		    ok &= ok1;
		}
	    }
	    if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK))
	    {
		bufprint(abuf, "b_size() failed after commiting a file, ");
		bufprint(abuf," err = %s\n", err_why(err)); 
		ok &= ok1;
	    }
	    else if (!(ok1 = (size == AVG_SIZE + 1))) {
		bufprint(abuf, "the new file has a wrong size\n");
		bufprint(abuf, "size = %i and must be %i\n",(size, AVG_SIZE));
	    }
	    ok &= ok1;
	    if (!(ok1 = (err = std_destroy(&tmp_cap)) == STD_OK)) {
		bufprint(abuf, "std_destroy() failed for a comited file, ");
		bufprint(abuf," err = %s\n", err_why(err));
	    }
	    ok &= ok1;
	}
    }
    
    /* tests with good parametrs for a commited file */
    
    read_buf[0] = write_buf[(AVG_SIZE + 1) / 2] ^1;
    
    if (!(ok1 = (err = b_insert(&commit_cap, (AVG_SIZE + 1)/2, read_buf,
				(b_fsize) 1, NO_COMMIT, &new_cap)) == STD_OK))
    {
	bufprint(abuf, "b_insert() failed for a commited file, ");
	bufprint(abuf, "err = %s\n", err_why(err));
	ok &= ok1 && (std_destroy(&uncommit_cap) == STD_OK);
    }
    else {  /* test b_insert on an uncommited file */
	if (!(ok1 = !cap_cmp(&uncommit_cap, &new_cap))) {
	    bufprint(abuf, "capability for a modified commited file ");
	    bufprint(abuf, "must be different from the original\n");
	}
	/* commit the file */
	if (!(ok1 = (err = b_modify(&new_cap, ZERO, read_buf, ZERO,
				    SAFE_COMMIT, &tmp_cap)) == STD_OK))
	{
	    bufprint(abuf, "b_modify() failed for an uncommited file ");
	    bufprint(abuf, "when commited, err = %s\n", err_why(err));
	    ok &= ok1;
	}
	else {  /* test the commit was succesfull */
	    if (!(ok1 = cap_cmp(&new_cap, &tmp_cap))) {
		bufprint(abuf, "b_modify() failed after commited gave a ");
		bufprint(abuf, "different capability\n");
	    }
	    ok &= ok1;
	    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) BIG_SIZE);
	    if (!(ok1 = (err = b_read(&tmp_cap, ZERO, read_buf, AVG_SIZE,
				      &size)) == STD_OK))
	    {
		bufprint(abuf, "b_read() failed after commiting a file\n");
		bufprint(abuf," err =\n", err_why(err));
		ok &= ok1;
	    } 
	    else { /* b_read was succesfull */
		if (!(ok1 = (!memcmp((_VOIDSTAR) read_buf,
				     (_VOIDSTAR) write_buf,
				     (size_t) ((AVG_SIZE + 1)/2 - 1)) &&
			     (read_buf[(AVG_SIZE + 1) / 2] ^ 1 == 
			      write_buf[(AVG_SIZE + 1) / 2]) &&
			     !memcmp((_VOIDSTAR) (read_buf + (AVG_SIZE+1)/2+1),
				     (_VOIDSTAR) (write_buf + (AVG_SIZE+1)/2),
				     (size_t) ((AVG_SIZE+ 1) / 2 - 1)))))
		{
		    bufprint(abuf, "wrong data after b_insert with ");
		    bufprint(abuf, "normal file()\n");
		    ok &= ok1;
		}
	    }
	    if (!(ok1 = (err = b_size(&tmp_cap,&size)) == STD_OK)){
		bufprint(abuf, "b_size() failed after commiting a file, ");
		bufprint(abuf,"err = %s\n", err_why(err)); 
		ok &= ok1;
	    }
	    else if (!(ok1 = (size == AVG_SIZE + 1))) {
		bufprint(abuf, "the new file has a wrong size, ");
		bufprint(abuf, "size = %ld and must be %ld\n",size, AVG_SIZE);
	    }
	    ok &= ok1;
	    if (!(ok1 = (err = std_destroy(&tmp_cap)) == STD_OK)) {
		bufprint(abuf, "std_destroy() failed for a comitted file, ");
		bufprint(abuf," err = %s\n", err_why(err));
	    }
	    ok &= ok1;
	}
    }
    
    ok &= ok1;
    clean (&null_cap, &commit_cap, abuf);
    test_for_exception(abuf);
    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
    return ok;
    
} /* test_b_insert */


