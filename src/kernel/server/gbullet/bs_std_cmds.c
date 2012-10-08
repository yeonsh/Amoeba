/*	@(#)bs_std_cmds.c	1.1	96/02/27 14:06:58 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BS_STD_CMDS.C
 *
 *	This file contains the routines for all the standard commands
 *	implemented by the bullet server
 *
 *  Author:
 *	Greg Sharp, Jan 1989
 *  Modified:
 *	Greg Sharp, Jan 1992 - finer locking granularity
 *  Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include <ctype.h>
#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"
#include "module/disk.h"
#include "module/strmisc.h"
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

extern	Superblk	Superblock;	/* in-core superblock */
extern	port		FilePutPort;	/* Put port for files */
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	Statistics	Stats;		/* list of statistics */
extern	b_fsize		Blksizemin1;	/* used by NUMBLKS */

extern	int	bs_print_msgs ;		/* print messages on stdout */
extern	int	bs_log_msgs ;		/* log messages on server */
extern	int	bs_ls_sick ;		/* set if log server is sick */

errstat
bs_std_age(hdr,peer)
header *	hdr;
int		peer;
{
    register errstat	err;

    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,peer)) == STD_OK)
	return gs_std_age(hdr) ;
    return err;
}

/*
 * REMOTE_SIZE
 *
 *	Gets the size of the remote file specified by cap.
 */

static errstat
remote_size(cap, size)
capability *	cap;
b_fsize *	size;
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = BS_SIZE;
    if ((n = trans(&hdr, NILBUF, (bufsize)0, &hdr, NILBUF, (bufsize)0)) != 0)
	return ERR_CONVERT(n);
    if (hdr.h_status != STD_OK)
	return ERR_CONVERT(hdr.h_status);
    *size = (b_fsize) hdr.h_offset;
    return STD_OK;
}


/*
 * REMOTE_READ
 *
 *	This routine reads the first "size" bytes of the file specified
 *	by "cap" into the buffer pointed to by "buf".  It does this via
 *	transactions with the file server managing "cap".
 */

errstat
bs_remote_read(cap, buf, size, offset)
capability *	cap;		/* capability for the file to be read */
bufptr		buf;		/* buffer to read the file into */
b_fsize		size;		/* size of file (and buffer) */
b_fsize		offset;		/* offset to start reading at */
{
    header      hdr;
    bufsize     n; 
     
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_offset = offset;
    size -= offset ;
    /* Until we get big transactions we put a loop around everything */
    do
    {
	hdr.h_command = BS_READ;
	hdr.h_size = size > BS_REQBUFSZ ? BS_REQBUFSZ : (short) size;
	n = trans(&hdr, NILBUF, 0, &hdr, buf+hdr.h_offset, hdr.h_size);
	if (ERR_STATUS(n))
	    return ERR_CONVERT(n);
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	hdr.h_offset += (short) n;
	size -= (short) n;
    } while (size > 0);
    return STD_OK;
}


/*
 * STD_COPY
 *
 *	This routine makes a copy of a file specified by the capability given
 *	in the parameter.  The file will normally be on another file server.
 *	It returns the capability for the newly created file.
 *	The value of the function is the error status of the create operation.
 *	The statistics differentiate between a local and remote copy to check
 *	that remote copies are indeed more frequent.
 */

errstat
bs_std_copy(hdr, buf, bufend)
header *	hdr;
bufptr		buf;
bufptr		bufend;
{
    capability	fcap;		/* capability of object to read */
    Cache_ent *	cp;		/* pointer to cache slot of new local file */
    Inodenum	dummy;		/* holds extra parameter for validate_cap */
    errstat	n;		/* holds return status of remote ops */
    b_fsize	size;		/* size of remote file */
    errstat	err;		/* Cache for return error value */

    /*
     * Must check that the server capability given is valid and has create
     * permission
     */
    n = bs_supercap(&hdr->h_priv, BS_RGT_CREATE,BS_PUBLIC);
    if (n == STD_CAPBAD)
	n = bs_supercap(&hdr->h_priv, BS_RGT_CREATE,BS_PEER);
    if (n == STD_CAPBAD)
	n = validate_cap(&hdr->h_priv, BS_RGT_CREATE, &dummy, L_NONE,BS_PEER);
    if (n != STD_OK)
	return n;

    /* Get the capability out of the buffer */
    if (buf_get_cap(buf, bufend, &fcap) == 0)
	return STD_ARGBAD;

    /* See if it is a local or a remote copy */
    if (PORTCMP(&fcap.cap_port, &FilePutPort))
    {
	Stats.std_localcopy++;
	n = make_cache_entry(&fcap.cap_priv, BS_RGT_READ, &cp, L_WRITE);
	if (n != STD_OK)
	    return n;
	if (cp->c_flags & NOTCOMMITTED)
	{
	    unlock_inode(cp->c_innum);
	    return STD_CAPBAD;
	}
	n = clone_file(&cp, cp->c_inode.i_size);
	/* Now cp points to the new uncommitted clone */
	if (n != STD_OK)
	{
	    dispose_file(cp);
	    return n;
	}
	assert(inode_locked_by_me(cp->c_innum) == 2);
    }
    else
    {
	Stats.std_remotecopy++;
	/* We still have no locks - get file size */
	n = remote_size(&fcap, &size);
	if (n != STD_OK)
	    return n;
	/* Allocate an inode and cache entry for the new file */
	err= bs_new_inode(&cp) ;
	if ( err!=STD_OK ) return err ;

	/* Allocate memory from buffer cache */
	if (alloc_cache_mem(cp, size) != STD_OK)
	{
	    n = STD_NOMEM;
	}
	else
	{
	    if ((cp->c_inode.i_size = size) > 0)
	    {
		/* Do a read transaction with remote bullet server */
		n = bs_remote_read(&fcap, cp->c_addr, size, (b_fsize)0);
	    }
	    else
		n = STD_OK;
	}
	if (n != STD_OK)
	{
	    dispose_file(cp);
	    return n;
	}
    }

    /* Create capability for new file */
    if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL,
						    &cp->c_inode.i_random) < 0)
    {
	dispose_file(cp);
	return STD_CAPBAD;
    }

    /*
     * Now allocate disk blocks and write the file to disk with safety.
     * The inode is also unlocked in the process.
     */
    hdr->h_port= FilePutPort ;
    return f_commit(cp, BS_SAFETY, &hdr->h_priv);
}


