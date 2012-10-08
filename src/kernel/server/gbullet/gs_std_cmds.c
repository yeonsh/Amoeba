/*	@(#)gs_std_cmds.c	1.1	96/02/27 14:07:25 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * MS_STD_CMDS.C
 *
 *	This file contains the routines for all the standard commands
 *	implemented by bullet member servers
 *
 *  Author:
 *	Ed Keizer
 */

#include <ctype.h>
#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"
#include "module/ar.h"
#include "module/strmisc.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "stats.h"
#include "bs_protos.h"
#include "assert.h"
#include "grp_defs.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "module/buffers.h"
#include "module/ar.h"

extern	Superblk	Superblock;	/* in-core superblock */
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	Statistics	Stats;		/* list of statistics */
extern	b_fsize		Blksizemin1;	/* used by NUMBLKS */



/*
 * STD_SETPARAMS
 *
 */

errstat
gs_std_setparams(hdr,buf,bufend)
header *	hdr;
bufptr		buf;
bufptr		bufend;
{
    errstat	err;		/* holds return status of remote ops */
    int		nparams;	/* # of params in buffer */
    char	*next;		/* start of next param descr. */
    char	*par_name;	/* name of param */
    char	*par_val;	/* value of param */
    char	*tmp_cp;

    int		newrep=0;	/* set rep factor */
    int		newrepval;	/* new value */

    int		newflag=0;	/* bits changed in flags */
    int		newflagval=0;	/* their values */

    int		newlog=0;	/* set log server */
    capability	newlogval;	/* new value */

    Stats.std_setparams++;

    if ( prv_number(&hdr->h_priv)==0 ) {
	err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PUBLIC);
	if (err != STD_OK)
	{
	    return err;
	} else {
	    nparams= hdr->h_size;
	    next=buf ;
	    while ( nparams-- ) {
		if ( next>=bufend ) return STD_ARGBAD ;
		par_name= next ;
		while ( next < bufend && *next!=0 ) next++ ;
		if ( next>=bufend ) return STD_ARGBAD ;
		par_val= ++next ;
		while ( next < bufend && *next!=0 ) next++ ;
		if ( next++>=bufend ) return STD_ARGBAD ;
		if ( strcmp(par_name,BS_REP_FACTOR)==0 ) {
		    if ( newrep ) return STD_ARGBAD ;
		    newrep=1 ;
		    newrepval=strtol(par_val,&par_val,0) ;
		    if ( newrepval<1 || newrepval>Superblock.s_maxmember ) {
			return STD_ARGBAD ;
		    }
		    /* Check for all digits */
		    if ( *par_val ) return STD_ARGBAD ;
		} else if ( strcmp(par_name,BS_SAFETY_REPL)==0 ) {
		    if ( newflag&S_SAFE_REP ) return STD_ARGBAD ;
		    newflag |= S_SAFE_REP ;
		    if ( strcmp(par_val,"true")==0 ) {
			newflagval|= S_SAFE_REP ;
		    } else if ( strcmp(par_val,"false")!=0 ) {
			return STD_ARGBAD ;
		    }
		} else if ( strcmp(par_name,BS_LOG_SERVER)==0 ) {
		    if ( newlog ) return STD_ARGBAD ;
		    newlog= 1 ;
		    tmp_cp=ar_tocap(par_val,&newlogval) ;
		    if ( *tmp_cp ) return STD_ARGBAD ;
		} else {
		    return STD_ARGBAD ;
		}
	    }
	    if ( next!=bufend ) return STD_ARGBAD ;
	}
    } else {
	return STD_COMBAD ;
    }
    if ( !newrep && !newflag && !newlog ) return STD_OK ;
    return gs_chsuper(newrep,newrepval,newflag,newflagval,newlog,&newlogval) ;
}

/*
 * STD_GETPARAMS
 *
 */

void
gs_std_getparams(hdr)
header *	hdr;
{
    errstat	err;		/* holds return status of remote ops */
    char	Buf[BS_STATUS_SIZE];
    char *	buf;
    char *	end;
    int		repl_factor ;
    int		flags ;
    capability	log_server; 

    Stats.std_getparams++;
    if ( prv_number(&hdr->h_priv)==0 ) {
	err = bs_supercap(&hdr->h_priv, BS_RGT_READ,BS_PUBLIC);
	if (err != STD_OK)
	{
	    hdr->h_status = err;
	    hdr->h_size = 0;
	} else {
	    hdr->h_size = 2;
	    buf= Buf ;
	    end= Buf + BS_STATUS_SIZE -1 ; /* One less, needs trailing zero */
	    super_lock() ;
	    repl_factor= Superblock.s_def_repl ;
	    flags= Superblock.s_flags ;
	    log_server= Superblock.s_logcap ;
	    super_free() ;
	    /* Default replication */
	    buf= bprintf(buf,end,BS_REP_FACTOR) ;
	    buf= bprintf(buf+1,end,"natural number");
	    buf= bprintf(buf+1,end,"default replication factor");
	    buf= bprintf(buf+1,end,"%d",repl_factor) ;
	    /* Replicate with safety */
	    buf= bprintf(buf+1,end,BS_SAFETY_REPL) ;
	    buf= bprintf(buf+1,end,"flag");
	    buf= bprintf(buf+1,end,"need replicas when safety is used");
	    buf= bprintf(buf+1,end,"%s",(flags&S_SAFE_REP?"true":"false")) ;
	    err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PUBLIC);
	    if ( err==STD_OK ) {
		/* log capability */
		buf= bprintf(buf+1,end,BS_LOG_SERVER) ;
		buf= bprintf(buf+1,end,"capability");
		buf= bprintf(buf+1,end,"log server identification");
		buf= bprintf(buf+1,end,"%s",ar_cap(&log_server)) ;
		hdr->h_size++ ;
	    }
	    /* Finish transaction */
	    hdr->h_status = STD_OK;
	    hdr->h_extra = buf +1 - Buf ;
	}
    } else {
	hdr->h_size = 0;
	hdr->h_status = STD_COMBAD ;
	hdr->h_extra = 0;
    }
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, Buf, hdr->h_extra);
#else
    putrep(hdr, Buf, hdr->h_extra);
#endif /* USE_AM6_RPC */
    return;
}
