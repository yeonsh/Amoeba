/*	@(#)sp_adm_cmd.c	1.1	96/02/27 10:02:57 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "thread.h"
#include "monitor.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "bullet/bullet.h"

#include "module/stdcmd.h"
#include "module/mutex.h"
#include "module/prv.h"
#include "module/tod.h"
#include "module/buffers.h"
#include "module/ar.h"

#include "global.h"
#include "caching.h"
#include "dirfile.h"
#include "filesvr.h"
#include "sp_impl.h"
#include "sp_print.h"
#include "sp_adm_cmd.h"
#include "seqno.h"
#include "modlist.h"
#include "dirmarshall.h"
#include "superfile.h"
#include "misc.h"
#include "main.h"
#include "timer.h"
#include "dirsvr.h"
#include "recover.h"

#define STACKSIZE	(2 * SP_BUFSIZE + 4096 + 4096)

#define GRPSTATUS_SIZE  512 		/* big enough to hold group status */
#define NTASKS		1		/* BUG: should go somewhere else */

/* For debugging: warn about directories that are going to be aged soon */
#define DANGEROUS_AGE	8

extern	port		my_super_getport;
extern  capability	my_supercap;

/*
 * Load status information into a char buffer:
 */
static char *
buf_put_grpstatus(buf, end)
char *buf, *end;
{
    sp_seqno_t global_seq;
    char *p;

    if (buf == NULL || (end - buf) < GRPSTATUS_SIZE) {
	return buf;
    }

    p = buf;
    (void)sprintf(p, "\nServer uptime: %ld (min); mytime: %ld; grptime: %ld\n",
		  sp_mytime() - sp_time_start(), sp_mytime(), sp_time());
    p += strlen(p);

    get_global_seqno(&global_seq);
    (void)sprintf(p, "global seqno: [%ld, %ld], group seqno: %ld\n", 
		  hseq(&global_seq), lseq(&global_seq), get_group_seqno());
    p += strlen(p);

    (void)sprintf(p, "\n#members %d; minimum #members %d; sp_seqno %ld\n",
                  get_total_member(), get_sp_min_copies(), get_seq_nr());
    p += strlen(p);

    (void)sprintf(p, "#read operations: %6d;  #write operations: %d\n", 
		  get_read_op(), get_write_op());
    p += strlen(p);

    if (ml_in_use()) {
	(void)sprintf(p, "Modlist: #mods: %6d; clean: %6d (msec)\n",
		      ml_nmods(), ml_time());
	p += strlen(p);
    }

    return p + 1;
}


/*
 * Return character buffer with status information:
 */
static int
sp_std_status(hdr, outbuf, outsize)
header *hdr;
char *outbuf;
int outsize;
{
    char *p;

    MON_EVENT("std_status");
    sp_begin();
    p = buf_put_status(outbuf, &outbuf[outsize]);
    p = buf_put_grpstatus(p, &outbuf[outsize]);
    sp_end();

    if (p == NULL) {
        hdr->h_status = STD_NOSPACE;
        return 0;
    }

    hdr->h_status = STD_OK;
    /* The client stub doesn't want the NULL byte, so: */
    return hdr->h_size = (p - outbuf) - 1;
}

errstat
remove_dir(obj)
objnum obj;
{
    capability newcaps[NBULLETS];
    capability curcaps[NBULLETS];
    int        i;

    assert(sp_in_use(&_sp_table[obj]));

    get_dir_caps(obj, curcaps);

    /* Watch out: if we inherited an old-style superfile with files shared
     * between Soap0 and Soap1, we should not destroy the previous versions.
     * TODO: actually we *can* let one server destroy the cap having its
     * own index, which is unique.
     */
    if (!NULLPORT(&curcaps[0].cap_port) && !NULLPORT(&curcaps[1].cap_port)) {
	return STD_OK;
    }

    for (i = 0; i < NBULLETS; i++) {
	if (!NULLPORT(&curcaps[i].cap_port)) {
	    destroy_file(&curcaps[i]);
	}
    }
	    
    cache_destroy(obj);
    sp_add_to_freelist(&_sp_table[obj]);

    /* update super table */
    clear_caps(newcaps, NBULLETS);
    set_dir_caps(obj, newcaps);
    write_blocks_modified();
    /* update mod list */
    ml_remove(obj);

    return STD_OK;
}

