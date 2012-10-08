/*	@(#)dir_info.h	1.2	94/04/07 15:04:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _DIR_INFO_H
#define _DIR_INFO_H

/*
 * directory contents maintenance
 */

#include "amoeba.h"
#include "capset.h"

struct dir_info {
    int		     dir_flags;	 /* misc flags; see below */
    int	 	     n_sub_dirs; /* number of subdirectories */
    int              n_files;	 /* number of files in the directory */
    long	    *fsizes;	 /* file sizes on various file servers */
    long	    *cumfsizes;	 /* cumulative file sizes in sub directories */
    char	    *dir_name;	 /* pathname of this directory */
    struct dir_info *dir_parent; /* parent dir_info node */
    struct dir_info *dir_childs; /* first child */
    struct dir_info *dir_sister; /* next child with same parent */

/* private: */
    capset	     dir_cap;	 /* capset of this directory */
    struct dir_info *dir_next;	 /* next on the list */
};

/* flags in dir_info structure */
#define DIR_REPORTED	0x01
#define DIR_EXPANDED	0x02
#define DIR_ARGUMENT	0x04
#define DIR_NULLDIR	0x08
#define DIR_CHILD	0x10

struct dir_info *search_dir_by_cap _ARGS((capability *dircap));
struct dir_info *search_dir	   _ARGS((capset *dircap));


#endif /* _DIR_INFO_H */
