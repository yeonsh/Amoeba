/*	@(#)Tbulletc.c	1.3	96/02/27 10:54:40 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tbulletc.c
 *
 * Original	: Apr 5, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Test suite for the Bullet file server (concurrency tests).
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include	"stdlib.h"
#include	"string.h"
#include	"signal.h"
#include	"unistd.h"
#include	"test.h"
#include	"amtools.h"
#include	"amoeba.h"
#include	"ampolicy.h"
#include	"bullet/bullet.h"
#include	"cmdreg.h"
#include	"stderr.h"


/* General stuff. Should go into a separate header file sometime. */
#define	PRIVATE	static
#define PUBLIC
#define EXTERN	extern

#define TRUE	1
#define FALSE	0


typedef	int		bool;
typedef	unsigned char	byte;
typedef	unsigned int	word;


/* Some global flags controlling the output and tests. */
PRIVATE	bool	verbose   = FALSE;	/* Give lots of info on tests. */
PRIVATE	bool	good_only = FALSE;	/* Only test with valid arguments. */
PRIVATE bool	bad_only  = FALSE;	/* Only test with invalid arguments. */
PRIVATE bool	quick     = FALSE;	/* Skip tests taking much time. */

/* The name of the current test_procedure. */
PRIVATE char	testname[32] = "";

/* Other globals for the sunch mechanism. */
PRIVATE int	master;
PRIVATE	bool	isslave	= FALSE;

PRIVATE	unsigned int nslaves = 0;
PRIVATE unsigned int nsigs   = 0;

/* Global stuff to be used by all, if everybody cleans up after himself. */
#define	K		1024L
#define	ZERO		((b_fsize) 0)
#define	DEF_SIZE	((b_fsize) (256L*K))
#define	AUX_SIZE	((b_fsize) (128L*K))
#define BUF_SIZE	(DEF_SIZE + AUX_SIZE)
#define DEF_OFFSET	((b_fsize) (128L*K))
#define	DEF_COMMIT	(BS_COMMIT | BS_SAFETY)
#define MAXBADCAPS	7

#define SIG_CONTINUE	SIGUSR1

int		nbadcaps = 0;		/* Actual number of bad capabilities. */
char		buffer[BUF_SIZE];	/* Buffer for reading data. */
char		def_buffer[DEF_SIZE];	/* Buffer for writing data. */
char		aux_buffer[DEF_SIZE];	/* Idem. */
capability	svr_cap, empty_cap, def_cap, uncommit_cap, bad_caps[MAXBADCAPS];




PRIVATE	bool	test_ok(err, testcode)
/* Checks the error code returned by supporting function calls.
 * Returns FALSE if 'err' != STD_OK and (sub)test has to be aborted.
 */

errstat	err;
char	testcode[];
{
  TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
    ("%s, %s (%s): test aborted\n", testname, testcode, err_why(err)));
  if (err == STD_OK) {
    if (verbose) 
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else 
    return FALSE;

}  /* test_ok() */




PRIVATE	bool	test_good(err, testcode)
/* Checks if a tested function call returned no error.
 * Returns TRUE if 'err' == STD_OK.
 */

errstat	err;
char	testcode[];
{
  TEST_ASSERT(err == STD_OK, TEST_SERIOUS,
		("%s, %s (%s)\n", testname, testcode, err_why(err)));
  if (err == STD_OK) {
    if (verbose)
      printf("passed: %s, %s\n", testname, testcode);

    return TRUE;
  }
  else
    return FALSE;

}  /* test_good() */




PRIVATE	bool	test_bad(err, testcode, argname)
/* Checks if a tested function call returned an error.
 * Returns TRUE if 'err' != STD_OK.
 */

errstat	err;
char	testcode[], argname[];
{
  TEST_ASSERT(err != STD_OK, TEST_SERIOUS,
	("%s, %s (no error for %s)\n", testname, testcode, argname));
  if (err != STD_OK) {
    if (verbose)
      printf("passed: %s, %s (%s)\n", testname, testcode, err_why(err));

    return TRUE;
  }
  else
    return FALSE;

}  /* test_bad() */




PRIVATE	void	test_synch()
/* Synchronizes the concurrent processes before every major test. */

{
  if (isslave) {
    nsigs = 0;

    if (kill(master, SIGINT) == -1)
      TEST_ASSERT(FALSE, TEST_SERIOUS,
		("%s, synch (couldn't notify master)\n", testname));

    (void) sleep(nslaves + 5);
    if (nsigs == 0) {
	/* printf("warning: missed continue signal\n"); */
    }
  }
  else {
    int nsleeps = 0;

    while (nsigs < nslaves && nsleeps++ < nslaves) {
	if (sleep(nslaves + 2) == 0) {
	    /* no more response */
	    break;
	}
    }

    if (nsigs > nslaves) {
	/* A client probably didn't receive our signal soon enough,
	 * and decided to continue.  Never mind.
	 */
	nsigs -= nslaves;
    } else {
	if (nsigs < nslaves) {
	    /* Not all clients reported completion of the current round.
	     * No sence in waiting any longer, since the other clients
	     * might become impatient.
	     */
	    /* printf("master: %d slaves didn't notify\n", nslaves - nsigs); */
	}
	nsigs = 0;
    }

    if (killpg(master, SIG_CONTINUE) == -1)
      TEST_ASSERT(FALSE, TEST_SERIOUS,
                ("%s, synch (couldn't respond to slaves)\n", testname));
  }

}  /* test_synch() */




PRIVATE	void	test_b_read()

