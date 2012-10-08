/*	@(#)stubcode.h	1.2	94/04/07 14:39:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"

/*
**	This file contains the data structures needed for the stubcode
**	code generator.
*/

#ifndef __STUBCODE__
#define __STUBCODE__

/*
**	The structure to point to an array or array parameter.
*/

typedef struct s_Link {
    struct s_Link        *psNext;     /* To make a list                       */
    struct s_PythonTree  *psLink;     /* To point to the parameter	      */} TsLink, *TpsLink;

/*
**	This structure describes the Python object
*/

typedef struct s_PythonTree {
    struct s_PythonTree  *psNext;     /* Points to the next object in a tuple */
    int                  iType;       /* The type of the object. See below.   */
    int                  iFlags;      /* See below.                           */
    struct itlist        *psType;     /* All the vital information about
                                         the object calculated by AIL.        */
    struct s_Link	 *psArrInfo;  /* If the type is an array then here the
                                         information about its parameter(s)
                                         is stored.                           */
    struct s_PythonTree  *psPrim;     /* If the type was a tuple or list the
                                         psPrim points to the primative type. */
} TsPythonTree, *TpsPythonTree;

/*
**	The Python object types.
*/

#define pt_tuple	0
#define pt_list		1
#define pt_string	2
#define pt_cap		3
#define pt_integer	4
#define pt_arglist	5

/*
**	The flags used in the tree
*/

#define PF_ARRPAR	0x0001	/*  The parameter is used for an array size  */
#define PF_FIXARR	0x0002	/*  The array is of fixed size		     */
#define PF_VARARR	0x0004	/*  The array is of variable size	     */
#define PF_IN		0x0008	/*  The parameter goes in		     */
#define PF_OUT		0x0010	/*  The parameter goes out		     */
#define PF_EXPR		0x0020	/*  The array size is given as an expression */
#define PF_CALC_IN	0x0040	/*  The integer can be calculated on in	     */
#define PF_CALC_OUT	0x0100	/*  The integer can be calculated on out     */
#define PF_CALC		(PF_CALC_IN | PF_CALC_OUT)
#define PF_INTUPLE	0x0200	/*   The parameter is part of a tuple	     */
#define PF_INLIST	0x0400  /*   The parameter is in a list		     */


TpsPythonTree	xpsOrderArgs ARGS((struct optr *));
long		xlGetType ARGS((struct typdesc *));
long		xlSetFlags ARGS((char *));

#endif __STUBCODE__
