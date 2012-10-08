/*	@(#)Tginit.h	1.3	96/02/27 10:55:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tginit.h
 *
 * Original	: March 24, 1992.
 * Author(s)	: Irina
 * Description	: Header file for all the sources of the ti_test_all.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */

#ifndef	__TGINIT_H_
#define __TGINIT_H_

#include        "stdlib.h"
#include        "string.h"
#include        "amtools.h"
#include        "amoeba.h"
#include        "ampolicy.h"
#include        "bullet/bullet.h"
#include        "cmdreg.h"
#include        "stderr.h"
#include        "random/random.h"
#include        "exception.h"
#include        "fault.h"
#include        "module/signals.h"
#include        "module/strmisc.h"
#include        "module/cap.h"
#include        "semaphore.h"
#include	"thread.h"
#include        "setjmp.h"

#define PRIVATE static
#define PUBLIC
#define EXTERN  extern

#define TRUE    1
#define FALSE   0

typedef int             bool;
typedef unsigned char   byte;
typedef struct {
    int     testno;
    char  * buf;  /* arguments for bprint */
    char  * p;
    char  * endi;
    int     flag;
    int   * aident;
} BUFPRINT, *ABUFPRINT;

EXTERN void bufprint _ARGS((BUFPRINT *abuf, char *format, ...));


/* Global stuff to be used by all, if everybody cleans up after himself. */

#define K               1024L
#define ZERO            ((b_fsize) 0)
#define ONE             ((b_fsize) 1)
#define AVG_SIZE        ((b_fsize) BS_REQBUFSZ)

#define BIG_SIZE        ((b_fsize) (BS_REQBUFSZ * 2))
#define NEG_ONE         ((b_fsize) -1)
#define NEG_AVG         ((b_fsize) (-BS_REQBUFSZ))

#define SAFE_COMMIT     (BS_COMMIT | BS_SAFETY)
#define NO_COMMIT	(0)


#define ALL_BS_RIGHT (BS_RGT_CREATE|BS_RGT_READ|BS_RGT_MODIFY|BS_RGT_DESTROY)

/* for create_file() */
#define VERIFY_NEW_DIF_OLD TRUE 

/* for verify_file() */
#define EQUAL_SIZE TRUE 

/* for execute_restrict() */
#define GOOD_CAPABILITY TRUE 


#ifndef __TGINIT_C_

EXTERN  void    init_exception_flag _ARGS((ABUFPRINT));
EXTERN  void    test_for_exception  _ARGS((ABUFPRINT));	
EXTERN	bool	obj_cmp   _ARGS((capability *, capability *, ABUFPRINT));
EXTERN  void    bwrite _ARGS((char *, ABUFPRINT));
EXTERN  void    make_cap_check_corrupt _ARGS((capability *, capability *)); 
EXTERN  void    make_cap_rights_corrupt _ARGS((capability *, capability *)); 
EXTERN  bool    create_file   _ARGS((capability *, char *, b_fsize, int,
                                      capability *, bool, ABUFPRINT));
EXTERN  bool    verify_file   _ARGS((char *, char *, b_fsize, 
                                     b_fsize, capability *, 
                                     bool, b_fsize, ABUFPRINT));
				   
EXTERN  bool    execute_restrict _ARGS((capability *, capability *,rights_bits,
                                 char *, bool,  ABUFPRINT));
EXTERN  bool    init_buffer   _ARGS((char *, ABUFPRINT, b_fsize));
EXTERN  bool    cap_server    _ARGS((capability *, char *, ABUFPRINT));

/* Some global flags controlling the output and tests. */
EXTERN	bool	verbose;	/* Give lots of info on tests. */
EXTERN  int     identg;
EXTERN  int     identl;

#endif  /* __TGINIT_C_ */
#endif	/* __TGINIT_H_ */