{
    errstat	err;
    capability	tmp_cap;
    b_fsize	nread;


    (void)strcpy(testname, "test_b_read()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    test_synch();

    /* Read AUX_SIZE bytes from the beginning of an empty file. */
    err = b_read(&empty_cap, ZERO, buffer, AUX_SIZE, &nread);
    if (test_good(err, "1a"))
      TEST_ASSERT(nread == ZERO, TEST_SERIOUS,
	("%s, 1a: bytes read should be 0 but is %ld\n", testname, (long)nread));

    test_synch();

    /* Read AUX_SIZE bytes from the beginning of a full file. */
    memset(buffer, 0, (size_t)AUX_SIZE);
    err = b_read(&def_cap, ZERO, buffer, AUX_SIZE, &nread);
    if (test_good(err, "1b")) {

      /* Check the size and contents of the file. */
      TEST_ASSERT(nread == AUX_SIZE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)nread));
      TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)nread) == 0, TEST_SERIOUS,
		("%s, 1b: data read do not match original data\n", testname));
    }

    test_synch();

    /* Read AUX_SIZE bytes from the middle of a full file. */
    memset(buffer, 0, (size_t)AUX_SIZE);
    err = b_read(&def_cap, DEF_OFFSET, buffer, AUX_SIZE, &nread);
    if (test_good(err, "1c")) {
      TEST_ASSERT(nread == AUX_SIZE, TEST_SERIOUS,
			("%s, 1c: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)nread));
      TEST_ASSERT(memcmp(buffer, def_buffer + (int)DEF_OFFSET, (size_t)nread) == 0,
    TEST_SERIOUS, ("%s, 1c: data read do not match original data\n", testname));
    }

    test_synch();

    /* Read 0 bytes from the end of a full file. */
    err = b_read(&def_cap, DEF_SIZE, buffer, ZERO, &nread);
    if (test_good(err, "1d"))
      TEST_ASSERT(nread == ZERO, TEST_SERIOUS,
	("%s, 1d: bytes read should be 0 but is %ld\n", testname, (long)nread));

    test_synch();

    /* Read AUX_SIZE bytes from the end of a full file. */
    err = b_read(&def_cap, DEF_SIZE, buffer, AUX_SIZE, &nread);
    if (test_good(err, "1e"))
      TEST_ASSERT(nread == ZERO, TEST_SERIOUS,
	("%s, 1e: bytes read should be 0 but is %ld\n", testname, (long)nread));

    test_synch();

    /* Read "filesize" bytes from the middle of a full file. */
    memset(buffer, 0, (size_t)DEF_SIZE);
    err = b_read(&def_cap, DEF_OFFSET, buffer, DEF_SIZE, &nread);
    if (test_good(err, "1f")) {
      TEST_ASSERT(nread == (DEF_SIZE - DEF_OFFSET), TEST_SERIOUS,
		("%s, 1f: bytes read should be %ld but is %ld\n",
			testname, (long)(DEF_SIZE - DEF_OFFSET), (long)nread));
      TEST_ASSERT(memcmp(buffer, def_buffer + (int)DEF_OFFSET, (size_t)nread) == 0,
    TEST_SERIOUS, ("%s, 1f: data read do not match original data\n", testname));
    }

    test_synch();

    /* Read an entire full file plus one extra byte. */
    memset(buffer, 0, (size_t)DEF_SIZE+1);
    err = b_read(&def_cap, ZERO, buffer, DEF_SIZE + 1, &nread);
    if (test_good(err, "1g")) {
      TEST_ASSERT(nread == DEF_SIZE, TEST_SERIOUS,
			("%s, 1g: bytes read should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)nread));
      TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)nread) == 0, TEST_SERIOUS,
		("%s, 1g: data read do not match original data\n", testname));
    }

    test_synch();

    /* Read one byte in the middle of a file. */
    buffer[0] = ~def_buffer[DEF_OFFSET];
    err = b_read(&def_cap, DEF_OFFSET, buffer, (b_fsize)1, &nread);
    if (test_good(err, "1h")) {
      TEST_ASSERT((int)nread == 1, TEST_SERIOUS,
			("%s, 1h: bytes read should be 1 but is %ld\n",
							testname, (long)nread));
      TEST_ASSERT(memcmp(buffer, def_buffer + (int)DEF_OFFSET, (size_t)nread) == 0,
    TEST_SERIOUS, ("%s, 1h: data read do not match original data\n", testname));
    }

    test_synch();

    /* Read an entire full file with the minimum required rights. */
    err = std_restrict(&def_cap, BS_RGT_READ, &tmp_cap);
    if (test_good(err, "1i, std_restrict()")) {
      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&tmp_cap, ZERO, buffer, DEF_SIZE, &nread);
      if (test_good(err, "1i")) {
        TEST_ASSERT(nread == DEF_SIZE, TEST_SERIOUS,
			("%s, 1i: bytes read should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)nread));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)nread) == 0, TEST_SERIOUS,
		("%s, 1i: data read do not match original data\n", testname));
      }
    }

}  /* test_b_read() */




PRIVATE	void	test_b_create()

{
    errstat	err;
    bool	ok;
    capability	new_cap, tmp_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_b_create()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    test_synch();

    /* Create an uncommitted file from the capability of an uncommitted file. */
    err = b_create(&uncommit_cap, def_buffer, DEF_SIZE, BS_SAFETY, &new_cap);
    if (test_good(err, "1a")) {

      /* The capability must be new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1a: new capability expected\n", testname));

      /* You can't read from an uncommitted file. */
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      (void)test_bad(err, "1a, b_read()", "uncommitted file");

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1a, std_destroy()");
      }
    }

    test_synch();

    /* Create (open) an existing committed file. */
    err = b_create(&def_cap, def_buffer, DEF_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1b")) {

      /* The new capability should be a copy of the original one. */
      TEST_ASSERT(ok = cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1b: original capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1b, b_size()"))
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1b: size should be %ld but is %ld\n",
				testname, (long)DEF_SIZE, (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1b, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read do not match original data\n", testname));
      }

      if (!ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1b, std_destroy()");
      }
    }

    test_synch();

    /* Create an uncommitted file from the capability of a committed file. */
    err = b_create(&def_cap, def_buffer, DEF_SIZE, BS_SAFETY, &new_cap);
    if (test_good(err, "1c")) {

      /* Make sure the new capability is not a copy of the original one. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1c: new capability expected\n", testname));

      /* You can't get the size from an uncommitted file. */
      err = b_size(&new_cap, &size);
      (void)test_bad(err, "1c, b_size()", "uncommitted file");

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1c, std_destroy()");
      }
    }

    test_synch(); 

    /* Create a file starting with the capability of an uncommitted file. */
    err = b_create(&uncommit_cap, def_buffer, DEF_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1d")) {

      /* The capability must be new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1d: new capability expected\n", testname));

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1d, std_destroy()");
      }
    }

    test_synch();

    /* Create a new file that is only slightly different (i.e. one bit). */
    def_buffer[DEF_SIZE-1] ^= (char)0x01;
    err = b_create(&def_cap, def_buffer, DEF_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1e")) {
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1e: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1e, b_size()"))
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1e: size should be %ld but is %ld\n",
				testname, (long)DEF_SIZE, (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1e, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1e: data read do not match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1e, std_destroy()");
      }
    }
    def_buffer[DEF_SIZE-1] ^= (char)0x01;

    test_synch();

    /* Create an empty file. */
    err = b_create(&def_cap, buffer, ZERO, DEF_COMMIT, &new_cap);
    if (test_good(err, "1f")) {
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1f: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1f, b_size()")) 
        TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	     ("%s, 1f: size should be 0 but is %ld\n", testname, (long)size));

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1f, std_destroy()");
      }
    }

    test_synch();

    /* Create a file of one byte with only the minimum required rights. */
    err = std_restrict(&def_cap, BS_RGT_CREATE, &tmp_cap);
    if (test_good(err, "1g, std_restrict()")) {
      err = b_create(&tmp_cap, aux_buffer+10, (b_fsize)1, DEF_COMMIT, &new_cap);
      if (test_good(err, "1g")) {
        /* Make sure the capability is completely new. */
        TEST_ASSERT(ok = !cap_cmp(&new_cap, &tmp_cap), TEST_SERIOUS,
			("%s, 1g: new capability expected\n", testname));

        /* Check the recorded size of the file. */
        err = b_size(&new_cap, &size);
        if (test_good(err, "1g, b_size()"))
          TEST_ASSERT((int)size == 1, TEST_SERIOUS,
			("%s, 1g: size should be 1 but is %ld\n",
							testname, (long)size));

        /* Check the recorded contents of the file. */
        buffer[0] = 0;
        err = b_read(&new_cap, ZERO, buffer, AUX_SIZE, &size);
        if (test_good(err, "1g, b_read()")) {
          TEST_ASSERT((int)size == 1, TEST_SERIOUS,
			("%s, 1g: bytes read should be 1 but is %ld\n",
							testname, (long)size));
          TEST_ASSERT(memcmp(buffer, aux_buffer+10, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1g: data read do not match original data\n", testname));
        }

        if (ok) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "1g, std_destroy()");
        }
      }
    }

}  /* test_b_create() */