void
age_directory(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    struct sp_table *st;
    objnum obj;

    /* Only execute it when the seqno mentioned in the request matches the
     * current seqno.  Only in that case we are really sure that the age
     * command was for the current directory version.
     */
    obj = hdr->h_offset;
    if (bad_objnum(obj)) {
	scream("got SP_DEL_DIR cmd for bad directory %ld", (long) obj);
	return;
    }

    sp_begin();

    st = &_sp_table[obj];
    if (!sp_in_use(st)) {
	scream("got SP_DEL_DIR for non-existant directory %ld", (long) obj);
    } else {
	errstat err;
	
	err = sp_refresh(obj, SP_LOOKUP);
	if (err == STD_OK) {
	    char      *p;
	    sp_seqno_t delseq;

	    p = buf_get_seqno(buf, buf + size, &delseq);
	    assert(p != NULL);

	    if (eq_seq_no(&delseq, &st->st_dir->sd_seq_no)) {
		message("aging out directory %ld with seqno [%ld, %ld]",
			(long) obj, hseq(&delseq), lseq(&delseq));
		remove_dir(obj);
	    } else {
		scream("not aging out %ld: seq is [%ld, %ld] iso [%ld, %ld]");
	    }
	} else {
	    scream("cannot refresh dir %ld; aging it anyway", (long) obj);
	    remove_dir(obj);
	}
    }

    sp_end();
}

#define TOUCH_BUF_SIZE	8000
static char  Touch_buf[TOUCH_BUF_SIZE];
static char *Touch_tail = NULL;

static void
init_touch_list()
{
    Touch_tail = Touch_buf;
}

static void
flush_touch_list()
{
    /* Either the touch list is full or we are ready.
     * If the buffer is non-empty, send it to the group.
     */
    header  hdr;
    errstat err;

    if (Touch_tail != Touch_buf) {
	hdr.h_command = SP_TOUCH_DIRS;
	hdr.h_extra = sp_whoami();
	hdr.h_size = (Touch_tail - Touch_buf) / sizeof(int32);
	err = send_to_group(&hdr, Touch_buf, Touch_tail - Touch_buf);

	if (err != STD_OK) {
	    scream("could not broadcast SP_TOUCH_DIRS (%s)", err_why(err));
	    /* retry? */
	}

	init_touch_list();
    }
}

static void
add_to_touch_list(obj)
objnum obj;
{
    char *end;

    /* When we come here make sure there is enough room left for at least
     * one extra object number.
     */
    assert(Touch_tail != NULL);
    end = &Touch_buf[sizeof(Touch_buf)];
    if ((end - Touch_tail) <= sizeof(int32)) {
	flush_touch_list();
    }

    Touch_tail = buf_put_objnum(Touch_tail, end, obj);
    assert(Touch_tail != NULL);
}

void
got_touch_list(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    char  *p, *end;

    if (hdr->h_extra == sp_whoami()) {
	/* sent it ourselves; we can safely ignore this */
	return;
    }

    message("got touch list from Soap %d: %d entries",
	    hdr->h_extra, hdr->h_size);

    sp_begin();

    p = buf;
    end = &buf[size];
    while (p != NULL && p < end) {
	objnum obj;

	if ((p = buf_get_objnum(p, end, &obj)) != NULL) {
	    if (bad_objnum(obj)) {
		scream("SP_TOUCH_DIRS: bad directory %ld", (long) obj);
	    } else if (!sp_in_use(&_sp_table[obj])) {
		scream("SP_TOUCH_DIRS: unused directory %ld", (long) obj);
	    } else {
		int ttl;
	
		ttl = get_time_to_live(obj);
		if (ttl < SP_MAX_TIME2LIVE - 1) {
		    message("correct time-to-live of dir %ld", (long) obj);
		    set_time_to_live(obj, SP_MAX_TIME2LIVE - 1);
		    /* Note: not SP_MAX_TIME2LIVE, because then we would
		     * include this dir in the next SP_TOUCH_DIRS broadcast
		     * ourselves.
		     */
		}
	    }
	}
    }

    sp_end();
}

