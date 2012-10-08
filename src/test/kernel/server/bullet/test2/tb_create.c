/*	@(#)tb_create.c	1.1	94/04/06 17:38:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: test_b_create.c
 * 
 * Original	: March 20, 1992
 * Author(s)	: Irina
 * Description	: Tests for b_create
 */

#include "Tginit.h"

PUBLIC bool 
test_b_create(server, abuf)
char       *server;
ABUFPRINT   abuf;
{
    errstat     err;
    bool        ok, ok1;
    capability  svr_cap, new_cap, tmp_cap;
    char       *write_buf;
    union {
	char *ptr; 
	bool (*func_ptr) _ARGS((char *, ABUFPRINT));
    } func_union;
	
    func_union.func_ptr = test_b_create;

    if (verbose)
	bufprint(abuf, "--- test b_create() --- \n");

    write_buf = (char *) malloc((size_t) BIG_SIZE);
    if (!write_buf) {
	bufprint(abuf, "Could not allocate buffer\n");
	return FALSE;
    }

    /* initial conditions for the tests */
    init_exception_flag(abuf);
    if (!(cap_server(&svr_cap, server, abuf) &&
	  init_buffer(write_buf, abuf, BIG_SIZE))) {
	test_for_exception(abuf);
	free((_VOIDSTAR) write_buf);
	return FALSE;
    }
    ok = TRUE;

    /* create with wrong capability for server */
    make_cap_check_corrupt(&new_cap, &svr_cap);
    err = b_create(&new_cap, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (!(ok1 = (err == STD_CAPBAD))) {
	bufprint(abuf, "b_create with corrupt check field returned %s\n",
		 err_why(err));
    }
    ok &= ok1;

    make_cap_rights_corrupt(&new_cap, &svr_cap);
    err = b_create(&new_cap, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);
    if (!(ok1 = (err == STD_CAPBAD))) {
	bufprint(abuf, "b_create with corrupt rights field returned %s\n",
		 err_why(err));
    }
    ok &= ok1;

    /* create with restricted rights */
    ok1 = execute_restrict(&svr_cap, &new_cap, ALL_BS_RIGHT ^ BS_RGT_CREATE,
		     "ALL_BS_RIGHT ^ BS_RGT_CREATE", GOOD_CAPABILITY, abuf);
    if (ok1) {
	err = b_create(&new_cap, write_buf, ZERO, SAFE_COMMIT, &tmp_cap);

	if (!(ok1 = (err == STD_DENIED))) {
	    bufprint(abuf, "b_create without rights returned %s\n",
		     err_why(err));
	}
    }
    ok &= ok1;

    /* test with null file, some files will be created and must be destroyed */
    ok &= execute_restrict(&svr_cap, &new_cap, BS_RGT_CREATE, "BS_CREATE",
			   GOOD_CAPABILITY, abuf) &&
	  create_file(&new_cap, write_buf, ZERO, SAFE_COMMIT, &tmp_cap,
		      VERIFY_NEW_DIF_OLD, abuf) &&
	 (std_destroy(&tmp_cap) == STD_OK);

    err = b_create(&new_cap, (char *) NULL, ZERO, SAFE_COMMIT, &tmp_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_create with NULL ptr and 0 size returned %s\n",
		 err_why(err));
    }
    ok &= ok1 && (std_destroy(&tmp_cap) == STD_OK);

    err = b_create(&new_cap, func_union.ptr, ZERO, SAFE_COMMIT, &tmp_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_create with func pointer and 0 size returned %s\n",
		 err_why(err));
    }
    ok &= ok1 && (std_destroy(&tmp_cap) == STD_OK);

    /* tests with commited and uncomitted file not null */

    ok &= execute_restrict(&svr_cap, &new_cap, BS_RGT_CREATE, "BS_CREATE",
			   GOOD_CAPABILITY, abuf) &&
	create_file(&new_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &tmp_cap,
	   VERIFY_NEW_DIF_OLD, abuf) && (std_destroy(&tmp_cap) == STD_OK) &&
	create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap,
		    VERIFY_NEW_DIF_OLD, abuf) &&
	create_file(&new_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &tmp_cap,
		    ! VERIFY_NEW_DIF_OLD, abuf) &&
	(std_destroy(&new_cap) == STD_OK) &&
	create_file(&svr_cap, write_buf, AVG_SIZE, NO_COMMIT, &new_cap,
		    VERIFY_NEW_DIF_OLD, abuf) &&
	create_file(&new_cap, write_buf, AVG_SIZE, NO_COMMIT, &tmp_cap,
	   VERIFY_NEW_DIF_OLD, abuf) && (std_destroy(&new_cap) == STD_OK) &&
	(std_destroy(&tmp_cap) == STD_OK);

    /* tests with bad pointer and commited not null file */

    abuf->aident = &identl;
    if (!setjmp(*((jmp_buf *) thread_alloc(&identl, sizeof(jmp_buf))))) {
	err = b_create(&svr_cap, (char*)NULL, AVG_SIZE, SAFE_COMMIT, &new_cap);
	if (!(ok1 = (err == RPC_BADADDRESS))) {
	    bufprint(abuf, "b_create file failed, err = %s, size = %ld\n",
		     err_why(err), AVG_SIZE);
	} else if (abuf->flag != EXC_SYS) {
	    bufprint(abuf, "b_create file failed, flag = %d", abuf->flag);
	}
    }

    ok &= ok1;

    abuf->flag = 0;
    err = b_create(&svr_cap, func_union.ptr, AVG_SIZE, SAFE_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_create failed, err = %s, size = %d, flag = %d\n",
		 err_why(err), AVG_SIZE, abuf->flag);
    }
    ok &= ok1;

    test_for_exception(abuf);

    /* tests with bad pointer and uncommited not null file */

    if (!setjmp(*((jmp_buf *) thread_alloc(&identl, sizeof(jmp_buf))))) {
	err = b_create(&svr_cap, (char *) NULL, AVG_SIZE, NO_COMMIT, &new_cap);
	if (!(ok1 = (err == RPC_BADADDRESS))) {
	    bufprint(abuf, "b_create failed, err = %s, size = %ld\n",
		     err_why(err), AVG_SIZE);
	} else if (abuf->flag != EXC_SYS) {
	    bufprint(abuf, "b_create failed, flag = %d\n", abuf->flag);
	}
    }

    ok &= ok1;

    abuf->flag = 0;
    abuf->aident = &identg;
    err = b_create(&svr_cap, func_union.ptr, AVG_SIZE, NO_COMMIT, &new_cap);
    if (!(ok1 = (err == STD_OK))) {
	bufprint(abuf, "b_create failed, err = %s, size = %d, flag = %d\n",
		 err_why(err), AVG_SIZE, abuf->flag);
    }
    ok &= ok1;
    test_for_exception(abuf);

    /* tests with big buffer */

    ok &= create_file(&svr_cap, write_buf, AVG_SIZE + 1, SAFE_COMMIT, &new_cap,
		      VERIFY_NEW_DIF_OLD, abuf) &&
	create_file(&new_cap, write_buf, AVG_SIZE + 1, SAFE_COMMIT, &tmp_cap,
		    ! VERIFY_NEW_DIF_OLD, abuf) &&
	(std_destroy(&new_cap) == STD_OK) &&
	create_file(&svr_cap, write_buf, AVG_SIZE + 1, NO_COMMIT, &new_cap,
		    VERIFY_NEW_DIF_OLD, abuf) &&
	create_file(&new_cap, write_buf, AVG_SIZE + 1, NO_COMMIT, &tmp_cap,
		    VERIFY_NEW_DIF_OLD, abuf) &&
	(std_destroy(&new_cap) == STD_OK) && (std_destroy(&tmp_cap) == STD_OK);

    ok &= create_file(&svr_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &new_cap,
		      VERIFY_NEW_DIF_OLD, abuf);
    write_buf[0] ^= 1;
    ok &= create_file(&new_cap, write_buf, AVG_SIZE, SAFE_COMMIT, &tmp_cap,
		      VERIFY_NEW_DIF_OLD, abuf);
    write_buf[0] ^= 1;
    ok &= (std_destroy(&new_cap) == STD_OK) &&
	(std_destroy(&tmp_cap) == STD_OK);

    test_for_exception(abuf);
    free((_VOIDSTAR) write_buf);
    return ok;

}				/* test_b_create() */
