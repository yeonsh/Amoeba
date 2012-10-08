/*	@(#)test.h	1.3	96/03/04 14:03:37 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* An include file to be shared among all the Amoeba test programs: */

#ifndef TEST_INCLUDED		/* Prevent multiple inclusions */
#define TEST_INCLUDED 1

#ifndef FILE
#	include <stdio.h>
#endif

#define TEST_FATAL   0
#define TEST_SERIOUS 1
#define TEST_WARNING 2

#define TEST_DASHES "--------------------------------------------------------------------------------"


#ifdef	MULTI_SRC

extern int test_bool;
extern int test_status;		/* An unlikely number */
extern int test_failures;
extern int test_fail_all;	/* Force failure in all tests. */

#else

int test_bool = 0;
int test_status = -914237;	/* An unlikely number */
int test_failures = 0;
int test_fail_all = 0;		/* Force failure in all tests. */

#endif	/* MULTI_SRC */


/* This should be called at the top of the main function in each test program.
 * prog_name should be the argv[0] of the program calling this macro:
 */
#define TEST_BEGIN(prog_name) { \
		char *cp, *test_name, *test_name_end = NULL; \
		int test_name_len; \
		/* Turn on test_fail_all, if -f specified:*/ \
		if (argc >=2 && strcmp(argv[1], "-F") == 0) { \
			test_fail_all = 1; \
			argv[1] = argv[0];	/* Delete first arg */ \
			argv++, argc--; \
		} \
		test_name = prog_name; \
		/* Strip off possible "./" prefix: */ \
		if (strncmp(test_name, "./", 2) == 0) test_name += 2; \
		/* Strip off prefix up to and including "test" or "test/": */ \
		for (cp = test_name; *cp; ++cp) \
			if (strncmp(cp, "test", 4) == 0) { \
				test_name = cp+4; \
				if (*test_name == '/') test_name++; \
				break; \
			} \
		/* Remove possible suffix from test_name: */ \
		test_name_end = test_name + strlen(test_name); \
		for (cp = test_name_end; cp > test_name; --cp) { \
			if (*cp == '.') { \
				test_name_end = cp; \
				break; \
			} \
			if (*cp == '/') \
				break; \
		} \
		test_name_len = test_name_end - test_name; \
		printf(         " --- Results of Test %0.*s: %0.*s\n", \
			test_name_len, test_name, \
			54 - test_name_len, TEST_DASHES); \
		fprintf(stderr, " --- %0.*s\n", \
			test_name_len, test_name); \
	}

/* This should be called to cause the termination of the test program:
 */
#define TEST_END() { \
		if (test_failures > 0) { \
			fprintf(stderr, " *** %d Failures\n", test_failures); \
			printf(" *** Test Failures: %d\n", test_failures); \
		} else { \
			printf(" --- Test Succeeded\n"); \
		} \
		exit(test_failures); \
	}

/* Tests the specified condition and reports error if not met.  Global variable
 * "test_fail_all" can be turned on to force failure, so that the test programs
 * themselves can be manually tested, without requiring the thing tested to
 * fail.
 */
#define TEST_ASSERT(bool_expr, severity, message) \
	if (!(test_bool = (bool_expr)) || test_fail_all) { \
		test_failures++; \
		printf(" *** %s (%d):\n *** ", __FILE__, __LINE__); \
		printf message; \
		if (severity == TEST_FATAL && !test_bool) \
			TEST_END(); \
	}

/* Following produces a standard error report line for the case where a tested
 * function returns an unexpected status number.  WARNING: This macro requires
 * the test program to declare a variable called status, and to assign the
 * return value of the tested function to it.  Message should be a string
 * that displays what function was called and what its args were.
 */
#define TEST_STATUS(got_status, expected_status, message) \
	TEST_ASSERT((test_status = got_status) == (expected_status), \
		    TEST_SERIOUS, \
		    ("%s%sreturned bad status: %d\n", \
		     message, (strlen(message) <= 53 ? " " : "\n\t\t"), \
		     test_status))

#endif /* TEST_INCLUDED */
