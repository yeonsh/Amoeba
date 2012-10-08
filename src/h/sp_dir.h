/*	@(#)sp_dir.h	1.4	96/02/27 10:26:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SP_DIR_H__
#define __SP_DIR_H__

/*
 * MAX_NAME is the maximum length of a directory entry name and
 * is defined in limits.h
 */

struct sp_direct {
    char        *d_name;	/* name (up to MAX_NAME + 1) */
    int          d_namlen;	/* redundant, but compatible */
    rights_bits *d_columns;	/* rights masks in the columns */
};

typedef struct _dirdesc {
    capset    dd_capset;
    int       dd_ncols;
    int       dd_nrows;
    char    **dd_colnames;
    struct sp_direct *dd_rows;
    int       dd_curpos;
} SP_DIR;

#define	sp_closedir	_sp_closedir
#define	sp_opendir	_sp_opendir
#define	sp_readdir	_sp_readdir
#define	sp_rewinddir	_sp_rewinddir
#define	sp_seekdir	_sp_seekdir
#define	sp_telldir	_sp_telldir
#define	sp_list		_sp_list

void    sp_closedir  _ARGS((SP_DIR *_dd));
SP_DIR *sp_opendir   _ARGS((char *_name));
struct sp_direct
       *sp_readdir   _ARGS((SP_DIR *_dd));
void    sp_rewinddir _ARGS((SP_DIR *_dd));
void    sp_seekdir   _ARGS((SP_DIR *_dd, long _pos));
long    sp_telldir   _ARGS((SP_DIR *_dd));

errstat sp_list	     _ARGS((capset *_dir, SP_DIR **_ddp));

#endif /* __SP_DIR_H__ */