PRIVATE	void	test_b_modify()

{
    errstat	err;
    bool	ok;
    capability	new_cap, tmp_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_b_modify()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    test_synch();

    /* Modify one byte in a committed file. */
    def_buffer[DEF_SIZE-1] ^= (char)0x01;
    err = b_modify(&def_cap, DEF_SIZE-1, def_buffer+DEF_SIZE-1, (b_fsize)1,
							DEF_COMMIT, &new_cap);
    if (test_good(err, "1b")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1b: new capability expected\n", testname));

      /* Check the size of the file. */
      err = b_size(&new_cap, &size);
      if (test_good(err, "1b, b_size()"))
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1b: size should be %ld but is %ld\n",
				testname, (long)DEF_SIZE, (long)size));

      /* Check the modified contents of the file. */
      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1b, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)DEF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
      }

      /* Clean up. */
      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1b, std_destroy()");
      }
    }
    def_buffer[DEF_SIZE-1] ^= (char)0x01;

    test_synch();

    /* Modify an empty file. */
    err = b_modify(&empty_cap, ZERO, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (test_good(err, "1d")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1d: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1d, b_size()"))
        TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: size should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));

      memset(buffer, 0, (size_t)AUX_SIZE);
      err = b_read(&new_cap, ZERO, buffer, AUX_SIZE, &size);
      if (test_good(err, "1d, b_read()")) {
        TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, aux_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1d: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1d, std_destroy()");
      }
    }

    test_synch();

    /* Modify at the end of a committed file. */
    err = b_modify(&def_cap, DEF_SIZE, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (test_good(err, "1e")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1e: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1e, b_size()"))
        TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
				("%s, 1e: size should be %ld but is %ld\n",
					testname, (long)BUF_SIZE, (long)size));

      memset(buffer, 0, (size_t)BUF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, BUF_SIZE, &size);
      if (test_good(err, "1e, b_read()")) {
        TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
					testname, (long)BUF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)DEF_SIZE) == 0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
        TEST_ASSERT(memcmp(buffer+DEF_SIZE, aux_buffer, (size_t)(size-DEF_SIZE))==0,
    TEST_SERIOUS, ("%s, 1e: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1e, std_destroy()");
      }
    }

    test_synch();

    /* Modify a file with only the minimum required rights. */
    err = std_restrict(&def_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
    if (test_good(err, "1f, std_restrict()")) {
      err = b_modify(&tmp_cap, DEF_OFFSET, aux_buffer, DEF_SIZE, DEF_COMMIT,
								&new_cap);
      if (test_good(err, "1f")) {

        /* The capability should be completely new. */
        TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1f: new capability expected\n", testname));

        /* Check the size of the modified file. */
        err = b_size(&new_cap, &size);
        if (test_good(err, "1f, b_size()"))
          TEST_ASSERT(size == DEF_OFFSET+DEF_SIZE, TEST_SERIOUS,
		("%s, 1f: size should be %ld but is %ld\n",
			testname, (long)(DEF_OFFSET+DEF_SIZE), (long)size));

        /* Check the unmodified contents of the file. */
        memset(buffer, 0, (size_t)(DEF_OFFSET+DEF_SIZE));
        err = b_read(&new_cap, ZERO, buffer, DEF_OFFSET+DEF_SIZE, &size);
        if (test_good(err, "1f, b_read()")) {
          TEST_ASSERT(size == DEF_OFFSET+DEF_SIZE, TEST_SERIOUS,
		("%s, 1f: bytes read should be %ld but is %ld\n",
			testname, (long)(DEF_OFFSET+DEF_SIZE), (long)size));
          TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)DEF_OFFSET) == 0,
    TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
          TEST_ASSERT(memcmp(buffer+DEF_OFFSET, aux_buffer,
				(size_t)(size-DEF_OFFSET)) == 0, TEST_SERIOUS,
		("%s, 1f: data read don't match original data\n", testname));
        }

        /* Clean up. */
        if (ok) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "1f, std_destroy()");
        }
      }
    }

    test_synch();

    /* Modify an entire committed file. */
    err = b_modify(&def_cap, ZERO, aux_buffer, DEF_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1g")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1g: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1g, b_size()"))
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1g: size should be %ld but is %ld\n",
			testname, (long)DEF_SIZE, (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1g, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1g: bytes read should be %ld but is %ld\n",
				testname, (long)DEF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, aux_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1g: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1g, std_destroy()");
      }
    }

}  /* test_b_modify() */




PRIVATE	void	test_b_delete()

