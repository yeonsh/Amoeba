/*	@(#)tb_info.c	1.1	94/04/06 17:38:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_std_info.c
 *
 * Original	: March 9, 1992
 * Author(s)	: Irina 
 * Description	: Tests for std_info
 *
 */

#include "Tginit.h"

PRIVATE void clean(a_file_cap, abuf)
capability * a_file_cap;
ABUFPRINT abuf;
{
    errstat err;

    if ((err = std_destroy(a_file_cap)) != STD_OK) {
	bufprint(abuf, "std_destroy() failed in test_std_info() ");
	bufprint(abuf, "for a non null file, err = %s\n", err_why(err));
    }
}

PUBLIC bool test_std_info(server, abuf)
char *server;
ABUFPRINT abuf;
{
    char sir[10];
    errstat err; 
    bool ok, ok1;
    b_fsize alt_size;
    int size, tmp_size, new_size,i,j;
    capability svr_cap, file_cap, tmp_cap;
    char * write_buf, * read_buf;

    if (verbose) 
	bufprint(abuf,"--- test std_info() ---\n");
    
    write_buf = (char *)malloc((size_t) AVG_SIZE);
    if (!write_buf){
	bufprint(abuf,"not enough memory\n");
	return FALSE;
    }
    read_buf = (char *) malloc((size_t) AVG_SIZE);
    if (!read_buf){
	bufprint(abuf,"not enough memory\n");
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    
    /* initial conditions for test */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) &&
	  init_buffer(write_buf, abuf, AVG_SIZE) &&
	  create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &file_cap, 
		      VERIFY_NEW_DIF_OLD, abuf)))
    {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf); free((_VOIDSTAR) read_buf);
	return FALSE;
    }
    ok = TRUE;
    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
    
    /* tests for server with corrupt capabilities */
    make_cap_check_corrupt(&tmp_cap, &svr_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_info(&tmp_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_info() on corrupt check ");
	bufprint(abuf, "field for server, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info on corrupt check" );
	bufprint(abuf, "field for server\n");
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&tmp_cap, &svr_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_info(&tmp_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_info() on corrupt rights ");
	bufprint(abuf, "field for server, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info on corrupt rights ");
	bufprint(abuf, "field for server\n");
    }
    ok &= ok1;
    
    /* test for server with restricted rights */ 
    ok1 = execute_restrict(&svr_cap, &tmp_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			  "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY, abuf);
    
    size = NEG_ONE;
    if (ok1 && !(ok1 = (err = std_info(&tmp_cap, read_buf,
				       (int) AVG_SIZE, &size)) == STD_DENIED))
    {
	bufprint(abuf, "wrong result for std_info() for server with ");
	bufprint(abuf, "capability restricted, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info() for server with ");
	bufprint(abuf, "capability restricted\n");
    }
    ok &= ok1;
    
    
    
    /* test for server with good parameters */
    if (!(ok1 = (err = std_info(&svr_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_OK))
    {
	bufprint(abuf, "std_info() failed, err = %s\n", err_why(err));
    }
    ok &= ok1;
    if (ok1) {
	if (verbose) {
	    bufprint(abuf, "Buffer size = %d\n", size);
	    bufprint(abuf, "Info for %s is : %s\n", server, read_buf);
	}
	(void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
	tmp_size = size - 4;
	if (!(ok1 = (err = std_info(&svr_cap, read_buf,
				    tmp_size, &new_size)) == STD_OVERFLOW))
	{
	    bufprint(abuf,"std_info() failed, err = %s", err_why(err));
	    bufprint(abuf," size = %d\n", tmp_size);
	}
	ok &= ok1;
	if (ok1) {
	    if (new_size != size) {
		bufprint(abuf, "old size = %d, new size = %d\n",
			 size, new_size);
	    }
	}
	if (verbose) {
	    bufprint(abuf, "%ld characters from info for %s is : %s\n", 
		     tmp_size, server, read_buf);
	}
    }
    
    /* tests for file with corrupt capabilities */
    
    make_cap_check_corrupt(&tmp_cap, &file_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_info(&tmp_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_info() on corrupt check ");
	bufprint(abuf, "field for a file, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info() on corrupt check ");
	bufprint(abuf, "field for file\n");
    }
    ok &= ok1;
    
    make_cap_rights_corrupt(&tmp_cap, &file_cap);
    size = NEG_ONE;
    if (!(ok1 = (err = std_info(&tmp_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_CAPBAD))
    {
	bufprint(abuf, "wrong result for std_info() on corrupt rights ");
	bufprint(abuf, "field for a file, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info on corrupt rights ");
	bufprint(abuf, "field for a file\n");
    }
    ok &= ok1;
    
    /* test for file with restricted rights */
    (void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
    ok1 = execute_restrict(&file_cap, &tmp_cap, ALL_BS_RIGHT ^ BS_RGT_READ,
			  "ALL_BS_RIGHT ^ BS_RGT_READ", GOOD_CAPABILITY, abuf);
    size = NEG_ONE;
    if (ok1 && !(ok1 = (err = std_info(&tmp_cap, read_buf,
				       (int) AVG_SIZE, &size)) == STD_DENIED))
    {
	bufprint(abuf, "wrong result for std_info() for file with ");
	bufprint(abuf, "capability restricted, err = %s\n", err_why(err));
    }
    if (!ok1 && (size != NEG_ONE)) {
	bufprint(abuf, "size modified for std_info() for a file with ");
	bufprint(abuf, "capability restricted\n");
    }
    ok &= ok1;
    
    /* test for file with good parameters */
    if (!(ok1 = (err = std_info(&file_cap, read_buf,
				(int) AVG_SIZE, &size)) == STD_OK))
    {
	bufprint(abuf, "std_info() failed, err = %s\n", err_why(err));
    }
    ok &= ok1;
    if (ok1) {
	if (verbose) {
	    bufprint(abuf, "Buffer size = %d\n", size);
	    bufprint(abuf, "Info for file is : %s\n", read_buf);
	}
	for (i = 0; i < sizeof(sir); i++) {
	    sir[i] = ' ';
	}
	for (i = size-7, j = 8;(i > 0) && (read_buf[i] != '-'); i--) {
	    sir[j--] = read_buf[i];
	}
	sir[9] = '\0';
	j = atoi(sir);
	if (!(ok1 = (err = b_size(&file_cap, &alt_size)) == STD_OK)) {
	    bufprint(abuf, "b_size() failed for a commited file, err = %s\n",
		     err_why(err));
	}
	ok &= ok1;
	(void) memset((_VOIDSTAR) read_buf, 0, (size_t) AVG_SIZE);
	if (!(ok1 = (j == alt_size))) {
	    bufprint(abuf, "size raported by the std_info incorrect\n");
	    bufprint(abuf, "real size = %d raported size = %d\n", tmp_size, j);
	}
	ok &= ok1;
	tmp_size = size - 4;
	if (!(ok1 = (err = std_info(&file_cap, read_buf,
				    tmp_size, &new_size)) == STD_OVERFLOW))
	{
	    bufprint(abuf, "std_info() failed, err = %s, size = %d\n", 
		     err_why(err), tmp_size);
	}
	ok &= ok1;
	if (ok1) {
	    if (new_size != size) {
		bufprint(abuf, "old size = %d, new size = %d\n",
			 size, new_size);
	    }
	}
	if (verbose) {
	    bufprint(abuf, "%ld characters from info for file is : %s\n",
		     tmp_size, read_buf);
	}
    }
    
    clean(&file_cap, abuf);
    test_for_exception(abuf);
    free((_VOIDSTAR) read_buf);free((_VOIDSTAR) write_buf);
    return ok;
    
}  /* test_std_info */
