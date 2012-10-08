/*	@(#)Tbullet1.c	1.2	94/04/06 17:36:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <stdio.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "posix/string.h"
#include "test.h"
#include "ampolicy.h"

#define alphabet  "abcdefghigjlmnopqrstuvwxyz"
#define ALPHABET  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define	AlPhAbEt  "ABCDEFGHIJKLMNOPQRSTUVWXYz"
#define DIGITS	  "0123456789"

/* Test the bullet file server. */
main(argc, argv)
	int argc; char **argv;
{
	capability b_cap, new_file;
	char *name;
	errstat status = STD_OK;

	TEST_BEGIN(argv[0]);

	/* Get the name of the bullet server to be tested: */
	if (argc > 1 && argv[1])
		name = argv[1];
	else
		name = DEF_BULLETSVR;

	TEST_ASSERT(name_lookup(name, &b_cap) == STD_OK, TEST_FATAL,
			("Cannot find specified bullet server: %s\n", name));

	/* Test operations on committed files with BS_SAFETY */
	/* ------------------------------------------------- */
	/* Test b_create: -------------------------------------------------- */
	status = b_create(&b_cap, ALPHABET, (b_fsize)26, BS_COMMIT|BS_SAFETY, &new_file);
	TEST_STATUS(status, STD_OK, "b_create(cap1, \"ABC...XYZ\", 26, BS_COMMIT|BS_SAFETY, cap2)");

	if (status == STD_OK)
	{
	    errstat stat2;
	    capability cmpcap;

/*---- Make a file which is the same and compare it ---*/
	    stat2 = b_create(&new_file, ALPHABET, (b_fsize)26, BS_COMMIT|BS_SAFETY, &cmpcap);
	    TEST_STATUS(stat2, STD_OK, "b_create(cap1, \"ABC...XYZ\", 26, BS_COMMIT|BS_SAFETY, cap2) for compare");

	    if (cap_cmp(&cmpcap, &new_file) == 0)
	    {
		(void) std_destroy(&cmpcap);
		TEST_ASSERT(0, TEST_SERIOUS,
		("Compare option of b_create with identical files failed\n"));
	    }

/*---- Compare it to a null file ----*/
	    stat2 = b_create(&new_file, "", (b_fsize)0, BS_COMMIT|BS_SAFETY, &cmpcap);
	    if (cap_cmp(&cmpcap, &new_file) == 1) /* they are same capability */
	    {
		TEST_ASSERT(0, TEST_SERIOUS,
		("Compare option of b_create with null file failed\n"));
	    }
	    else
		(void) std_destroy(&cmpcap);

/*---- Compare it to a non-null different sized file ----*/
	    stat2 = b_create(&new_file, "ABCD", (b_fsize)4, BS_COMMIT|BS_SAFETY, &cmpcap);
	    if (cap_cmp(&cmpcap, &new_file) == 1) /* they are same capability */
	    {
		TEST_ASSERT(0, TEST_SERIOUS,
		("Compare option of b_create with different sized files failed\n"));
	    }
	    else
		(void) std_destroy(&cmpcap);
/*---- Compare it to a non-null same sized file ----*/
	    stat2 = b_create(&new_file, AlPhAbEt, (b_fsize)26, BS_COMMIT|BS_SAFETY, &cmpcap);
	    if (cap_cmp(&cmpcap, &new_file) == 1) /* they are same capability */
	    {
		TEST_ASSERT(0, TEST_SERIOUS,
		("Compare option of b_create with different same sized files failed\n"));
	    }
	    else
		(void) std_destroy(&cmpcap);

	    (void) std_destroy(&new_file);
	}


	/* Test operations on uncommitted files, without BS_SAFETY: */
	/* ----------------------------------------------------------------- */

	/* Test b_create (NULL contents): ---------------------------------- */
	status = b_create(&b_cap, "", (b_fsize)0, 0, &new_file);
	TEST_STATUS(status, STD_OK, "b_create(cap1, \"\", 0, 0, cap2)");

	if (status == STD_OK) {
		/* Test destroying of uncommitted file: -------------------- */
		status = std_destroy(&new_file);
		TEST_STATUS(status, STD_OK, "std_destroy(uncommitted_NULL_file)");
	}

	/* Test b_create: -------------------------------------------------- */
	status = b_create(&b_cap, ALPHABET, (b_fsize)26, 0, &new_file);
	TEST_STATUS(status, STD_OK, "b_create(cap1, \"ABC...XYZ\", 26, 0, cap2)");

	if (status == STD_OK) {
		/* Test destroying of uncommitted file: -------------------- */
		status = std_destroy(&new_file);
		TEST_STATUS(status, STD_OK, "std_destroy(uncommitted_nonNULL_file)");
	}

	/* Test b_create: -------------------------------------------------- */
	status = b_create(&b_cap, "", (b_fsize)0, 0, &new_file);
	TEST_STATUS(status, STD_OK, "b_create(cap1, \"\", 0, 0, cap2)");

	if (status == STD_OK) {
		/* Test b_insert: ------------------------------------------ */
		status = b_insert(&new_file, (b_fsize)0, ALPHABET, (b_fsize)26, 0, &new_file);
		TEST_STATUS(status, STD_OK, "b_insert(cap, 0, \"ABC...XYZ\", 26, 0, cap)");
		/* New_file now should contain the string:
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		 */

		/* Test b_modify: ------------------------------------------ */
		status = b_modify(&new_file, (b_fsize)3, DIGITS, (b_fsize)10, 0, &new_file);
		TEST_STATUS(status, STD_OK, "b_modify(cap, 3, \"0...9\", 10, 0, cap)");
		/* New_file now should contain the string:
			"ABC0123456789NOPQRSTUVWXYZ"
		 */

		status = b_modify(&new_file, (b_fsize)24, alphabet, (b_fsize)26, 0, &new_file);
		TEST_STATUS(status, STD_OK, "b_modify(cap, 24, \"abc...xyz\", 26, 0, cap2)");
		/* New_file now should contain the string:
		    "ABC0123456789NOPQRSTUVWXabcdefghijklmnopqrstuvwxyz"
		 */

		/* Test whether file can be commited ----------------------- */
		status = b_modify(&new_file, (b_fsize)0, "", (b_fsize)0, BS_COMMIT, &new_file);
		TEST_STATUS(status, STD_OK, "b_modify(cap, 0, \"\", 0, BS_COMMIT, cap2)");

		/* Test b_read: -------------------------------------------- */

		/* Test std_destroy(committed_file): ----------------------- */
		status = std_destroy(&new_file);
		TEST_STATUS(status, STD_OK, "std_destroy(committed_file)");
	}
	TEST_END();
	/*NOTREACHED*/
}