{
    errstat	err;
    bool	ok;
    capability	new_cap, tmp_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_b_delete()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    test_synch();

    /* Delete an entire committed file. */
    err = b_delete(&def_cap, ZERO, DEF_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1a")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1a: new capability expected\n", testname));

      /* Check the size of the file. */
      err = b_size(&new_cap, &size);
      if (test_good(err, "1a, b_size()"))
        TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	      ("%s, 1a: size should be 0 but is %ld\n", testname, (long)size));

      /* Clean up. */
      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1a, std_destroy()");
      }
    }

    test_synch();

    /* Delete one byte in a committed file. */
    err = b_delete(&def_cap, DEF_OFFSET, (b_fsize)1, DEF_COMMIT, &new_cap);
    if (test_good(err, "1b")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1b: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1b, b_size()"))
        TEST_ASSERT(size == DEF_SIZE-1, TEST_SERIOUS,
			("%s, 1b: size should be %ld but is %ld\n",
				testname, (long)(DEF_SIZE-1), (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1b, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE-1, TEST_SERIOUS,
			("%s, 1b: bytes read should be %ld but is %ld\n",
				testname, (long)(DEF_SIZE-1), (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)DEF_OFFSET)==0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
        TEST_ASSERT(memcmp(buffer+DEF_OFFSET, def_buffer+DEF_OFFSET+1,
				(size_t)(size-DEF_OFFSET)) == 0, TEST_SERIOUS,
		("%s, 1b: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1b, std_destroy()");
      }
    }

    test_synch();

    /* Delete in an empty file. */
    err = b_delete(&empty_cap, ZERO, AUX_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1d")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1d: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1d, b_size()"))
        TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	      ("%s, 1d: size should be 0 but is %ld\n", testname, (long)size));

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1d, std_destroy()");
      }
    }

    test_synch();

    /* Delete at the end of a committed file. */
    err = b_delete(&def_cap, DEF_SIZE, AUX_SIZE, DEF_COMMIT, &new_cap);
    if (test_good(err, "1e")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1e: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1e, b_size()"))
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1e: size should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
      if (test_good(err, "1e, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
					testname, (long)DEF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1e: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1e, std_destroy()");
      }
    }

    test_synch();

    /* Delete in a file with only the minimum required rights. */
    err = std_restrict(&def_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
    if (test_good(err, "1f, std_restrict()")) {
      err = b_delete(&tmp_cap, DEF_OFFSET, DEF_SIZE, DEF_COMMIT, &new_cap);
      if (test_good(err, "1f")) {

        /* The capability should be completely new. */
        TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1f: new capability expected\n", testname));

        /* Check the size of the new file. */
        err = b_size(&new_cap, &size);
        if (test_good(err, "1f, b_size()"))
          TEST_ASSERT(size == DEF_OFFSET, TEST_SERIOUS,
			("%s, 1f: size should be %ld but is %ld\n",
				testname, (long)DEF_OFFSET, (long)size));

        /* Check the contents of the file. */
        memset(buffer, 0, (size_t)DEF_SIZE);
        err = b_read(&new_cap, ZERO, buffer, DEF_SIZE, &size);
        if (test_good(err, "1f, b_read()")) {
          TEST_ASSERT(size == DEF_OFFSET, TEST_SERIOUS,
			("%s, 1f: bytes read should be %ld but is %ld\n",
				testname, (long)DEF_OFFSET, (long)size));
          TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1f: data read don't match original data\n", testname));
	}

        /* Clean up. */
        if (ok) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "1f, std_destroy()");
        }
      }
    }

}  /* test_b_delete() */


PRIVATE	void	test_b_insert()

{
    errstat	err;
    bool	ok;
    capability	new_cap, tmp_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_b_insert()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    test_synch();

    /* Insert one byte in a committed file. */
    err = b_insert(&def_cap, ZERO, aux_buffer+10, (b_fsize)1, DEF_COMMIT,
								&new_cap);
    if (test_good(err, "1a")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1a: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1a, b_size()"))
        TEST_ASSERT(size == DEF_SIZE+1, TEST_SERIOUS,
			("%s, 1a: size should be %ld but is %ld\n",
			testname, (long)(DEF_SIZE+1), (long)size));

      memset(buffer, 0, (size_t)(BUF_SIZE));
      err = b_read(&new_cap, ZERO, buffer, BUF_SIZE, &size);
      if (test_good(err, "1a, b_read()")) {
        TEST_ASSERT(size == DEF_SIZE+1, TEST_SERIOUS,
			("%s, 1a: bytes read should be %ld but is %ld\n",
				testname, (long)(DEF_SIZE+1), (long)size));
        TEST_ASSERT(buffer[0] == aux_buffer[10], TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
        TEST_ASSERT(memcmp(buffer+1, def_buffer, (size_t)(size-1))==0, TEST_SERIOUS,
		("%s, 1a: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1a, std_destroy()");
      }
    }

    test_synch();

    /* Insert in an empty file. */
    err = b_insert(&empty_cap, ZERO, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (test_good(err, "1d")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1d: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1d, b_size()"))
        TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: size should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));

      memset(buffer, 0, (size_t)DEF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, AUX_SIZE, &size);
      if (test_good(err, "1d, b_read()")) {
        TEST_ASSERT(size == AUX_SIZE, TEST_SERIOUS,
			("%s, 1d: bytes read should be %ld but is %ld\n",
				testname, (long)AUX_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, aux_buffer, (size_t)size) == 0, TEST_SERIOUS,
		("%s, 1d: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1d, std_destroy()");
      }
    }

    test_synch();

    /* Insert at the end of a committed file. */
    err = b_insert(&def_cap, DEF_SIZE, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (test_good(err, "1e")) {

      /* The capability should be completely new. */
      TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1e: new capability expected\n", testname));

      err = b_size(&new_cap, &size);
      if (test_good(err, "1e, b_size()"))
        TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
			("%s, 1e: size should be %ld but is %ld\n",
			testname, (long)BUF_SIZE, (long)size));

      memset(buffer, 0, (size_t)BUF_SIZE);
      err = b_read(&new_cap, ZERO, buffer, BUF_SIZE, &size);
      if (test_good(err, "1e, b_read()")) {
        TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
			("%s, 1e: bytes read should be %ld but is %ld\n",
				testname, (long)BUF_SIZE, (long)size));
        TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)DEF_SIZE) == 0, TEST_SERIOUS,
		("%s, 1e1: data read don't match original data\n", testname));
        TEST_ASSERT(memcmp(buffer+DEF_SIZE, aux_buffer, (size_t)(size-DEF_SIZE))==0,
    TEST_SERIOUS, ("%s, 1e2: data read don't match original data\n", testname));
      }

      if (ok) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "1e, std_destroy()");
      }
    }

    test_synch();

    /* Insert in a file with only the minimum required rights. */
    err = std_restrict(&def_cap, BS_RGT_MODIFY | BS_RGT_READ, &tmp_cap);
    if (test_good(err, "1f, std_restrict()")) {
      err = b_insert(&tmp_cap, DEF_OFFSET, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
      if (test_good(err, "1f")) {

        /* The capability should be completely new. */
        TEST_ASSERT(ok = !cap_cmp(&new_cap, &def_cap), TEST_SERIOUS,
			("%s, 1f: new capability expected\n", testname));

        /* Check the size of the modified file. */
        err = b_size(&new_cap, &size);
        if (test_good(err, "1f, b_size()"))
          TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
		("%s, 1f: size should be %ld but is %ld\n",
			testname, (long)BUF_SIZE, (long)size));

        /* Check the first part of the file. */
        memset(buffer, 0, (size_t)BUF_SIZE);
        err = b_read(&new_cap, ZERO, buffer, BUF_SIZE, &size);
        if (test_good(err, "1f, b_read()")) {
          TEST_ASSERT(size == BUF_SIZE, TEST_SERIOUS,
			("%s, 1f: bytes read should be %ld but is %ld\n",
				testname, (long)BUF_SIZE, (long)size));
          TEST_ASSERT(memcmp(buffer, def_buffer, (size_t)DEF_OFFSET) == 0,
    TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
          TEST_ASSERT(memcmp(buffer+DEF_OFFSET, aux_buffer, (size_t)AUX_SIZE) == 0,
    TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
          TEST_ASSERT(memcmp(buffer+DEF_OFFSET+AUX_SIZE, def_buffer+DEF_OFFSET,
				(size_t)(size-DEF_OFFSET-AUX_SIZE)) == 0,
    TEST_SERIOUS, ("%s, 1f: data read don't match original data\n", testname));
        }

        /* Clean up. */
        if (ok) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "1f, std_destroy()");
        }
      }
    }

}  /* test_b_insert() */



