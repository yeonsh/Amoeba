/*	@(#)Tsoap.h	1.3	96/03/04 14:03:31 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tsoap.h
 *
 * Original	: July 23, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Header file for all the sources of the Tsoap program.
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */

#ifndef	__TSOAP_H__
#define	__TSOAP_H__

#include	"amtools.h"
#include	"amoeba.h"
#include	"ampolicy.h"
#include	"capset.h"
#include	"soap/soap.h"
#include	"cmdreg.h"
#include	"stderr.h"

#ifndef	__TSOAP_C__
#define	MULTI_SRC
#endif	/* __TSOAP_C__ */

/* This must come after the conditional definition of MULTI_SRC. */
#include	"test.h"


#define	PRIVATE	static
#define PUBLIC
#define EXTERN	extern

#define TRUE	1
#define FALSE	0


typedef	int		bool;
typedef unsigned char	byte;


/* Global stuff to be used by all, if everybody cleans up after himself. */
#define	DEF_COLS	2
#define	MAX_COLS	SP_MAXCOLUMNS

#define	DEL	0
#define	MOD	1
#define	READ	2
#define	NOCOL	3
#define	BADCOL	4
#define	NORGT	5
#define	DEF	6


#ifdef	__TSOAP_C__

EXTERN	void	test_sp_append();
EXTERN	void	test_sp_masks();
EXTERN	void	test_sp_create();
EXTERN	void	test_sp_delete();
EXTERN	void	test_sp_lookup();
EXTERN	void	test_sp_replace();
EXTERN	void	test_sp_traverse();
EXTERN	void	test_sp_discard();
EXTERN	void	test_bad_capsets();
EXTERN	void	test_empty_dir();
EXTERN	void	test_empty_name();
EXTERN	void	test_bad_name();
EXTERN	void	test_no_entry();
EXTERN	void	test_bad_columns();
EXTERN	void	test_bad_rights();
EXTERN	void	test_no_access();
EXTERN	void	test_bad_access();
EXTERN	void	test_bad_many();
EXTERN	void	test_bad_misc();

#else

/* A global flag controlling the output. */
EXTERN	bool	verbose;	/* Give lots of info on tests. */

/* The name of the current test_procedure. */
EXTERN	char	testname[];

/* The path to the CWD. */
EXTERN	char	*testpath;

/* A few column headers. */
EXTERN	char	*no_colnames[];
EXTERN	char	*one_colname[];
EXTERN	char	*def_colnames[];
EXTERN	char	*all_colnames[];
EXTERN	char	*many_colnames[];

/* Column masks arrays. */
EXTERN	long	empty_masks[];
EXTERN	long	full_masks[];
EXTERN	long	def_masks[];
EXTERN	long	rgt_masks[][1];

/* Directory names. */
EXTERN	char	def_name[];
EXTERN	char	aux_name[];
EXTERN	char	long_name[];
EXTERN	char	bad_name[];

/* A few capability sets. */
EXTERN	capset	dir_capset, def_capset, aux_capset;
EXTERN	capset	bad_capsets[], rgt_capsets[];
EXTERN	errstat	cap_err[];
EXTERN	int	nbadcaps;


EXTERN	bool	test_ok();
EXTERN	bool	test_good();
EXTERN	bool	test_bad();
EXTERN	void	check_capsets();
EXTERN	void	check_masks();

#endif	/* __TSOAP_C__ */

#endif	/* __TSOAP_H__ */