/*
 * STD_DESTROY
 *
 *	This removes a bullet server file completely and frees all the
 *	resources it was using.
 */

errstat
bs_std_destroy(hdr)
header *	hdr;
{
    errstat	err;		/* status of capability check */
    Inodenum	inode;		/* inode to destroy */

    Stats.std_destroy++;
    err = validate_cap(&hdr->h_priv, BS_RGT_DESTROY, &inode, L_WRITE, BS_PUBLIC);
    if (err == STD_OK)
	destroy_inode(inode,0,ANN_DELAY|ANN_ALL,SEND_CANWAIT);
    return err;
}


/*
 * STD_INFO
 *
 *	Returns standard string describing the specified bullet server object.
 */


#define	BS_INFOSTRLEN	32

void
bs_std_info(hdr, peer)
header *	hdr;
int		peer;
{
    char 	info_str[BS_INFOSTRLEN];
    char *	end;
    errstat	err;
    Inodenum	inode;
    Inode *	ip;
    b_fsize	size;

    Stats.std_info++;

    end = info_str + BS_INFOSTRLEN;

    err = validate_cap(&hdr->h_priv, BS_RGT_READ, &inode, L_READ, peer);
    switch (err)
    {
    case STD_OK:
	ip = Inode_table + inode;
	if ( !(ip->i_flags&I_INUSE) ) /* File is uncommitted */
	{
	    unlock_inode(inode);
	    hdr->h_size = 0;
	    err = STD_CAPBAD;
	}
	else
	{
	    /* Otherwise copy the info string and add the file size */
	    size = ip->i_size;
	    unlock_inode(inode);
	    DEC_FILE_SIZE_BE(&size);
	    end = bprintf(info_str, end, "- %8ld bytes", size);
	}
	break;
    
    case STD_DENIED:
	break;

    default:
	if ( prv_number(&hdr->h_priv)==0 ) {
	    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_READ,peer)) == STD_OK) {
	        if ( peer==BS_PEER ) {
		    end = bprintf(info_str, end, "bullet server member");
	        } else {
		    end = bprintf(info_str, end, "bullet group server");
	        }
	    }
	}
	break;
    }

    if ((hdr->h_status = err) != STD_OK) {
	if ( err==BULLET_LAST_ERR ) return ;
	hdr->h_size = 0;
    } else
    {
	hdr->h_size = end - info_str;
	*end = '\0';
    }
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, info_str, hdr->h_size);
#else
    putrep(hdr, info_str, hdr->h_size);
#endif /* USE_AM6_RPC */
}


/*
 * STD_RESTRICT
 *
 *	This function implements the STD_RESTRICT function for the
 *	bullet server.  It takes a capability and builds a new one
 *	with fewer rights.
 */

errstat
bs_std_restrict(priv, rights_mask, peer)
private *	priv;
rights_bits	rights_mask;
int		peer ;
{
    Inode *	ip;
    Inodenum	inode;
    rights_bits	rights;
    errstat	err;
    port	*super_random;

    Stats.std_restrict++;

    /*
     * We handle the restricting of the super capability separately since we
     * it has no inode.
     */
    if ( peer==BS_PEER ) {
	super_random= &Superblock.s_supercap.cap_priv.prv_random ;
    } else {
	super_random= &Superblock.s_membercap.cap_priv.prv_random ;
    }

    if (prv_number(priv) == 0) /* a super capability! */
    {
        if (prv_decode(priv, &rights, super_random) == 0 &&
	    prv_encode(priv, (objnum) 0, rights & rights_mask, super_random)
									  == 0)
	{
	    return STD_OK;
	}
	return STD_CAPBAD;
    }

    /*
     * We got a file capability.  Validate capability, get the current rights
     * field and make a restricted capability.
     */
    if ((err = validate_cap(priv, (rights_bits) 0, &inode, L_READ, BS_PUBLIC)) == STD_OK)
    {
	ip = Inode_table + inode;
	if (prv_decode(priv, &rights, &ip->i_random) != 0 ||
	    prv_encode(priv, inode, rights & rights_mask, &ip->i_random) != 0)
	{
	    err = STD_CAPBAD;
	}
	unlock_inode(inode);
    }
    return err;
}


