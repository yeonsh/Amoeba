/*	@(#)soap.h	1.6	96/02/27 10:38:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SOAP_H__
#define __SOAP_H__

/* Some miscellaneous constants for this directory service: */

#define SP_BUFSIZE	(8 * 1024)
#define SP_MAXCOLUMNS	4
#define SP_NTRY		3		/* # retries in idempotent ops */

/* Commands: */

#define SP_CREATE	(SP_FIRST_COM + 0)
#define SP_DISCARD	(SP_FIRST_COM + 1)
#define SP_LIST		(SP_FIRST_COM + 2)
#define SP_APPEND	(SP_FIRST_COM + 3)
#define SP_CHMOD	(SP_FIRST_COM + 4)
#define SP_DELETE	(SP_FIRST_COM + 5)
#define SP_LOOKUP	(SP_FIRST_COM + 6)
#define SP_SETLOOKUP	(SP_FIRST_COM + 7)
#define SP_INSTALL	(SP_FIRST_COM + 8)
#define SP_PUTTYPE	(SP_FIRST_COM + 9)
#define SP_REPLACE	(SP_FIRST_COM + 10)
#define SP_GETMASKS	(SP_FIRST_COM + 11)
#define SP_GETSEQNR	(SP_FIRST_COM + 12)

#ifdef SOAP_DIR_SEQNR
#define SP_USER_LAST	(SP_FIRST_COM + 20) /* some room for more user cmds */
#else
/* Mark the last command: */
#define SP_USER_LAST	(SP_FIRST_COM + 11)
#endif

/* Errors: */

#define SP_UNAVAIL	((errstat) SP_FIRST_ERR -1)
#define SP_NOTEMPTY	((errstat) SP_FIRST_ERR -2)
#define SP_UNREACH	((errstat) SP_FIRST_ERR -3)
#define SP_CLASH	((errstat) SP_FIRST_ERR -4)

/* Rights: */

#define SP_COLMASK	((1L << SP_MAXCOLUMNS) - 1)
#define SP_DELRGT	0x80L
#define SP_MODRGT	0x40L

/* Other stuff: */

#define SP_DEFAULT	((capset *) 0)

typedef struct {
    capset     se_capset;
    char      *se_name;
} sp_entry;

typedef struct {
    short      sr_status;
    capability sr_typecap;
    long       sr_time;
    capset     sr_capset;
} sp_result;

typedef struct {
    uint32 seq[2];  /* 64 bits sequence number */
} sp_seqno_t;
 
/* For name-space pollution reduction: */

#define	sp_append	_sp_append
#define	sp_chmod	_sp_chmod
#define	sp_create	_sp_create
#define	sp_delete	_sp_delete
#define	sp_discard	_sp_discard
#define	sp_getmasks	_sp_getmasks
#define	sp_getseqno	_sp_getseqno
#define	sp_install	_sp_install
#define	sp_lookup	_sp_lookup
#define	sp_replace	_sp_replace
#define	sp_setlookup	_sp_setlookup
#define	sp_traverse	_sp_traverse
#define	sp_get_workdir	_sp_get_workdir
#define	sp_get_rootdir	_sp_get_rootdir

/* Prototypes for the stubs: */

errstat sp_append    _ARGS((capset *_dir, const char *_name, capset *_cs,
			    int _ncols, rights_bits _cols[]));
errstat sp_chmod     _ARGS((capset *_dir, const char *_name,
			    int _ncols, rights_bits _cols[]));
errstat sp_create    _ARGS((capset *_server, char *_columns[], capset *_dir));
errstat sp_delete    _ARGS((capset *_dir, const char *_name));
errstat sp_discard   _ARGS((capset *_dir));
errstat sp_getmasks  _ARGS((capset *_dir, const char *_name,
			    int _ncols, rights_bits _cols[]));
errstat sp_getseqno  _ARGS((capset *_dir, sp_seqno_t *_seqno));
errstat sp_install   _ARGS((int _n, sp_entry _entries[],
			    capset _capsets[], capability *_oldcaps[]));
errstat sp_lookup    _ARGS((capset *_dir, const char *_path, capset *_cs));
errstat sp_replace   _ARGS((capset *_dir, const char *_name, capset *_cs));
errstat sp_setlookup _ARGS((int _n, sp_entry _in[], sp_result _out[]));
errstat sp_traverse  _ARGS((capset *_dir, const char **_path, capset *_last));
errstat sp_get_workdir _ARGS((capset *_work));
errstat sp_get_rootdir _ARGS((capset *_root));

/* Other soap related library routines */
#define	sp_mkdir	_sp_mkdir
#define	sp_mask		_sp_mask
#define	sp_init		_sp_init
#define	sp_alloc	_sp_alloc

errstat	sp_mkdir _ARGS((capset *_start, char *_path, char **_colnames));
int	sp_mask _ARGS((int _nmasks, long *_maskar)); 
errstat sp_init	_ARGS((void));
char *	sp_alloc _ARGS((unsigned int _size, unsigned int _n));

#endif /* __SOAP_H__ */
