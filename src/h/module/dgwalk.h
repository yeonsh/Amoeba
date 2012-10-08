/*	@(#)dgwalk.h	1.3	96/02/27 10:32:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Definition file for external interface to dgwalk set of
   routines

   Author: E.G. Keizer
	   Faculteit Wiskunde & Informatika
	   Vrije Universiteit
	   Amsterdam

   These routines create a set of directory graphs that enables
   an amoeba program(mer) to walk through a set of directories.

*/

#include <stderr.h>
#include <_ARGS.h>

#ifndef __MODULE_DGWALK_H__
#define __MODULE_DGWALK_H__

/* A few typedefs are needed for for recursion */
typedef struct dgw_objtag dgw_object ;
typedef struct dgw_pathtag dgw_paths ;
typedef struct dgw_dirtag dgw_dirlist ;

/* The struct passed to the first routine telling it what to do */

typedef struct {
	short		mode;		/* Ad hoc or All */
	dgw_paths	*entry;		/* Entry into tree */
				/* Allowed dir servers, null term list */
	capset		**servers;
	int		(*dodir)();	/* Do_dir_entry */
	int		(*testdir)();	/* Test_dir_entry */
	int		(*doagain)();	/* Last chance */
} dgw_params;

/* Contents of dgw_mode */
#define	DGW_ALL		1	/* Otherwise ADHOC */

/* Possible return values of dodir/testdir */
#define DGW_OK		1	/* It is fine, just continue */
#define DGW_STOP	2	/* Don't bother me about
				   this again, will you? */
#define DGW_AGAIN	3	/* Try again please, don't
				   traverse subdirs now */
#define DGW_QUIT	4	/* Stop processing, if possible return STD_INTR
				   */

/* Structure passed to dodir */
struct dgw_pathtag {
	char			*entry;
	dgw_paths 		*dotdot;
	capability		cap;
	dgw_paths		*next;
	/* Internal */
	short			done;
};

struct dgw_dirtag {
	dgw_object	*dir;
	dgw_dirlist	*next;
};

/* The rest can only be seen through calls of walkit() */

struct dgw_objtag {
	dgw_dirlist	*subdirs;	/* Subdir obj's */
	dgw_paths	*paths;		/* Ways to get in */
	capability	*cap;		/* Ptr to a cap */
	/* Private, not to be used outside dgwalk.c */
	int		flag;
};

/* flag bit values */
#define	DGW__DONE	1
#define	DGW__IGNORE	2	/* Totally over and done with */
#define DGW__AGAIN	4	/* Do try again */
#define	DGW__TESTED	8	/* testdir has been called */


#define	DGW_ONULL	(dgw_object *)NULL
#define DGW_LNULL	(dgw_dirlist *)NULL

/* The routines */

#define	dgwalk		_dgwalk
#define	dgwexpand	_dgwexpand

errstat	dgwalk _ARGS(( dgw_params * params ));
errstat dgwexpand _ARGS(( dgw_paths * p,
		void (*proc)(char * a, capset * b, capset * c, char * d) ));

#endif /* __MODULE_DGWALK_H__ */