/*
 * STD_TOUCH
 *
 *	This stops garbage collection of a file by resetting the lifetime
 *	of the file.  This is a side-effect of validate_cap().
 *	No rights are required to touch an object.
 */

errstat
bs_std_touch(priv)
private *	priv;
{
    Inodenum	inode;

    Stats.std_touch++;
    return validate_cap(priv, (rights_bits) 0, &inode, L_NONE, BS_PUBLIC);
}


/*
 * STD_NTOUCH
 *
 *	This stops garbage collection of a file by resetting the lifetime
 *	of the n files corresponding to the array of private parts.  This is
 *	a side-effect of validate_cap().
 *	No rights are required to touch an object.
 *	The routine assumes the array priv really points to sufficient data
 *	so that we don't stomp someone else's memory.
 */

errstat
bs_std_ntouch(n, priv, done)
bufsize		n;	/* # entries in priv array */
private *	priv;	/* array of caps to touch */
bufsize *	done;	/* returns the # successfull touches */
{
    Inodenum	inode;
    errstat	ret;
    errstat	err;
    bufsize	count;

    ret = STD_OK;
    count = 0;
    Stats.std_touch += n;
    if (n != 0)
    {
	do
	{
	    if (prv_number(priv) == 0)
		err= STD_OK; /* Skip the super cap */
	    else
		err = validate_cap(priv, (rights_bits) 0, &inode, L_NONE,BS_PUBLIC);
	    if (err != STD_OK)
		ret = err;
	    else
		count++;
	    priv++;
	} while (--n != 0);
    }
    *done = count;
    return ret;
}


/*
 * STD_SETPARAMS
 *
 */

errstat
bs_std_setparams(hdr,buf,bufend)
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
    int		newval ;

    Stats.std_setparams++;

    if ( prv_number(&hdr->h_priv)==0 ) {
	err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER);
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
#ifdef DEBUG_MSGS
		if ( strcmp(par_name,BS_DEBUG_LEVEL)==0 ) {
		    newval=strtol(par_val,&par_val,0) ;
		    /* Check for all digits */
		    if ( *par_val ) return STD_ARGBAD ;
		    bs_debug_level=newval ;
		} else
#endif
		if ( strcmp(par_name,BS_MSG_LOGGING)==0 ) {
		    if ( strcmp(par_val,"true")==0 ) {
			newval= 1 ;
		    } else if ( strcmp(par_val,"false")!=0 ) {
			return STD_ARGBAD ;
		    } else newval= 0 ;
		    /* Enable/Disable and reset logging */
		    bs_log_msgs= newval ; bs_ls_sick = 0 ;
		} else if ( strcmp(par_name,BS_MSG_PRINTING)==0 ) {
		    if ( strcmp(par_val,"true")==0 ) {
			newval= 1 ;
		    } else if ( strcmp(par_val,"false")!=0 ) {
			return STD_ARGBAD ;
		    } else newval= 0 ;
		    /* Enable and reset logging */
		    bs_print_msgs= newval ;
		} else {
		    return STD_ARGBAD ;
		}
	    }
	    if ( next!=bufend ) return STD_ARGBAD ;
	}
    } else {
	return STD_COMBAD ;
    }
    return STD_OK ;
}

/*
 * STD_GETPARAMS
 *
 */

void
bs_std_getparams(hdr)
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
	err = bs_supercap(&hdr->h_priv, BS_RGT_READ,BS_PEER);
	if (err != STD_OK)
	{
	    hdr->h_status = err;
	    hdr->h_size = 0;
	} else {
	    hdr->h_size = 2;
	    buf= Buf ;
	    end= Buf + BS_STATUS_SIZE -1 ; /* One less, needs trailing zero */
#ifdef DEBUG_MSGS
	    /* debug level */
	    hdr->h_size++ ;
	    buf= bprintf(buf,end,BS_DEBUG_LEVEL) ;
	    buf= bprintf(buf+1,end,"natural number");
	    buf= bprintf(buf+1,end,"debug level");
	    buf= bprintf(buf+1,end,"%d",bs_debug_level) ;
	    buf++ ;
#endif
	    /* Message destinations */
	    buf= bprintf(buf,end,BS_MSG_LOGGING) ;
	    buf= bprintf(buf+1,end,"flag");
	    buf= bprintf(buf+1,end,"send messages to log server");
	    buf= bprintf(buf+1,end,"%s",(bs_log_msgs?"true":"false")) ;
	    buf= bprintf(buf+1,end,BS_MSG_PRINTING) ;
	    buf= bprintf(buf+1,end,"flag");
	    buf= bprintf(buf+1,end,"send messages to stderr");
	    buf= bprintf(buf+1,end,"%s",(bs_print_msgs?"true":"false")) ;
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