PRIVATE	void	test_cache()

{
  errstat	err;
  bool		ok;
  capability	uncommit_cap, new_cap;
  b_fsize	size;


  (void)strcpy(testname, "test_cache()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Create an uncommitted file from the server-capability. */
  err = b_create(&svr_cap, def_buffer, DEF_SIZE, 0x00, &uncommit_cap);
  if (test_good(err, "1a, b_create()")) {

    /* Make sure the new capability is not a copy of the original one. */
    TEST_ASSERT(ok = !cap_cmp(&uncommit_cap, &svr_cap), TEST_SERIOUS,
			("%s, 1a: new capability expected\n", testname));

    /* You can't get the size from an uncommitted file. */
    err = b_size(&uncommit_cap, &size);
    (void)test_bad(err, "1a, b_size()", "uncommitted file");

    /* Delete everything in the uncommitted file and commit the result. */
    err = b_delete(&uncommit_cap, ZERO, DEF_SIZE, BS_COMMIT, &new_cap);
    if (test_good(err, "1b, b_delete()")) {

      /* The capability should be an exact copy of the original one. */
      TEST_ASSERT(ok = cap_cmp(&new_cap, &uncommit_cap), TEST_SERIOUS,
			("%s, 1b: original capability expected\n", testname));

      /* Check the size of the modified file (it is committed now). */
      err = b_size(&new_cap, &size);
      if (test_good(err, "1b, b_size()"))
        TEST_ASSERT(size == ZERO, TEST_SERIOUS,
	      ("%s, 1b: size should be 0 but is %ld\n", testname, (long)size));

      /* Clean up if necessary. */
      if (!ok) {
	/* The capability was new: destroy the new file. */
        err = std_destroy(&new_cap);
        (void)test_good(err, "1b, std_destroy()");
	ok = TRUE;
      }
    }

    /* Destroy the original uncommitted file. */
    if (ok) {
      err = std_destroy(&uncommit_cap);
      (void)test_good(err, "1c, std_destroy()");
    }
  }

}  /* test_cache() */


PRIVATE	test_bad_caps()

{
    int		i;
    errstat	err, err1, err2;
    bool	eq;
    capability	new_cap;
    b_fsize	size;
    char	argname[32];

    /* Since signals are used to synchronize the Tbulletc jobs,
     * we may get either RPC_ABORTED or RPC_NOTFOUND when locating
     * a server.  This is only expected for the fifth capability,
     * whose server port we have mangled (see make_bad_caps).
     */
#define MAP_ERROR(nbad, err) \
    if (!quick && (nbad) == 4 && (err) == RPC_ABORTED) { err = RPC_NOTFOUND; }

    (void)strcpy(testname, "test_bad_caps()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with bad capabilities. */
    for (i = 0; i < nbadcaps; i++) {
      sprintf(argname, "bad_caps[%d]", i);

      test_synch();

      err1 = b_size(&bad_caps[i], &size);
      MAP_ERROR(i, err1);
      (void)test_bad(err1, "2a, b_size()", argname);

      test_synch();

      err2 = b_read(&bad_caps[i], ZERO, buffer, AUX_SIZE, &size);
      MAP_ERROR(i, err2);
      (void)test_bad(err2, "2b, b_read()", argname);

      eq = err1 == err2;

      test_synch();

      err1 = b_create(&bad_caps[i], aux_buffer, AUX_SIZE, DEF_COMMIT, &new_cap);
      MAP_ERROR(i, err1);
      if (!test_bad(err1, "2c, b_create()", argname)) {
        if (!cap_cmp(&new_cap, &bad_caps[i])) {
          err = std_destroy(&new_cap);
	  (void)test_good(err, "2c, std_destroy()");
	}
      }

      eq = eq && (err1 == err2);

      test_synch();

      err2 = b_modify(&bad_caps[i], DEF_OFFSET, aux_buffer, AUX_SIZE,
							DEF_COMMIT, &new_cap);
      MAP_ERROR(i, err2);
      if (!test_bad(err2, "2d, b_modify()", argname)) {
        if (!cap_cmp(&new_cap, &bad_caps[i])) {
          err = std_destroy(&new_cap);
	  (void)test_good(err, "2d, std_destroy()");
	}
      }

      eq = eq && (err1 == err2);

      test_synch();

      err1 = b_delete(&bad_caps[i], DEF_OFFSET, AUX_SIZE, DEF_COMMIT, &new_cap);
      MAP_ERROR(i, err1);
      if (!test_bad(err1, "2e, b_delete()", argname)) {
        if (!cap_cmp(&new_cap, &bad_caps[i])) {
          err = std_destroy(&new_cap);
	  (void)test_good(err, "2e, std_destroy()");
	}
      }

      eq = eq && (err1 == err2);

      test_synch();

      err2 = b_insert(&bad_caps[i], DEF_OFFSET, aux_buffer, AUX_SIZE,
							DEF_COMMIT, &new_cap);
      MAP_ERROR(i, err2);
      if (!test_bad(err2, "2f, b_insert()", argname)) {
        if (!cap_cmp(&new_cap, &bad_caps[i])) {
          err = std_destroy(&new_cap);
	  (void)test_good(err, "2f, std_destroy()");
	}
      }

      TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
                ("%s (error codes don't compare)\n", testname));
    }

}  /* test_bad_caps() */




PRIVATE	test_uncomm_file()

{
    errstat	err1, err2;
    b_fsize	size;


    (void)strcpy(testname, "test_uncomm_file()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with an uncommitted file. */
    test_synch();

    err1 = b_size(&uncommit_cap, &size);
    (void)test_bad(err1, "2a, b_size()", "uncommitted file");

    test_synch();

    err2 = b_read(&uncommit_cap, DEF_OFFSET, buffer, AUX_SIZE, &size);
    (void)test_bad(err2, "2b, b_read()", "uncommitted file");

    TEST_ASSERT(err1 == err2, TEST_WARNING,
                ("%s (error codes don't compare)\n", testname));

}  /* test_uncomm_file() */




PRIVATE	test_bad_rights()

{

    errstat	err, err1, err2;
    bool	eq = TRUE;
    capability	tmp_cap, new_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_bad_rights()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with insufficient rights. */
    err = std_restrict(&def_cap, ~BS_RGT_READ, &tmp_cap);
    if (test_good(err, "2a, std_restrict()")) {
      test_synch();

      err1 = b_size(&tmp_cap, &size);
      (void)test_bad(err1, "2a, b_size()", "insufficient rights");

      test_synch();

      err2 = b_read(&tmp_cap, DEF_OFFSET, buffer, AUX_SIZE, &size);
      (void)test_bad(err2, "2b, b_read()", "insufficient rights");

      eq = err1 == err2;

      test_synch();

      err1 = b_modify(&tmp_cap, DEF_OFFSET, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
      if (!test_bad(err1, "2c, b_modify()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2c, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);

      test_synch();

      err2 = b_delete(&tmp_cap, DEF_OFFSET, AUX_SIZE, DEF_COMMIT, &new_cap);
      if (!test_bad(err2, "2d, b_delete()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2d, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);

      test_synch();

      err1 = b_insert(&tmp_cap, DEF_OFFSET, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
      if (!test_bad(err1, "2e, b_insert()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2e, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);
    }

    err = std_restrict(&def_cap, ~BS_RGT_CREATE, &tmp_cap);
    if (test_good(err, "2f, std_restrict()")) {
      test_synch();

      err2 = b_create(&tmp_cap, aux_buffer, AUX_SIZE, DEF_COMMIT, &new_cap);
      if (!test_bad(err2, "2f, b_create()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2f, std_destroy()");
	}
      }

      eq = eq && (err1 == err2);
    }

    err = std_restrict(&def_cap, ~BS_RGT_MODIFY, &tmp_cap);
    if (test_good(err, "2g, std_restrict()")) {
      test_synch();

      err1 = b_modify(&tmp_cap, DEF_OFFSET, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
      if (!test_bad(err1, "2g, b_modify()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2g, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);

      test_synch();

      err2 = b_delete(&tmp_cap, DEF_OFFSET, AUX_SIZE, DEF_COMMIT, &new_cap);
      if (!test_bad(err2, "2h, b_delete()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2h, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);

      test_synch();

      err1 = b_insert(&tmp_cap, DEF_OFFSET, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
      if (!test_bad(err1, "2i, b_insert()", "insufficient rights")) {
        if (!cap_cmp(&new_cap, &tmp_cap)) {
          err = std_destroy(&new_cap);
          (void)test_good(err, "2i, std_destroy()");
	}
      }
 
      eq = eq && (err1 == err2);
    }

    TEST_ASSERT(eq,TEST_WARNING,("%s (error codes don't compare)\n", testname));

}  /* test_bad_rights() */




PRIVATE	test_bad_size()

{

    errstat	err, err1, err2;
    bool	eq;
    capability	new_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_bad_size()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with bad filesize -1. */
    test_synch();

    err1 = b_read(&def_cap, DEF_OFFSET, buffer, (b_fsize)-1, &size);
    (void)test_bad(err1, "2a, b_read()", "size < 0");

    test_synch();

    err2 = b_create(&def_cap, def_buffer, (b_fsize)-1, DEF_COMMIT, &new_cap);
    if (!test_bad(err2, "2b, b_create()", "size < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2b, std_destroy()");
      }
    }

    eq = err1 == err2;

    test_synch();

    err1 = b_modify(&def_cap, DEF_OFFSET, aux_buffer, (b_fsize)-1, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err1, "2c, b_modify()", "size < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2c, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err2 = b_delete(&def_cap, DEF_OFFSET, (b_fsize)-1, DEF_COMMIT, &new_cap);
    if (!test_bad(err2, "2d, b_delete()", "size < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2d, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err1 = b_insert(&def_cap, DEF_OFFSET, aux_buffer, (b_fsize)-1, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err1, "2e, b_insert()", "size < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2e, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    /* Test with bad filesize 0. */
    test_synch();

    err2 = b_delete(&def_cap, DEF_OFFSET, ZERO, DEF_COMMIT, &new_cap);
    if (!test_bad(err2, "2f, b_delete()", "size = 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2f, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err1 = b_insert(&def_cap, DEF_OFFSET, aux_buffer, ZERO, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err1, "2g, b_insert()", "size = 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2g, std_destroy()");
      }
    }

    TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
                ("%s (error codes don't compare)\n", testname));

}  /* test_bad_size() */




PRIVATE	test_bad_offsets()

{

    errstat	err, err1, err2;
    bool	eq;
    capability	new_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_bad_offsets()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with bad offset -1. */
    test_synch();

    err1 = b_read(&def_cap, (b_fsize)-1, buffer, AUX_SIZE, &size);
    (void)test_bad(err1, "2a, b_read()", "offset < 0");

    test_synch();

    err2 = b_modify(&def_cap, (b_fsize)-1, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err2, "2b, b_modify()", "offset < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2b, std_destroy()");
      }
    }

    eq = err1 == err2;

    test_synch();

    err1 = b_delete(&def_cap, (b_fsize)-1, AUX_SIZE, DEF_COMMIT, &new_cap);
    if (!test_bad(err1, "2c, b_delete()", "offset < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2c, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err2 = b_insert(&def_cap, (b_fsize)-1, aux_buffer, AUX_SIZE, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err2, "2d, b_insert()", "offset < 0")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2d, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    /* Try reading from offset "filesize" + 1. */
    test_synch();

    err1 = b_read(&def_cap, DEF_SIZE+1, buffer, AUX_SIZE, &size);
    (void)test_bad(err1, "2e, b_read()", "offset > filesize");

    eq = eq && (err1 == err2);

    test_synch();

    err2 = b_modify(&def_cap, DEF_SIZE+1, aux_buffer, ZERO, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err2, "2f, b_modify()", "offset > filesize")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2f, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err1 = b_delete(&def_cap, DEF_SIZE+1, ZERO, DEF_COMMIT, &new_cap);
    if (!test_bad(err1, "2g, b_delete()", "offset > filesize")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2g, std_destroy()");
      }
    }

    eq = eq && (err1 == err2);

    test_synch();

    err2 = b_insert(&def_cap, DEF_SIZE+1, aux_buffer, ZERO, DEF_COMMIT,
								&new_cap);
    if (!test_bad(err2, "2h, b_insert()", "offset > filesize")) {
      if (!cap_cmp(&new_cap, &def_cap)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2h, std_destroy()");
      }
    }

    TEST_ASSERT(eq && err1 == err2, TEST_WARNING,
                ("%s (error codes don't compare)\n", testname));

}  /* test_bad_offsets() */




PRIVATE	test_bad_many()

{
    errstat	err;
    capability	new_cap;
    b_fsize	size;


    (void)strcpy(testname, "test_bad_many()");
    if (verbose)  printf("\n----  %s  ----\n", testname);

    /* Test with multiple bad arguments. */
    test_synch();

    err = b_read(bad_caps, (b_fsize)-1, buffer, (b_fsize)-1, &size);
    (void)test_bad(err, "2a, b_read()", "multiple arguments");

    test_synch();

    err = b_create(bad_caps, def_buffer, (b_fsize)-1, 0x00, &new_cap);
    if (!test_bad(err, "2b, b_create()", "multiple arguments")) {
      if (!cap_cmp(&new_cap, bad_caps)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2b, std_destroy()");
      }
    }

    test_synch();

    err = b_modify(bad_caps, (b_fsize)-1, def_buffer, (b_fsize)-1, 0x00,
								&new_cap);
    if (!test_bad(err, "2c, b_modify()", "multiple arguments")) {
      if (!cap_cmp(&new_cap, bad_caps)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2c, std_destroy()");
      }
    }

    test_synch();

    err = b_delete(bad_caps, (b_fsize)-1, (b_fsize)-1, 0x00, &new_cap);
    if (!test_bad(err, "2d, b_delete()", "multiple arguments")) {
      if (!cap_cmp(&new_cap, bad_caps)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2d, std_destroy()");
      }
    }

    test_synch();

    err = b_insert(bad_caps, (b_fsize)-1, def_buffer, (b_fsize)-1, 0x00,
								&new_cap);
    if (!test_bad(err, "2e, b_insert()", "multiple arguments")) {
      if (!cap_cmp(&new_cap, bad_caps)) {
        err = std_destroy(&new_cap);
        (void)test_good(err, "2e, std_destroy()");
      }
    }

}  /* test_bad_many() */




PRIVATE	int	usage(progname)
/* Print a message about how to use the test. */

char	progname[];
{
  fprintf(stderr, "Usage: %s [-vgbq] [-n <# of slaves>] [servername]\n",
								progname);
  exit(1);

}  /* usage() */




PRIVATE bool	make_badcaps()

{
  int		i;
  errstat	err;
  capability	tmp_cap;


  (void)strcpy(testname, "make_badcaps()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Generate a few bad capabilities from the server capability. */
  for (i = 0; i < MAXBADCAPS; i++)
    bad_caps[i] = def_cap;

  /* Change the address of the object. */
  for (i = 0; i < 3; i++)
    bad_caps[0].cap_priv.prv_object[i] = ~def_cap.cap_priv.prv_object[2-i];

  /* Change the rights field. */ 
  bad_caps[1].cap_priv.prv_rights ^= 0x01;

  /* Change the check field. */
  bad_caps[2].cap_priv.prv_random._portbytes[0] ^= 0x01;
  nbadcaps = 3;

  /* Copy the address of a restricted object to an unrestricted capability. */
  err = std_restrict(&def_cap, 0L, &tmp_cap);
  if (test_good(err, "std_restrict()")) {
    bad_caps[3] = empty_cap;
    for (i = 0; i < PORTSIZE; i++)
      bad_caps[3].cap_port._portbytes[i] = tmp_cap.cap_port._portbytes[i];
    for (i = 0; i < 3; i++)
      bad_caps[3].cap_priv.prv_object[i] = tmp_cap.cap_priv.prv_object[i];

    nbadcaps++;
  }

  /* Change the port-address of the server. */
  if (!quick) {
    /* RPCs from bad borts have a long time-out. */
    for (i = 0; i < PORTSIZE; i++)
      bad_caps[nbadcaps].cap_port._portbytes[i] =
				~def_cap.cap_port._portbytes[PORTSIZE-i-1];
    nbadcaps++;
  }

  /* Make a capability for a destroyed file. */
  err = b_create(&svr_cap, def_buffer, DEF_SIZE, DEF_COMMIT,
						&bad_caps[nbadcaps]);
  if (test_good(err, "b_create()")) {
    err = std_destroy(&bad_caps[nbadcaps]);
    if (test_good(err, "std_destroy()"))
      nbadcaps++;
  }

  /* Get the capability for a Soap directory. */
  err = name_lookup(".", &bad_caps[nbadcaps]);
  if (test_good(err, "name_lookup()"))
    nbadcaps++;

}  /* make_badcaps() */




PRIVATE	bool	test_init()
/* Initialize the global variables. */

{
  b_fsize	j;
  errstat	err;
  bool		ok;


  (void)strcpy(testname, "test_init()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Fill the buffers with every possible byte-value. */
  for (j = 0; j < DEF_SIZE; j++)
    aux_buffer[j] = ~(def_buffer[j] = (char)(j & 0xFF));

  /* Create an empty file. */
  err = b_create(&svr_cap, buffer, ZERO, DEF_COMMIT, &empty_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !cap_cmp(&empty_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok)
    return FALSE;

  /* Create a medium sized file. */
  err = b_create(&svr_cap, def_buffer, DEF_SIZE, DEF_COMMIT, &def_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !cap_cmp(&def_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok) {
    err = std_destroy(&empty_cap);
    (void)test_ok(err, "std_destroy()");
    return FALSE;
  }

  /* Create an uncommitted file. */
  err = b_create(&svr_cap, def_buffer, DEF_SIZE, BS_SAFETY, &uncommit_cap);
  if (ok = test_ok(err, "b_create()")) {
    TEST_ASSERT(ok = !cap_cmp(&uncommit_cap, &svr_cap), TEST_SERIOUS,
    ("(FATAL) %s, b_create() (existing capability): test aborted\n", testname));
  }

  if (!ok) {
    err = std_destroy(&def_cap);
    (void)test_ok(err, "std_destroy()");
    err = std_destroy(&empty_cap);
    (void)test_ok(err, "std_destroy()");
    return FALSE;
  }

  if (bad_only)
    make_badcaps();

  return TRUE;

}  /* test_init() */




PRIVATE	void	test_cleanup()

{
  bool	ok;


  (void)strcpy(testname, "test_cleanup()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  /* Get rid of all the temporary files. */
  ok = (std_destroy(&uncommit_cap) == STD_OK);
  ok = (std_destroy(&def_cap) == STD_OK) && ok;
  ok = (std_destroy(&empty_cap) == STD_OK) && ok;

  TEST_ASSERT(ok, TEST_WARNING,
                ("%s (couldn't clean up properly)\n", testname));

}  /* test_cleanup() */




/*ARGSUSED*/
PRIVATE	void	handler(sig)
/* Handles signals coming in for the master and counts them. */

int	sig;
{
  if (isslave) {
      if (sig != SIG_CONTINUE) {
	  printf("slave: unexpected signal %d\n", sig);
      }
  } else {
      if (sig != SIGINT) {
	  printf("master: unexpected signal %d\n", sig);
      }
  }
	  
  nsigs++;

}  /* handler() */




PRIVATE	bool	spawn(nprocs, progname)
/* Forks the specified number of slaves (default is 1). */

int	nprocs;
char	progname[];
{
  int	i;
  int	pid;
  char	fname[32];


  (void)strcpy(testname, "spawn()");
  if (verbose)  printf("\n----  %s  ----\n", testname);

  master = getpid();
  if (setpgrp(master, master) == -1) {
    TEST_ASSERT(FALSE, TEST_SERIOUS,
                        ("%s (couldn't setup process group)\n", testname));
    return FALSE;
  }

  for (i = 1; i <= nprocs; i++) {
    pid = fork();
    if (pid == -1) {
      TEST_ASSERT(FALSE, TEST_SERIOUS,
		 ("%s: %s (couldn't fork slave %d)\n", progname, testname, i));
    }
    else if (pid == 0) {
      sprintf(fname, "Tbulletc.err%d", i);
      if ((int)freopen(fname, "w", stdout) == -1) {
	fprintf(stderr, "child %d: freopen failed\n", i);
        return FALSE;
      }

      if ((int)signal(SIGINT, SIG_IGN) == -1) {
	  TEST_ASSERT(FALSE, TEST_SERIOUS,
			("%s (couldn't ignore SIGINT)\n", testname));
	  return FALSE;
      }
      if ((int)signal(SIG_CONTINUE, handler) == -1) {
        TEST_ASSERT(FALSE, TEST_SERIOUS,
			("%s (couldn't install signal handler)\n", testname));
        return FALSE;
      }

      isslave = TRUE;
      nslaves = nprocs;

      while (nsigs == 0) {
	  (void) sleep(nslaves);
	  if (nsigs == 0) {
	      printf("waiting for continue signal from master\n");
	  }
      }

      return TRUE;
    }
    else {
      if (setpgrp(pid, master) == -1) {
        TEST_ASSERT(FALSE, TEST_SERIOUS,
			("%s (couldn't add slave %d to group)\n", testname, i));
        (void)kill(pid, SIGKILL);
      }
      else
        nslaves++;
    }
  }

  if (nslaves) {
    if ((int)signal(SIGINT, handler) == -1) {
      TEST_ASSERT(FALSE, TEST_SERIOUS,
			("%s (couldn't install signal handler)\n", testname));
      nslaves = 0;
    }

    if ((int)signal(SIG_CONTINUE, SIG_IGN) == -1) {
      TEST_ASSERT(FALSE, TEST_SERIOUS,
			("%s (couldn't ignore continue signal)\n", testname));
      nslaves = 0;
    }
  }
  else
    TEST_ASSERT(nslaves > 0, TEST_SERIOUS,
			("%s (no slaves forked successfully)\n", testname));

  (void)sleep(nslaves);
  (void)killpg(master, SIG_CONTINUE);
  return nslaves;
      
}  /* spawn() */




PUBLIC	int	main(argc, argv)

int	argc;
char	*argv[];
{
  char		c, *server;
  errstat	err;
  extern char	*optarg;
  extern int	optind;
  int		nprocs = 1;
  int		status;


  /* Check the command-line  options. */
  while((c = getopt(argc, argv, "vgbqn:")) != EOF) {
    switch(c) {
      case 'v': verbose   = TRUE;  break;
      case 'g': good_only = TRUE;  break;
      case 'b': bad_only  = TRUE;  break;
      case 'q': quick     = TRUE;  break;
      case 'n': nprocs	  = atoi(optarg); break;

      default:  usage(argv[0]);
    }
  }

  /* Check the number of slaves to be forked. */
  if (nprocs < 1 || nprocs > 32)
    nprocs = 1;

  /* Do all the tests by default. */
  if (!good_only && !bad_only) 
    good_only = bad_only = TRUE;

  /* Decide which Bullet server to use. */
  if (optind == argc)
    server = DEF_BULLETSVR;
  else if (optind+1 == argc)
    server = argv[optind];
  else
    usage(argv[0]);

  /* Get the capability of the Bullet server. */
  if ((err = name_lookup(server, &svr_cap)) != STD_OK) {
    fprintf(stderr, "Couldn't get capability for server %s (%s).\n",
							server, err_why(err));
    exit(-1);
  }

  TEST_BEGIN(argv[0]);
  if (test_init()) {
    if (spawn(nprocs, argv[0])) {
      if (good_only) {
        test_b_read();
        test_b_create();
        test_b_modify();
        test_b_delete();
        test_b_insert();
	test_cache();
      }

      if (bad_only) {
        test_bad_caps();
        test_uncomm_file();
        test_bad_rights();
        test_bad_size();
        test_bad_offsets();
        test_bad_many();
      }
    }

    if (isslave)
      exit(0);
    else
      while (wait(&status) > 0);

    test_cleanup();
  }
  TEST_END();
  /*NOTREACHED*/
}  /* main() */