void
sp_age(hdr)
header *hdr;
{
    struct sp_table *st;
    objnum     obj;
    errstat    err;

    message("got SP_AGE from Soap %d", hdr->h_extra);

    sp_begin();
    
    /* only age directories when we are fully recovered and functioning! */
    if (!dirsvr_functioning()) {
	scream("server not functional; ignore SP_AGE");
	sp_end();
	return;
    }

    for (obj = 1; obj < _sp_entries; obj++) {
	st = &_sp_table[obj];

	if (sp_in_use(st)) {
	    int ttl;

	    if ((ttl = get_time_to_live(obj)) > 0) {
		/* decrement time-to-live attribute for this dir */
		set_time_to_live(obj, ttl - 1);
	    }
	    if (ttl < DANGEROUS_AGE) {
		message("sp_age: dir %ld now has age %d", obj, ttl-1);
	    }
	}
    }

    sp_end();

    init_touch_list();

    for (obj = 1; obj < _sp_entries; obj++) {
	st = &_sp_table[obj];

	sp_begin();

	if (sp_in_use(st)) {
	    int ttl = get_time_to_live(obj);

	    if (ttl <= 0 && has_lowest_id(sp_whoami()) &&
		(sp_refresh(obj, SP_LOOKUP) == STD_OK))
	    {
		char   buf[sizeof(sp_seqno_t)];
		char  *p;
		header hdr;

		message("sp_age: age out directory %ld", (long) obj);

		set_time_to_live(obj, 0); /* was reset by sp_refresh */

		/* Send a SP_DEL_DIR to the group, so that all living members
		 * delete this directory consistently.
		 * When it is received back by the group thread, age_dir() 
		 * will take care of the actual removal of the directory.
		 * For extra safety we also add the current sequence number
		 * to the request. 
		 */
		p = buf_put_seqno(buf, buf + sizeof(buf),
				  &st->st_dir->sd_seq_no);
		assert(p != NULL);

		hdr.h_command = SP_DEL_DIR;
		hdr.h_offset = obj;
		err = send_to_group(&hdr, buf, p - buf);
		if (err != STD_OK) {
		    scream("couldn't broadcast SP_DEL_DIR (%s)", err_why(err));
		    /* ignore this; maybe a member left */
		}
	    } else if (ttl == SP_MAX_TIME2LIVE - 1) {
		/* This was a fresh directory before we aged it.
		 * Maybe it was fresh because a read operation was performed
		 * on it, and then the other members may not know about it.
		 * Tell them.
		 */
		add_to_touch_list(obj);
	    }
	}

	sp_end();
    }

    flush_touch_list();
}

static errstat
sp_std_age()
{
    /* resend the STD_AGE as SP_AGE to the entire group */
    header newhdr;

    newhdr.h_command = SP_AGE;
    newhdr.h_extra = sp_whoami();
    return send_to_group(&newhdr, NILBUF, 0);
}

static void
sp_adm_cmd(hdr, inbuf, insize, outbuf, outsize)
header *hdr;
char   *inbuf;
int     insize;
char   *outbuf;
int     outsize;
/*ARGSUSED*/
{
    int size = 0;

    /* TODO: SP_NEWBULLET support.
     * It probably requires help from (part of) the original watchdog code.
     */
    switch (hdr->h_command) {
    case STD_AGE:  
	MON_EVENT("std_age");
	hdr->h_status = sp_std_age();
	break;
    case STD_TOUCH:
	MON_EVENT("std_flush");
	sp_begin();
	hdr->h_status = ml_flush();
	sp_end();
	break;
    case STD_INFO:
	MON_EVENT("std_info");
	/* temporary hack: */
	sp_begin();
	ml_print();
	sp_end();
	(void) strcpy(outbuf, "soap generic capability");
	size = strlen(outbuf);
	hdr->h_size = size;
	hdr->h_status = STD_OK;
	break;
    case STD_STATUS:
#ifdef PROFILING
	start_printing();
	prof_dump(stdout);
	stop_printing();
#else
#ifdef PRINT_TABLE
	sp_begin();
	sp_print_table();
	sp_end();
#endif
#endif
	size = sp_std_status(hdr, outbuf, outsize);
	hdr->h_status = STD_OK;
	break;
    default:
	hdr->h_status = STD_COMBAD;
	break;
    }

    putrep(hdr, outbuf, (bufsize) size);
}


static void
sp_adm_server(param, psize)
char *param;
int psize;
/*ARGSUSED*/
{
    bufsize n;
    port    putport;
    header  req_hdr;
    char    req_buf[SP_BUFSIZE];
    char    rep_buf[SP_BUFSIZE];

    /* sanity check on the get-port of the administrative thread: */
    if (NULLPORT(&my_super_getport)) {
	panic("admin getport is null!");
    }

    message("sp_adm_server listening to %s", ar_port(&my_super_getport));
    priv2pub(&my_super_getport, &putport);
    message("corresponding put port is %s", ar_port(&putport));
    
    for (;;) {
	req_hdr.h_port = my_super_getport;
	n = getreq(&req_hdr, req_buf, sizeof(req_buf));

	if (ERR_STATUS(n)) {
	    scream("getreq failed %s\n", err_why((errstat) ERR_CONVERT(n)));
	} else {
	    /* check that it is an owner capability: */
	    if (!PORTCMP(&req_hdr.h_priv.prv_random,
			 &my_supercap.cap_priv.prv_random))
	    {
		req_hdr.h_status = STD_CAPBAD;
		putrep(&req_hdr, NILBUF, (bufsize) 0);
	    } else {
		sp_adm_cmd(&req_hdr, req_buf, (int) n, rep_buf, SP_BUFSIZE);
	    }
	}
    }
}

void
start_adm_server()
{
    if (!thread_newthread(sp_adm_server, STACKSIZE, (char *) NULL, 0)) {
	panic("cannot sp_adm_server thread");
    }
}

