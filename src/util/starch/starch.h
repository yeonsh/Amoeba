/*	@(#)starch.h	1.6	96/02/27 13:17:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
	
   This file contains the definitions used for internal data structures.
   The definitions of tape_layout can be found in blocks.h.

*/

#include <amoeba.h>
#include <capset.h>
#include <module/name.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/soap/soap.h>
#include <server/bullet/bullet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <time.h> */

#ifdef MEMDEBUG
#include "/home/keie/src/mnemosyne/mnemosyne.h"
#define getmem(size)	((ptr)calloc((unsigned)size,1))
#endif

/* General setup */

/* Tuning parameters */

#define	MEM_EXTRA	50*1024		/* Held in reserve */

#define USE_DEFAULT	"/-"		/* Default for -u */

#define BULLET_DEFAULT	"/profile/cap/bulletsvr/default"

#define N_ID_IN_BLOCK	256		/* # id's in one id block */

#define CAPS_IN_ID	2		/* # of capabilities in an id */
					/* Should be filled with normal
					   number of cap sets for a file.
					 */

#define	BREAD_SIZE	32768		/* Size of one b_read */

#define MAX_COLS	10		/* Max # columns for SP_MASK */

#define TOP_NCOLS	4		/* # columns for top entries */

#ifdef AMOEBA
#define	MULTI_THREADED
#endif

#ifdef MULTI_THREADED
#define N_IOBUFS	80		/* Number of IO buffers */
#endif

/* The parameters used to create `default' directories are in starch.c */

/* The following should only be considered when porting this program */

#define UnsChar	1		/* Have unsigned char */

#ifdef __STDC__
# ifndef UnsChar
# define UnsChar	1
# endif
typedef void *ptr ;
#endif


#define	ALL_RIGHTS	PRV_ALL_RIGHTS	/* Maximum amoeba rights */
#define CAP_SZ		16		/* The size of a single capability */

#ifdef UnsChar
typedef unsigned char	byte ;
#define byteval(x)	(x)
#else
typedef char		byte ;
#define byteval(x)	((x)&0xFF)
#endif

#ifndef __STDC__
typedef byte		*ptr;
#endif

/* End of tuning parameters */

#define	ARCHID		"starch"	/* Put in archive */
#define ARCHVERS	"1"		/* Put in archive */

/* The first major data structure is the internal capability table
   It is a array indexed by internal capability number which are assigned
   from 1 upwards. Capability #1 is used to indicate a fictional
   directory with the names used to create the archive/dump.
   Capability #0 does not exists and is used for error returns and such.

   Access is in the way blocks are accessed in a UNIX inode i.e.
   the first 7 are accessed directly the 8th double indirect and
   the 9th triple indirect. This allows for a total accessible storage of
   722,731,015 bytes. The routines for access to this structure are
   carefully constructed to allow `paging' part of the table to a disk file.

   Each entry contains a type indicator, a byte with flags, the capability
   number of a parent, for directories the address of a table
   containing the names of all entries and lastly a `union' with
    - under -o and -c	a capability set containing the union of all
      			capabilities seen for that object
    - under -i and -c	the destination capability

*/

struct cap_t {
	unsigned short		flag ;
	char			type ;
	char			lock ;
	unsigned		server_no ;
	long			parent ;
	struct dirlist		*dir ;
	/* The next two elements need to be in this order, see cs_reset */
	capset			cs ;
	suite			caps[CAPS_IN_ID] ;
};

#define	cn_flag(v)	((v)->flag)
#define	cn_type(v)	((v)->type)
#define	cn_server(v)	((v)->server_no)
#define	cn_lock(v)	((v)->lock)
#define	cn_parent(v)	((v)->parent)
#define	cn_dir(v)	((v)->dir)
#define	cn_capsrc(v)	((v)->cs)
#define	cn_capdst(v)	((v)->cs)
#define	cn_suite(v)	((v)->caps)

/* Flag values */
/* Methods to deal with entries */
#define M_UNKNOWN	0	/* Intermediate value */
#define	M_IGNORE	1
#define M_CONTENT	2	/* Dump contents of file */
#define M_CAP		4	/* Dump capability */
#define	EXTRACT		8	/* Extract this cap, -i & -c */
#define ERROR		16	/* Error in this cap */
#define EXISTS		32	/* Use existing cap, -i & -c */
#define LINKED		64	/* Linked, link id is in parent field */
#define LOOPED		128	/* Used to prevent loops */
#define VALID		256	/* Valid capdst field */
#define REACHABLE	512	/* Capdst accessible through soap */
#define UNR_ENTRY	1024	/* Prospective entry point for cre_unreach */
#define UNR_DIR		2048	/* Directory not normally reachable */
#define CREATED		4096	/* Created by get.c */
#define NEWER		8192	/* Current version is newer */

