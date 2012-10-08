/*	@(#)Tbullet.h	1.4	96/03/04 14:03:42 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tbullet.h
 *
 * Original	: Nov 22, 1990.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Header file for all the sources of the Bullet test suite.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */

#ifndef	__TBULLET_H__
#define __TBULLET_H__

#include        "stdlib.h"
#include        "string.h"
#include        "amtools.h"
#include        "amoeba.h"
#include        "ampolicy.h"
#include        "bullet/bullet.h"
#include        "module/cap.h"
#include        "cmdreg.h"
#include        "stderr.h"

#ifndef	__TBULLET_C__
#define	MULTI_SRC
#endif	/* __TBULLET_C__ */

/* This must come after the conditional definition of MULTI_SRC. */
#include        "test.h"


#define PRIVATE static
#define PUBLIC
#define EXTERN  extern

#define TRUE    1
#define FALSE   0


typedef int             bool;
typedef unsigned char   byte;


/* Global stuff to be used by all, if everybody cleans up after himself. */
#define K               1024L
#define ZERO            (b_fsize)0
#define ONE             (b_fsize)1
#define AVG_SIZE        (b_fsize)(500L*K)
#define AVG_OFFS        (b_fsize)(200L*K)
#define AUX_SIZE        (AVG_SIZE - AVG_OFFS)
#define BIG_SIZE        (AVG_SIZE + AUX_SIZE)
#define NEG_ONE         (-ONE)
#define NEG_AVG         (-AVG_SIZE)
#define SAFE_COMMIT     (BS_COMMIT | BS_SAFETY)


#ifdef	__TBULLET_C__

EXTERN	void	test_b_size();
EXTERN	void	test_b_read();
EXTERN	void	test_b_create();
EXTERN	void	test_b_modify();
EXTERN	void	test_b_delete();
EXTERN	void	test_b_insert();
EXTERN	void	test_combi();
EXTERN	void	test_bad_caps();
EXTERN	void	test_uncomm_file();
EXTERN	void	test_bad_rights();
EXTERN	void	test_bad_size();
EXTERN	void	test_bad_offsets();
EXTERN	void	test_bad_many();

#else

EXTERN	bool	verbose;
EXTERN	char	testname[];	/* The name of the current test procedure. */
EXTERN	int	nbadcaps;	/* Actual number of bad capabilities. */
EXTERN	char	read_buf[];	/* Buffer for reading data. */
EXTERN	char	write_buf[];	/* Buffer for writing data. */
EXTERN	errstat	cap_err[];	/* Expected errors for bad capabilities. */

EXTERN	capability      svr_cap, empty_cap, one_cap, commit_cap, uncommit_cap,
			bad_caps[];


EXTERN	bool	test_ok();
EXTERN	bool	test_good();
EXTERN	bool	test_bad();
EXTERN	bool	obj_cmp();

#endif	/* __TBULLET_C__ */

#endif	/* __TBULLET_H__ */

