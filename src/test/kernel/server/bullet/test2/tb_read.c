/*	@(#)tb_read.c	1.1	94/04/06 17:39:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_read.c
 *
 * Original	: March 20, 1992
 * Author(s)	: Irina 
 * Description	: Tests for b_read
 *
 */

#include "Tginit.h"

PUBLIC bool test_b_read(server, abuf)
char *server;
ABUFPRINT abuf;
{
    errstat err;
    bool ok, ok1;
    b_fsize size;
    capability svr_cap, new_cap, null_cap, commit_cap, uncommit_cap;
    char * write_buf, * read_buf;
    union {
	char *ptr; 
	bool (*func_ptr) _ARGS((char *, ABUFPRINT));
    } func_union;
	
    func_union.func_ptr = test_b_read;

    if (verbose)
	bufprint(abuf,"--- test b_read() ---\n");
    
    write_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory\n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!read_buf){
	bufprint(abuf,"not enough memory\n");
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
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
    
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&svr_cap, ZERO, read_buf,
			      (b_fsize) 1, &size)) == STD_COMBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() with server cap, ");
	bufprint(abuf, "size = %ld, err = %s\n", ZERO, err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() with server capability\n");
    }
    ok &= ok1;
    
    /* tests with corrupt capabilities */
    make_cap_check_corrupt(&new_cap, &commit_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&new_cap, ZERO, read_buf,
			      AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() on corrupt ");
	bufprint(abuf, "check field\n err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() on corrupt check field\n");
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&new_cap, &commit_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&new_cap, ZERO, read_buf,
			      AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() on corrupt ");
	bufprint(abuf, "rights field\n,  err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read on corrupt rights field\n");
    }
    ok &= ok1;
    
    /* test with uncommited file */
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&uncommit_cap, ZERO, read_buf,
			      ONE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() on an ");
	bufprint(abuf, "uncommited file\n err = %s", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE))
    {
	bufprint(abuf, "!Size modified for b_read() on an uncommited file\n");
    }
    ok &= ok1;
    
    /* test with wrong parametrs */
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&commit_cap,NEG_ONE,read_buf,
			      AVG_SIZE, &size)) == STD_ARGBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() on an commited ");
	bufprint(abuf, "file with offset < 0, err = %s, flag = %d\n", 
		 err_why(err), abuf->flag);
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() with offset < 0\n");
    }
    ok &= ok1;
    
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&commit_cap, ZERO, read_buf,
			      NEG_ONE, &size)) == STD_ARGBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() with size < 0, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() with size < 0\n");  
    }
    ok &= ok1;
    
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&commit_cap, NEG_ONE,read_buf,
			      NEG_ONE, &size)) == STD_ARGBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() with offset < 0 ");
	bufprint(abuf, "and size < 0, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for ");
	bufprint(abuf, "b_read() with offset < 0 and size < 0\n");
    }
    ok &= ok1;
    
    /* test with bad arguments and uncommited file */
    size = NEG_ONE;
    if (!(ok1 = (err = b_read(&uncommit_cap,NEG_ONE,read_buf,
			      AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "! Wrong result for b_read() with offset < 0 ");
	bufprint(abuf, "on an uncommited file\n err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() with offset < 0 ");
	bufprint(abuf, "on an uncommited file\n");
    }
    ok &= ok1;
    
    /* tests with an empty file 
       verify_file(bufw, bufr, size, offset, cap, equal, file_size, abuf) */
    ok &= verify_file(write_buf, read_buf, ZERO, ZERO, &null_cap, 
		      EQUAL_SIZE, ZERO, abuf);
    test_for_exception(abuf);
    ok &= verify_file((char *) NULL, read_buf, ZERO, ZERO, &null_cap, 
		      EQUAL_SIZE, ZERO, abuf);
    test_for_exception(abuf);
    ok &= verify_file(func_union.ptr, read_buf, ZERO, ZERO, &null_cap, 
		      EQUAL_SIZE, ZERO, abuf);
    test_for_exception(abuf);
    
    /* tests with an commited file */
    ok &= verify_file(write_buf, read_buf, AVG_SIZE, ZERO, &commit_cap, 
		      EQUAL_SIZE, AVG_SIZE, abuf);
    /* test without rights */
    
    ok1 = execute_restrict(&commit_cap, &new_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			 "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY,  abuf);
    size = NEG_ONE;
    if (ok1 && !(ok1 = (err = b_read(&new_cap, ZERO , read_buf,
				     AVG_SIZE, &size)) == STD_DENIED))
    {
	bufprint(abuf, "! Wrong result for b_read() without rights, ");
	bufprint(abuf, "err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() without rights\n");
    }
    ok &= ok1;
    
    /* test with bad pointer */
    size = NEG_ONE;
    abuf -> aident = &identl;
    if (!setjmp(*((jmp_buf *)thread_alloc(&identl, sizeof(jmp_buf))))) {
	if (!(ok1 = ((err = b_read(&commit_cap, ZERO, (char *) NULL,
				   AVG_SIZE, &size)) == RPC_BADADDRESS)))
	{
	    bufprint(abuf, "b_read() failed for a commited file with null ");
	    bufprint(abuf, "pointer,  err = %s\n", err_why(err));
	}
	else if (abuf -> flag != EXC_SYS) {
	    bufprint(abuf, "b_read() failed for a commited file with null ");
	    bufprint(abuf, "pointer, flag = %d\n", abuf -> flag);
	}
    }
    
    abuf -> flag = 0;
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() for a commited ");
	bufprint(abuf, "file with null pointer\n");
    }
    ok &= ok1;
    
    size = NEG_ONE;
    
    if (!setjmp(*((jmp_buf *)thread_alloc(&identl, sizeof(jmp_buf))))) {
	if (!(ok1 = ((err = b_read(&commit_cap, ZERO, func_union.ptr, 
				  AVG_SIZE, &size)) == RPC_BADADDRESS)))
	{
	    bufprint(abuf, "b_read() failed for a commited file with an ");
	    bufprint(abuf, "invalid pointer, err = %sn", err_why(err));
	}
	else if (abuf -> flag != EXC_SYS) {
	    bufprint(abuf, "b_read() failed for a commited file with an ");
	    bufprint(abuf, "invalid pointer, flag = %d\n", abuf -> flag);
	}
    }
    
    abuf -> flag = 0;
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() for a commited ");
	bufprint(abuf, "file with an invalid pointer\n");
    }
    ok &= ok1;
    
    
    /* test with bad pointer for size */
    size = NEG_ONE;
    
    if (!setjmp(*((jmp_buf *)thread_alloc(&identl, sizeof(jmp_buf))))) {
	if (!(ok1 = (( err = b_read(&commit_cap, ZERO, read_buf, AVG_SIZE,
				    (b_fsize *) NULL)) == RPC_BADADDRESS)))
	{
	    bufprint(abuf, "b_read() failed for invalid pointer for size, ");
	    bufprint(abuf, "err = %s\n", err_why(err));
	}
	else if (abuf -> flag != EXC_SYS) {
	    bufprint(abuf, "b_read() failed for invalid pointer for size\n");
	    bufprint(abuf," flag = %d\n", abuf -> flag);
	}
    }
    
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "!Size modified for b_read() for invalid pointer\n");
    }
    ok &= ok1;
    
    abuf ->flag = 0;
    abuf -> aident = &identg;
    
    /* test with offset > 0 and size + offset > file size */
    
    if (!(ok1 = (((err = b_read(&commit_cap, (b_fsize) 2, read_buf,
				AVG_SIZE, &size)) == STD_OK) &&
		 (size == AVG_SIZE - 2))))
    {
	bufprint(abuf, "b_read() failed for offset > 0, size + offset > ");
	bufprint(abuf, "file size, err = %s, offset = %d, ", err_why(err), 2),
	bufprint(abuf, "size = %ld, file size = %ld,length = %ld\n",
		 size, AVG_SIZE, AVG_SIZE);
    }
    ok &= ok1;
    
    /* verify_file() uses b_read() and memcmp */
    ok &=  verify_file(write_buf, read_buf, ZERO, ONE, &commit_cap, 
		       EQUAL_SIZE, AVG_SIZE, abuf) &&
	   verify_file(write_buf, read_buf, ONE, ZERO, &null_cap, 
		       ! EQUAL_SIZE, ZERO, abuf) &&
	   verify_file(write_buf, read_buf, ONE, ZERO, &commit_cap, 
		       EQUAL_SIZE, AVG_SIZE, abuf) &&
	   verify_file(write_buf, read_buf, AVG_SIZE + 1, ZERO, &commit_cap, 
		       ! EQUAL_SIZE, AVG_SIZE, abuf);

    ok &= (std_destroy(&null_cap) == STD_OK) &&
	  (std_destroy(&commit_cap) == STD_OK) &&
	  (std_destroy(&uncommit_cap) == STD_OK);
    test_for_exception(abuf);    
    
    free((_VOIDSTAR) read_buf); free((_VOIDSTAR) write_buf);
    return ok;
    
} /* test_b_read */