/* Type values */
#define C_DIR		1
#define C_FILE		2
#define C_OTHER		3
#define C_EMPTY		4
#define C_NONE		5	/* Entry has disappeared */

/* Dirlists are simply arrays of structures with two entries, the
   internal cap # and a ptr to the name
*/

struct dirlist {
	sp_seqno_t	d_version ;	/* Current version number */
	unsigned	c_count ;	/* # of columns */
	char		**c_names ;	/* column names */
	unsigned	r_count ;	/* # of entries */
	struct dir_ent	*r_entries ;	/* pointer to first of table */
} ;

struct dir_ent {
	char		*name ;
	long		*rights ;
	long 		time ;
	short		flags ;		
	long		capno ;
	long		caprights ;
} ;

#define acq_cap(dir_ent)	((dir_ent)->capno)

/* Values of flags in Dir_ent's, only used by starch -i */
#define	ENT_EXISTS	1		/* Destination already exists */
#define ENT_EXTRACT	2		/* To be extracted */
#define ENT_VALID	4		/* Initialised entry */
#define ENT_DONE	8		/* Entry installed in soap */

extern	int		max_capid ;	/* Max cap id used */

/* Definitions for program flags */
extern	char		*progname ;
extern	enum action {
	NONE, IN, OUT, LIST, RESCAN, NAMES, PRIV
	} act; /* o,i,t,c,g */
extern	int		verbose ;	/* v */
extern	int		io_type ;	/* l & -m & -M */
extern	int		force ;		/* w */
extern	int		retain ;	/* r */
extern	int		debug ;		/* d */
extern	int		equiv ;		/* e */
extern	char		*unreach_path ;	/* k */
extern	unsigned long	sz_medium ;	/* s */
extern	int		stats ;		/* S */

struct	pathlist {
	char		*path ;
	capset		caps ;
	struct pathlist	*next ;
} ;

struct	patternlist {
	char			*pattern ;
	struct patternlist	*next ;
} ;

struct	serverlist {
	char			*path ;
	capability		server ;
	int			number ;
	char			type ;
	char			i_type ;
	struct serverlist	*next ;
} ;

extern	struct pathlist		*xlist ;	/* x */
extern	struct patternlist	*xxlist ;	/* X */

/* The list of servers in the archive */
extern  struct serverlist	*arch_slist ;

extern	char			*use_types ;	/* u */
extern	char			*asis_types ;	/* y */
extern	char			*ign_types ;	/* z */
extern	struct serverlist	*use_servers ;	/* -U */
extern	struct serverlist	*asis_servers ;	/* -Y */
extern	struct serverlist	*ign_servers ;	/* -Z */
extern	struct serverlist	*file_servers ;	/* -B */
extern	struct serverlist	*dir_servers ;	/* -D */
extern	struct serverlist	*priv_servers ;	/* -P */

extern	struct pathlist		*files ;	/* rest of arguments  */

/* Used in several places */
extern char		*archive ;	/* current archive name */
extern char		*ar_type ;	/* -f, -a or -A archive */
extern capset		arch_cs ;	/* capset for archive */
extern capability	arch_cap ;	/* cap for archive */
extern int		arch_valid ;	/* valid capability in arch_cap */
extern capset		arch_dir ;	/* the capability with the dir entry */
extern int		do_stdio ;	/* Use stdin/stdout */
extern int		description_dump ;/* Dump description of dump */
extern FILE		*i_talkfile;	/* input from terminal */
extern FILE		*o_talkfile;	/* output to terminal */
extern int		in_multi ;	/* More then one input */

/* Multi archive admin */
struct f_list_t {
	char		*path ;
	int		type ;
	struct f_list_t	*next ;
} ;

/* Possible types for io_type and f_list_t.type */
#define IO_FILE		0		/* A bullet file, (default) */
#define IO_TAPE		1		/* Tape (-m) */
#define IO_TAPE_NR	2		/* Tape (-M) */
#define IO_FLOP		3		/* Floppy disk */

