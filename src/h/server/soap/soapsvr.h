/*	@(#)soapsvr.h	1.5	96/02/27 10:38:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SOAPSVR_H__
#define __SOAPSVR_H__

#define SP_MAX_TIME2LIVE 48	/* Number of STD_AGE's required to
				 * to expire a freshly STD_TOUCH'ed
				 * directory object.
				 */

/* Commands used by Soap internally: */
#define SP_FIRST_ADM	(SP_USER_LAST + 1)
#define SP_INTENT	(SP_FIRST_ADM + 0) /* no longer used */
#define SP_SEQ		(SP_FIRST_ADM + 1)
#define SP_COPY		(SP_FIRST_ADM + 2)
#define SP_TICK		(SP_FIRST_ADM + 3)
#define SP_NEWBULLET	(SP_FIRST_ADM + 4)
#define SP_AGE		(SP_FIRST_ADM + 5)
#define SP_EXPIRE	(SP_FIRST_ADM + 6)
#define SP_INITDONE	(SP_FIRST_ADM + 7)
#define SP_INTENTIONS	(SP_FIRST_ADM + 8) /* SP_INTENT now has a new format */
#define SP_SHUTDOWN	(SP_FIRST_ADM + 9) /* we really need STD_SHUTDOWN */

#ifdef SOAP_GROUP
/* group command values */
#define SP_GRP_JOIN	(SP_FIRST_ADM + 10)
#define SP_GRP_LEAVE	(SP_FIRST_ADM + 11)
#define SP_GRP_RESET	(SP_FIRST_ADM + 12)
#define SP_GRP_STATUS	(SP_FIRST_ADM + 13)

#define SP_GET_STATE	(SP_FIRST_ADM + 14)
#define SP_DIR_SEQS	(SP_FIRST_ADM + 15)
#define SP_DIRS		(SP_FIRST_ADM + 16)
#define SP_DEL_DIR	(SP_FIRST_ADM + 17)
#define SP_TOUCH_DIRS	(SP_FIRST_ADM + 18)
#endif

struct sp_row {
    char           *sr_name;			/* name of the row */
    capability	    sr_typecap;			/* Guido's type capability */
    long	    sr_time;			/* Andy's time in seconds */
    capset	    sr_capset;			/* Robbert's capability set */
    rights_bits     sr_columns[SP_MAXCOLUMNS];	/* Robbert's right masks */
};
typedef struct sp_row sp_row;

struct sp_save {
    struct sp_row   ss_oldrow;	/* Old row, if doing delete, replace */
    short	    ss_rownum;	/* Affected row num, if doing delete/replace */
    command	    ss_command;	/* The type of modification */
    struct sp_save *ss_next;	/* Pointer to next in list */
#ifdef SOAP_DIR_SEQNR
    sp_seqno_t      ss_old_seq; /* previous sequence number */
#endif
};

struct sp_dir {
#ifdef SOAP_DIR_SEQNR
    sp_seqno_t       sd_seq_no;   /* sequence number of last update */
    short            sd_old_format; /* original dir file has old format */
#endif
    short            sd_ncolumns; /* # columns		   */
    short	     sd_nrows;	  /* # rows		   */
    port	     sd_random;	  /* random check number   */
    char	   **sd_colnames; /* names of the columns  */
    struct sp_row  **sd_rows;	  /* descr. of the rows    */
    struct sp_save  *sd_mods;	  /* pending modifications */
    struct sp_table *sd_update;	  /* next in update list   */

    /* WARNING: sd_rows points to a mallocd array of sp_row struct pointers.
     * The number of entries this array should have, is determined by
     * a call ``sp_rows_to_allocate(sd_nrows)''.
     */
};
typedef struct sp_dir sp_dir;

struct sp_table {
    struct sp_dir   *st_dir;	   /* the directory proper, or NULL */
    short	     st_flags;	   /* telling if it is out of cache, etc. */
    short	     st_time_left; /* for garbage collection (if used) */
};
typedef struct sp_table sp_tab;

#define SF_STATE	0x000f	/* least significant 4 bits are for state */
#define SF_FREE		0x0000	/* dir is not in use */
#define SF_IN_CORE	0x0001	/* dir is in core */
#define SF_CACHED_OUT	0x0002	/* dir is not in cache currently */

#define sp_in_use(st)		(((st)->st_flags & SF_STATE) != SF_FREE)

#define SF_FLAGS	0xfff0	/* most significant 12 bits are misc flags */
#define SF_DESTROYING	0x0010	/* we intend to destroy this dir */
#define SF_HAS_MODS	0x0020	/* dir has pending modifications */

#define	sp_begin	_sp_begin
#define	sp_end		_sp_end

void	sp_begin _ARGS((void));
void	sp_end _ARGS((void));

#endif /* __SOAPSVR_H__ */
