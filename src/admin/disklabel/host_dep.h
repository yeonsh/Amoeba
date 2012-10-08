/*	@(#)host_dep.h	1.2	94/04/06 11:43:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if !defined(__HOST_DEP_H__)
#define	__HOST_DEP_H__

/*
 * Host operating system disk label dependencies
 *
 *	The following structure defines the set of functions that are needed
 *	in the disk labelling process.  For each supported host OS there should
 *	be a set of these functions.
 *
 * Author: Gregory J. Sharp, June 1992
 */

typedef struct host_dep	host_dep;
struct host_dep {
    int		(*f_getlabel)	  _ARGS(( capability *, char *, disklabel * ));
    int		(*f_validlabel)	  _ARGS(( char * ));
    disk_addr	(*f_partn_offset) _ARGS(( disklabel * ));
    void	(*f_labeldisk)	  _ARGS(( dsktab *, int ));
    void	(*f_printlabel)	  _ARGS(( char * ));
    int		f_endian;
};

/* Valid values for f_endian field */
#define	BIG_E		0
#define	LITTLE_E	1

#endif /* __HOST_DEP_H__ */