struct f_head_t {
	struct f_list_t	*first ;
	struct f_list_t	*next ;
	struct f_list_t	*last ;
};

extern struct f_head_t	arch_list ;	/* List of '-f' archives */
extern struct f_head_t	ddi_list ;	/* List of '-A' archives */
extern struct f_head_t	ddo_list ;	/* List of '-a' archives */

/* `defaults' for directories */
extern char		*sts_columns[] ;/* column names for sp_create */
extern int		sts_ncols ;	/* # columns */
extern long		sts_rights[] ;	/* default rights */

/* default method to deal with an entry */
extern int		def_method ;

/* Service definitions */
#define	MYMEM(type)	((type *)getmem(sizeof(type)))

/* Routines used in the program */
extern void		syntax();
#ifdef __STDC__
extern void		ini_talk(void);
extern int		answer(char *poss);
extern void		fatal(char *fmt, ... ) ;
extern void		t_fatal(int mt_flag, char *fmt, ... ) ;
extern void		t_finish(void) ;
extern void		warning(char *fmt, ... ) ;
extern void		message(char *fmt, ... ) ;
extern void		err_flush(void);
#ifdef MULTI_THREADED
extern void		err_lock(void);
extern void		err_unlock(void);
#endif
extern char		*keepstr(char *str) ;
extern ptr		keepmem(char *mem, unsigned n) ;
#ifndef MEMDEBUG
extern ptr		getmem(size_t size);
#endif
extern ptr		adjmem(ptr mem,size_t size);
extern void		addcap(capset *object_cs,capability *new_cap);
extern long		dircap(capability *cap, int *new,struct cap_t **pp_id);
extern long		findcs(capset *cs, int res, long pid);
extern void		ar_flush(void);
extern int		cs_overlap(capset *recv, capset *cs);
extern void		cs_transfer(capset *recv, capset *cs);
extern void		cs_insert(capset *recv, capability *cap);
extern void		cs_reset(capset *cs);
extern errstat		cs_info(capset *cs, char *s, int in_len, int *out_len);
extern errstat		cs_b_read(capset *cs, b_fsize offset, char *buf, b_fsize to_read, b_fsize *were_read);
extern errstat		cs_b_size(capset *cs, b_fsize *size);
extern errstat		cre_empty(capability *cap);
extern void		del_unreach(void);
extern void		cre_unreach(void);
extern void		killed(int sig);
extern void		dir_free(struct cap_t *);
extern capset		**srch_servers(void);
extern void		fr_servers(capset **);
extern void		f_insert(struct f_head_t *, char *,int);
extern int		f_next(struct f_head_t *);
#else
extern void		ini_talk();
extern int		answer();
extern void		fatal();
extern void		t_fatal() ;
extern void		t_finish() ;
extern void		warning();
extern void		message() ;
extern void		err_flush();
#ifdef MULTI_THREADED
extern void		err_lock();
extern void		err_unlock();
#endif
extern char		*keepstr();
extern ptr		keepmem();
#ifndef MEMDEBUG
extern ptr		getmem();
#endif
extern ptr		adjmem();
extern long		newset();
extern void		addcap();
extern long		dircap();
extern long		findcs();
extern void		ar_flush();
extern int		cs_overlap();
extern void		cs_transfer();
extern void		cs_insert();
extern void		cs_reset();
extern errstat		cs_info();
extern errstat		cs_b_read();
extern errstat		cs_b_size();
extern errstat		cre_empty();
extern void		del_unreach();
extern void		cre_unreach();
extern void		killed();
extern void		dir_free();
extern capset		**srch_servers();
extern void		fr_servers();
extern void		f_insert();
extern int		f_next();
#endif
extern struct cap_t	*id_get();
extern struct cap_t	*lid_get();
extern long		id_link();
extern char		*id_pn();
extern void		soap_err();
extern void		bullet_err();

/* Exception return value, used in threads to indicate that the thread
   needs to die neatly.
 */
#define	EXCEPTION	-1
/* OK return value */
#define OK		1
/* NOT-OK non-fatal return value */
#define NOT_OK		0

/* Routines used by the program */

#include <module/tod.h>
#include <module/stdcmd.h>
#include <module/tape.h>
#include <module/prv.h>
#include <module/ar.h>

/* An abbreviation */
#define	SAME_SERVER(a,b)	(PORTCMP(&((a)->cap_port),&((b)->cap_port)))
