/*	@(#)main.c	1.7	96/02/27 10:22:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "thread.h"
#include "monitor.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "bullet/bullet.h"
#include "assert.h"
#include "module/cap.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef AMOEBA
#include <signal.h>
#endif

#include "module/stdcmd.h"
#include "module/mutex.h"
#include "module/prv.h"
#include "module/tod.h"
#include "module/buffers.h"

#include "global.h"
#include "caching.h"
#include "watchdog.h"
#include "dirfile.h"
#include "filesvr.h"
#include "intentions.h"
#include "expire.h"
#include "sp_impl.h"
#include "superfile.h"
#include "timer.h"
#include "diag.h"
#include "main.h"

#define NCAPCACHE	256

/* stack size of various threads: */
#define TIMERSIZE	(4096 + 4096)
#define STACKSIZE	(2*SP_BUFSIZE + 4096 + 4096)
#define INITSERVERSIZE	STACKSIZE
#define DEF_THREADS	6	 /* #thread handling generic soap requests */
#define MAX_THREADS	20

static int N_threads = DEF_THREADS;
static int Startup_Fsck = 1;	   /* set to 0 when quick startup is desired */

/* memory that may be allocated must be within reasonable limits: */
#define MIN_KBYTES	100
#define MAX_KBYTES	64000
#define DEF_MAX_KBYTES	500

capability        _sp_generic;		/* with put port for user requests */
port 		  _sp_getport;		/* corresponding getport */

static capability my_supercap;		/* with put port for admin request */
static port	  my_super_getport;	/* corresponding getport */

static capability other_supercap; 	/* admin capability of other Soap */

static mutex Init_mutex;
static int   Initialised = 0;
static int   Help_needed = 0;

/* Default strategy to determine if the other side has gone forever:
 * after 5 minutes of RPC_NOTFOUNDs, ask the system administrator what's up.
 * Depending on the answer, switch to 1 copy mode, or keep on waiting.
 * Alternatively, Soap can be instructed to automatically switch to 1 copy
 * mode after a certain period.
 */
#define DEF_MANUAL_TIMEOUT	5	/* ask after 5 minutes */
#define DEF_AUTO_TIMEOUT	0	/* don't do it automatically */

static int   N_ticks = 0;
static int   Manual_timeout = DEF_MANUAL_TIMEOUT;
static int   Auto_timeout = DEF_AUTO_TIMEOUT;
static int   Forced_startup = 0;

/*
 * Capabilities used by the Soap stubs library:
 */
capset _sp_rootdir;
capset _sp_workdir;  /* not used but needed to silence the loader */

/*
 * Most global/static variables may only be accessed after the mutex has been
 * acquired!
 */
static mutex sp_mutex;

/*
 * Forward declarations of functions in this file:
 */
void sp_begin _ARGS((void));
static void start_up _ARGS((void));

#ifdef __STDC__
errstat
to_other_side(int ntries, header *inhdr,  char *inbuf,  bufsize insize,
	      header *outhdr, char *outbuf, bufsize outsize, bufsize *retsize)
#else
errstat
to_other_side(ntries, inhdr, inbuf, insize, outhdr, outbuf, outsize, retsize)
int      ntries;
header  *inhdr;
char    *inbuf;
bufsize  insize;
header  *outhdr;
char    *outbuf;
bufsize  outsize;
bufsize *retsize;
#endif
{
    errstat err;
    int     try;
    bufsize n;
    header  outheader;
    /* local copy to handle the case that we retry and inhdr == outhdr */

    inhdr->h_port = other_supercap.cap_port;
    inhdr->h_priv = other_supercap.cap_priv;
    
    for (try = 1; try <= ntries; try++) {
	n = trans(inhdr, inbuf, insize, &outheader, outbuf, outsize);

	if (ERR_STATUS(n)) {
	    err = ERR_CONVERT(n);
	    if (err == RPC_NOTFOUND) {
		/* no need to try further */
		break;
	    }
	} else {
	    *retsize = n;
	    err = ERR_CONVERT(outheader.h_status);

	    if (err == STD_NOTNOW && try < ntries) {
		message("to_other_side(cmd=%d): other side busy; retry",
			inhdr->h_command);
		(void) sleep(1);
	    } else {
		break;
	    }
	}
    }

    *outhdr = outheader;
    return err;
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
    
    if ((p = buf_put_status(outbuf, &outbuf[outsize])) == NULL) {
	hdr->h_status = STD_NOSPACE;
	sp_end();
	return 0;
    }
    sp_end();
    hdr->h_status = STD_OK;
    /* The client stub doesn't want the NULL byte, so: */
    return hdr->h_size = (p - outbuf) - 1;
}


/* There's only one critical section defined in this server.  For pre-
 * emptively scheduled threads this is probably insufficient.  Note that
 * sp_begin() and sp_end() do not nest (calling sp_begin twice
 * causes eternal blocking).  Sp_try is the same as sp_begin, except
 * that it returns an error instead of blocks if the critical region is
 * occupied already.
 *
 * Before entering the critical region intent_rollforward() makes sure that
 * everything is up-to-date.
 */

#ifdef AMOEBA
errstat
sp_init()
{
    return STD_OK;
}
#endif

void
sp_begin()
{
    mu_lock(&sp_mutex);
    intent_check(0);
}

int
sp_try()
{
    if (mu_trylock(&sp_mutex, 0L) < 0) {
	return 0;
    }
    intent_check(0);
    return 1;
}

void
sp_end()
{
    mu_unlock(&sp_mutex);
}

/* A request arrived at the control thread.  Check the capability.  If this
 * is correct try to enter the critical region.  Don't enter if someone
 * else is there already, because it might cause deadly embrace.
 */
int
for_me(hdr)
header *hdr;
{
    capability cap;
    
    cap.cap_port = hdr->h_port;
    cap.cap_priv = hdr->h_priv;

#ifdef AMOEBA
    if (!cap_cmp(&cap, &my_supercap)) {
	MON_EVENT("not a valid super cap");
	hdr->h_status = STD_CAPBAD;
	return 0;
    }
#else
    {
	/* We don't have an extra administrative thread in this case,
	 * so we'll have to do something different here:
	 */
	rights_bits r;
	port *supercheck = &my_supercap.cap_priv.prv_random;

	if (prv_decode(&cap.cap_priv, &r, supercheck) != 0 || r != 0xff) {
	    MON_EVENT("not a valid super cap");
	    hdr->h_status = STD_CAPBAD;
	    return 0;
	}
    }
#endif

    if (!sp_try()) {
	MON_EVENT("avoiding deadly embrace");
	hdr->h_status = STD_NOTNOW;
	return 0;
    }
    return 1;
}

/*
 * Send our sequence number to the other server.
 * This is done during recovery of myself or of the other server.
 * The other server replies with its sequence number.
 * The caller uses that to decide who is the master in the recovery procedure.
 */
static errstat
sp_sendseq(ntries, other_seq)
int   ntries;
long *other_seq;
{
    header  hdr;
    bufsize retsize;
    long    my_seq;
    errstat err;
    
    MON_EVENT("send sequence number");
    my_seq = get_seq_nr();
    hdr.h_command = SP_SEQ;
    hdr.h_seq = my_seq;
    err = to_other_side(ntries, &hdr, NILBUF, 0, &hdr, NILBUF, 0, &retsize);

    if (err == STD_OK) {
	*other_seq = hdr.h_seq;
    }

    return err;
}

static void
help_other_side()
{
    static int helping_other_side = 0;
    long    other_seq;
    errstat err;

    if (helping_other_side) {
	message("already helping other side");
	return;
    }
	
    helping_other_side = 1;
    
    if ((err = copy_super_blocks()) == STD_OK) {
	/* Now send over my sequence number,  causing the other side
	 * to initialise.
	 */
	err = sp_sendseq(3, &other_seq);

	if (err == STD_OK) {
	    if (other_seq != get_seq_nr()) {
		panic("protocol bug: seq_nrs not equal after recovery");
	    }
	    message("other side is back");
	} else {
	    scream("could not send seq_nr after copying superfile (%s)",
		   err_why(err));
	}
    } else {
	scream("failed to copy superfile (%s)", err_why(err));
    }

    helping_other_side = 0;
}

static void
do_start_up(mode, msg)
int mode;
char *msg;
{
    message(msg);
    if (get_copymode() != mode) {
	message("switching to %d copy mode", mode);
	set_copymode(mode);
    }
    start_up();
}

static int
other_side_gone_forever()
{
    char answer[4];
    int i;

    for (i = 0; i < 3; i++) {
	scream("Other side not responding after %d minutes", Manual_timeout);
	printf("SOAP %d: Can I safely switch to 1-copy mode [n/y]? ",
	       sp_whoami());

	answer[0] = '\0';
	(void) fgets(answer, sizeof(answer), stdin);
	if (answer[0] == 'y' || answer[0] == 'Y') {
	    return 1;
	} else if (answer[0] == 'n' || answer[0] == 'N') {
	    return 0;
	} else {
	    message("Answer with 'n' or 'y' please.");
	}
    }

    /* don't know; assume our partner will come back */
    return 0;
}

static void
try_to_recover()
{
    errstat err;

    if (get_copymode() == 1) {
	/* If we crashed while in one-copy mode, we don't need help */
	do_start_up(1, "coming up in 1-copy mode");
    } else {
	/* We crashed while in two-copy mode.  We need to contact the other
	 * side to see which of us has the most up-to-date superfile (this
	 * is determined by the sequence number in super block 0).
	 */
	long my_seq, other_seq;

	my_seq = get_seq_nr();

	/* If Help_needed is already set, it could be that the other side
	 * is still busy sending us the super blocks.  So in that case
	 * only try to send over the sequence number once.
	 */
	err = sp_sendseq((Help_needed ? 1 : 5), &other_seq);

	if (err == STD_OK) {
	    if (other_seq < my_seq) {
		/* This means I have the most up-to-date superfile.
		 * In the current protocol, when both servers are in
		 * two copy mode, their sequence numbers cannot differ
		 * by more than 1.  We check for this.
		 */
		if (other_seq < my_seq - 1) {
		    panic("protocol bug: other side missed %ld updates",
			  my_seq - other_seq);
		} else {
		    do_start_up(2, "I have the most up-to-date superfile");
		    help_other_side();
		}
	    } else if (other_seq == my_seq) {
		do_start_up(2, "same sequence number; no recovery required");
	    } else {
		/* Need help from other side, having a more up-to-date
		 * super file.  We just wait for its SP_COPY requests.
		 */
		Help_needed = 1;
		message("need help from the other side");
	    }
	} else {
	    /* This is not necessarily serious; maybe the other side
	     * has also crashed, in which case we just have to wait.
	     * Or it just may be handling user requests in one-copy mode.
	     * We will try to recover again in the next iteration of 
	     * the watchdog.
	     */
	    message("could not get my sequence nr to the other side (%s)",
		    err_why(err));

	    /* Maybe the machine storing directory file replicas has gone,
	     * so do a complete fsck to see if directories have become
	     * unavailable.
	     */
	    if (!Startup_Fsck) {
		message("ignoring -n flag");
		Startup_Fsck = 1;
	    }

	    if (Forced_startup) {
		do_start_up(1, "!! copy mode: command line override !!");
	    } else {
		/* The watchdog will try again within 60 seconds.
		 * But we want to deal with the possibility that the other
		 * side has gone forever, so we need some escape mechanism.
		 * Requiring manual intervention is safest, so that's what
		 * we implement per default.  Alternatively an automatic
		 * strategy based on a timeout is possible, but that has the
		 * problem that a network partition longer than timeout
		 * specified may lead to the wrong decision.
		 */
		if (err == RPC_NOTFOUND) {
		    static int did_ask = 0;

		    if (!did_ask &&
			(Manual_timeout > 0 && N_ticks >= Manual_timeout)) {
			if (other_side_gone_forever()) {
			    do_start_up(1, "!! copy mode: manual override !!");
			}
			did_ask = 1;
		    } else if (Auto_timeout > 0 && N_ticks >= Auto_timeout) {
			do_start_up(1, "!! copy mode: automatic override !!");
		    }
		}
	    }
	}
    }
}

/*
 * Received a sequence number from the other server.  If in one-copy-mode,
 * and the sequence number is current (that is, identical to mine), I
 * switch back to two-copy-mode.  If I'm recovering and the sequence number
 * is lower or equal to mine, I know now that I'm up-to-date and can complete
 * the recovery by loading the directories.  If the sequence number is lower
 * than mine, this means that the other server needs to be recovered.  I can
 * help by sending my super file across.
 */
static int
sp_gotseq(hdr)
header *hdr;
{
    long other_seq = hdr->h_seq;
    long my_seq = get_seq_nr();
    
    if (!for_me(hdr)) {
	return 0;
    }
    
    if (get_copymode() == 1) {
	MON_EVENT("got seq number in 1-copy mode");
	if (other_seq > my_seq) {
	    /* This would mean that the Soap coming up has performed
	     * updates without me knowing it!
	     */
	    panic("protocol bug: (1-copy mode) other side made %ld updates",
		  other_seq - my_seq);
	}

	if (other_seq == my_seq) {
	    MON_EVENT("switching to 2 copy mode");
	    message("other side is back");
	    set_copymode(2);
	    flush_copymode();
	}
    } else {
	MON_EVENT("got seq number in 2-copy mode");

	/* The other Soap is also up, and it has crashed in two-copy mode
	 * (that's why it is sending its seq_nr).  But the fact that *I*
	 * am also in 2 copy mode means that our sequence numbers should
	 * not differ by more than 1 (otherwise one of us would be in
	 * one copy mode).
	 */
	if (other_seq < my_seq - 1) {
	    scream("protocol bug: (2-copy mode) seq_nrs differ by %ld",
		   my_seq - other_seq);
	}

	if (fully_initialised()) {
	    if (other_seq > my_seq) {
		/* This would mean that I recovered without the help
		 * of the other side, when it was really illegal.
		 */
		panic("protocol bug: (2-copy mode) other side is master");
	    }
	} else {
	    if (other_seq <= my_seq) {
		if (Help_needed) {
		    do_start_up(2, "initialize after help from other side");
		} else {
		    do_start_up(2, "initialize without help from other side");
		}
	    }
	}
    }

    /* we'll send our sequence number back */
    hdr->h_seq = my_seq;
    hdr->h_status = STD_OK;

    if (other_seq < my_seq) {
	/* Other side needs help for recovery.
	 * Its super blocks may not be completely up-to-date.
	 */
	putrep(hdr, NILBUF, 0);	/* acknowledge first to avoid deadlock */
	help_other_side();

	sp_end();
	return -1;	/* suppress putrep--done already */
    }

    sp_end();
    return 0;
}


/*
 * Replace Bullet server by capability in buffer.
 */
static int
new_bullet(hdr, buf, size)
header *hdr;
char *buf;
{
    capability servercaps[NBULLETS];
    capability newserver;
    int which;

    if (!for_me(hdr)) {
	return 0;
    }

    if (!fully_initialised()) {
	hdr->h_status = STD_NOTNOW;
	sp_end();
	return 0;
    }

    which = hdr->h_extra;
    if ((which < 0 || which >= NBULLETS) ||
	(buf_get_cap(buf, &buf[size], &newserver) == NULL))
    {
	sp_end();
	hdr->h_status = STD_COMBAD;
	return 0;
    }

    if (which == 1) {
	MON_EVENT("replacing Bullet server 1");
    } else {
	MON_EVENT("replacing Bullet server 0");
    }

    /* TODO:  What about checking whether this new server is up
     * before accepting it?
     */

    fsvr_get_svrs(servercaps, NBULLETS);
    servercaps[which] = newserver;

    /* The super capabilities for the bullet server are kept as capabilities
     * for the invalid object 0.
     * When the intention is accepted and executed, the file server will
     * be replaced.
     */
    intent_add((objnum)0, servercaps);

    hdr->h_status = sp_commit();
    return 0;
}


/*
 * A clock tick has arrived.  If not busy call the watchdog.
 */
static void
call_watchdog()
{
    MON_EVENT("watchdog bells");

    N_ticks++;
    if (!fully_initialised()) {
	try_to_recover();
    }

    /* If not completely recovered yet, don't call the watchdog */
    if (!fully_initialised()) {
	sp_end();
	return;
    }

    /* clear the intentions list */
    intent_check(1);

    /* Only participate in background replication if in one-copy-mode, or
     * if I'm the master.  
     */
    if (get_copymode() == 1) {
	watchdog();
	sp_begin(); /* get global lock again; it was unlocked by watchdog() */
	expire_delete_dirs();
    } else {
	if (sp_whoami() == 0) {
	    watchdog();
	} else {
	    /* See if all file servers are available again; we must do it
	     * here, since this replica doesn't run the normal watchdog code.
	     */
	    fsvr_check_svrs();
	    expire_delete_dirs();
	}
    }
}

static int
sp_tick(hdr)
header *hdr;
{
    if (!for_me(hdr)) {
	return 0;
    }
    time_update();
    call_watchdog();
    hdr->h_status = STD_OK;
    return 0;
}

static int
sp_shutdown(hdr)
header *hdr;
{
    if (!for_me(hdr)) {
	return 0;
    }

    message("going to shut down");
    hdr->h_status = STD_OK;
    putrep(hdr, NILBUF, 0);	/* first tell we accepted it */
    sp_end();
    exit(0);
    return -1;
}

/*
 * Switch on the command field in a request.  Return the size of the reply.
 * If the command returns SP_REPEAT, we'll try again after a while.
 * Careful here:  I didn't go through the trouble of saving the input buffer.
 * This works as long as all commands that might return SP_REPEAT
 * (i.e., the ones that call sp_commit()) leave the input buffer alone.
 */
static int
sp_command(hdr, inbuf, n, outbuf, outsize)
header *hdr;
char *inbuf;
int n;
char *outbuf;
int outsize;
{
    header copy_hdr;
    unsigned int sleep_time = 1;
    int size;
    
    copy_hdr = *hdr;
    for (;;) {
	switch (hdr->h_command) {
	case SP_SEQ:	    size = sp_gotseq      (hdr);	     break;
	case SP_TICK:	    size = sp_tick        (hdr);	     break;
	case SP_SHUTDOWN:   size = sp_shutdown    (hdr);     	     break;
	case STD_AGE:	    size = expire_std_age (hdr);	     break;
	case SP_AGE:	    size = expire_age     (hdr);	     break;
	case SP_INTENTIONS: size = sp_gotintent   (hdr, inbuf, n);   break;
	case SP_COPY:	    size = got_super_block(hdr, inbuf, n);   break;
	case SP_NEWBULLET:  size = new_bullet     (hdr, inbuf, n);   break;
	case STD_STATUS:    size = sp_std_status  (hdr, outbuf, outsize);
								     break;
	case SP_EXPIRE:	    size = expire_dirs    (hdr, inbuf, n,
						   outbuf, outsize); break;
	case SP_APPEND:	    /* If the append operation is accepted, we will be
			     * adding one more row to the cache.  See if we
			     * need to uncache another directory first.
			     */
			    make_room(1); /* fallthrough */
	default:	    hdr->h_port = _sp_generic.cap_port;
			    size = sp_trans(hdr, inbuf, n, outbuf, outsize);
			    break;
	}
	if (hdr->h_status != SP_REPEAT) {
	    return size;
	}

	/*
	 * Retry the request, using the original header.
	 */
	*hdr = copy_hdr;

	if (sp_whoami() != 0) {
	    /* Sleep a while; if it fails again, sleep a bit longer.
	     * NOTE: livelock is not protected against, currently.
	     * It is made unlikely by slowing down Soap1 enough, eventually.
	     */
	    (void) sleep(sleep_time);
	    if (sleep_time < 32) {
		sleep_time++;
	    }
	}
    }
}

#ifdef __STDC__
static bufsize
get_request(port *getport, header *hdr, char *buf, bufsize size)
#else
static bufsize
get_request(getport, hdr, buf, size)
port   *getport;
header *hdr;
char   *buf;
bufsize size;
#endif
{
    bufsize n;

    /* get a request, ignoring errors */
    for (;;) {
	hdr->h_port = *getport;
	n = getreq(hdr, buf, size);

	if (ERR_STATUS(n)) {
	    errstat err = ERR_CONVERT(n);

	    scream("getwork: getreq failed (%s)", err_why(err));
	    (void) sleep(1);
	} else {
	    return n;
	}
    }
}

/*
 * Get a request, execute it, and send a reply.
 */
static void
sp_getwork(getport)
port *getport;
{
    header hdr;
    char inbuf[SP_BUFSIZE], outbuf[SP_BUFSIZE];
    bufsize n;
    int reply_size;
    
    n = get_request(getport, &hdr, inbuf, (bufsize)sizeof(inbuf));

    /*
     * Now execute it.  A negative reply_size is used to tell us that the
     * the putrep() already has been done.
     */
    reply_size = sp_command(&hdr, inbuf, (int) n,
			    outbuf, (int) sizeof(outbuf));
    if (reply_size >= 0) {
	putrep(&hdr, outbuf, (bufsize) reply_size);
    }
    
#if 0
    /* execute intentions if not busy */
    if (sp_try()) {
	sp_end();
    }
#endif
}

/*
 * This routine is the thread to get requests on the private port for this
 * server
 */
/*ARGSUSED*/
static void
admin_requests(param, psize)
char *param;
int psize;
{
    (void) timeout(DEFAULT_TIMEOUT);
    
    for (;;) {
	sp_getwork(&my_super_getport);
    }
}


/*
 * This is the entry procedure of a thread.  It calls sp_getwork() repeatedly.
 */
/*ARGSUSED*/
static void
worker(param, psize)
char *param;
int psize;
{
    (void) timeout(DEFAULT_TIMEOUT);
    
    for (;;) {
	sp_getwork(&_sp_getport);
    }
}

/* This is the timer thread.  It calls the watchdog every minute. */

/*ARGSUSED*/
static void
timer(param, psize)
char *param;
int psize;
{
    unsigned int sleep_time;

    (void) timeout(DEFAULT_TIMEOUT);
    sleep_time = 60;
    if (sp_whoami() == 1) {
	/* Let soap1 sleep a bit longer.  This the avoids the (unlikely)
	 * possibility that both servers continously fail to get their
	 * seq_number at the other side, after both went down in 2-copy mode.
	 */
	sleep_time += 2;
    }

    for (;;) {
	(void) sleep(sleep_time);
	time_update();
	if (sp_try()) {
	    call_watchdog();
	}
    }
}


/*
 * This is a permanent server thread that is active on the private port.
 * It is started very early to answer any requests that come in on the private
 * port while soap is initialising.  This has two functions:
 *   1. To stop another invocation of the SOAP server with the same replica
 *	number.
 *   2. To let the boot server know that it is alive and not try to start
 *	it again just because it is slow at starting up.
 * You may be wondering what happens if real work comes in.  The answer is
 * that when real work does come in then SOAP should be trying to start up
 * a second thread to handle real work by then.  Therefore there should be
 * a very short wait on the mutex here before this thread can execute the
 * request and turn into a normal private port thread.
 */
static char init_std_info[] = "SOAP server initializing";

/*ARGSUSED*/
void
init_server(param, psize)
char *param;
int psize;
{
    header hdr;
    char inbuf[SP_BUFSIZE], outbuf[SP_BUFSIZE];
    bufsize req_size;
    int reply_size;
    
    /*
     * In the initialisation phase we want to handle STD_INFO
     * separately and always answer without locking anything.
     * After that we want to let sp_command handle everything
     * We can't just switch over to sp_getwork since it also puts
     * big buffers on the stack.
     */
    for (;;) {
	req_size = get_request(&my_super_getport, &hdr,
			       inbuf, (bufsize) sizeof(inbuf));

	if (!Initialised && hdr.h_command == STD_INFO) {
	    hdr.h_status = STD_OK;
	    putrep(&hdr, init_std_info, strlen(init_std_info));
	} else {
	    /* We may not be allowed to do real work yet.  The
	     * mutex "Init_done" will be released by main() when
	     * it's ok to go ahead.
	     */
	    mu_lock(&Init_mutex);
	    
	    reply_size = sp_command(&hdr, inbuf, (int) req_size,
				    outbuf, (int) sizeof(outbuf));
	    if (reply_size >= 0) {
		putrep(&hdr, outbuf, (bufsize) reply_size);
	    }

	    mu_unlock(&Init_mutex);
	}
    }
    /*NOTREACHED*/
}

static void
start_up()
{
    int nstarted;

    intent_rollforward();	/* make sure the intentions are done. */
    init_sp_table(Startup_Fsck);

#ifdef AMOEBA
    /* try to start server_threads worker threads */
    for (nstarted = 0; nstarted < N_threads; nstarted++) {
	if (!thread_newthread(worker, STACKSIZE, (char *)NULL, 0)) {
	    MON_EVENT("thread_newthread failed");
	    scream("thread_newthread failed");
	    break;
	}
    }
    if (nstarted == 0) {
	panic("no worker threads started");
    } else {
	message("%d threads started", nstarted);
    }
#endif
}

/*
 * The initialization procedure.  Initialize the cache, and read the super
 * file.  If in one-copy-mode I can load the directories and start operation.
 * Otherwise I will have to communicate with the other server first.
 */
void
sp_doinit(max_Kbytes)
int max_Kbytes;
{
    char    info[40];
    int	    size;
    errstat err;
    
    mu_init(&sp_mutex);

    if (!cc_init(NCAPCACHE)) {
	panic("doinit: cc_init failed (not enough memory)");
    }

    cache_set_size(max_Kbytes);
    /* This computes the maximum number of directories to be kept in-cache,
     * which is also used by super_init to determine how many blocks of
     * the super file should be kept in-core.
     */

    /*
     * Allocate and read super file.  The capability for is fetched
     * from the capability environment.
     * Also initialises variables stored in block 0,
     * such as ports to listen to, file servers to be used etc.
     */
    super_init(&_sp_generic, &my_supercap, &other_supercap);

    /*
     * Create public versions of the capabilities retrieved
     * from the super block
     */
    _sp_getport = _sp_generic.cap_port;
    priv2pub(&_sp_getport, &_sp_generic.cap_port);

    my_super_getport = my_supercap.cap_port;
    priv2pub(&my_super_getport, &my_supercap.cap_port);
    
    priv2pub(&other_supercap.cap_port, &other_supercap.cap_port);
    /* We only need the put version of the other side's supercap */

    /*
     * Make sure there is no other server listening to my port.
     */
    err = std_info(&my_supercap, info, (int) sizeof(info), &size);
    if (err == STD_OK || err == STD_OVERFLOW) {
	/* Oops, a SOAP server is already listening to this port: */
	panic("server %d is already active", sp_whoami());
    }

    /*
     * Initialise other modules.
     */
    cache_init(max_Kbytes);
    time_update();

    expire_init();
#ifdef AMOEBA
    /* Start a temporary thread listening to this port, to make any
     * later servers invoked on this port go away:
     */
    if (!thread_newthread(init_server, INITSERVERSIZE, (char *)0, 0)) {
	panic("cannot start the init server thread");
    }
#endif
    message("starting in %d copy mode", get_copymode());
    try_to_recover();
}

static void
usage(progname)
char *progname;
{
    printf("Usage: %s [-n] [-k max_Kbytes] 0|1\n", progname);
    exit(1);
}

#ifndef AMOEBA
static void
interrupted(sig)
int sig;
{
    /* used to force controlled exit, needed for profiling */
    exit(1);
}
#endif

/* The main procedure of the directory server.  Call sp_doinit and start the
 * timer thread.  Then start accepting requests on the private port.
 */
int
main(argc, argv)
int argc;
char **argv;
{
    /* argument processing: */
    extern int getopt();
    extern int optind;
    extern char *optarg;
    int optchar;
    char *progname = "soap";
    int max_Kbytes = DEF_MAX_KBYTES;	/* for cache size */
    mutex hang_me;
    
    if (argv[0] != NULL) {
	progname = argv[0];
    }

    (void) timeout(DEFAULT_TIMEOUT);
    
    while ((optchar = getopt(argc, argv, "A:FM:k:ns:?")) != EOF) {
	switch (optchar) {
	case 'A':
	    Auto_timeout = atoi(optarg);
	    break;
	case 'F':
	    Forced_startup = 1;
	    break;
	case 'M':
	    Manual_timeout = atoi(optarg);
	    break;
	case 'k':
	    max_Kbytes = atoi(optarg);
	    if (max_Kbytes < MIN_KBYTES || max_Kbytes > MAX_KBYTES) {
		fprintf(stderr, "%s: -k %d: bad size (allowed %d..%d)\n",
			progname, max_Kbytes, MIN_KBYTES, MAX_KBYTES);
		exit(1);
	    }
	    break;
	case 'n':
	    /* Don't cache in & check all directory files on startup.
	     * This might save a minute startup time or so, depending on
	     * the number of directories.
	     */
	    Startup_Fsck = 0;
	    break;
	case 's':
	    N_threads = atoi(optarg);
	    if (N_threads < 1 || N_threads > MAX_THREADS) {
		fprintf(stderr, "%s: -s %d: invalid number of threads.\n",
							progname, N_threads);
		if (N_threads > MAX_THREADS)
		    N_threads = MAX_THREADS;
		else
		    N_threads = 1;
		fprintf(stderr, "%s: defaulting to %d threads\n",
							progname, N_threads);
	    }
	    break;

	case '?':
	default:
	    usage(progname);
	    break;
	}
    }

    /*
     * fetch the required 0|1 argument
     */
    if (optind != argc - 1) {
	usage(progname);
    }

    /* don't use atoi, to avoid interpreting a bogus arg as "0" */
    if (strcmp(argv[optind], "0") == 0) {
	sp_set_whoami(0);
    } else if (strcmp(argv[optind], "1") == 0) {
	sp_set_whoami(1);
    } else {
	usage(progname);
    }

    message("initializing");
    mu_lock(&Init_mutex);
    sp_doinit(max_Kbytes);

#ifdef AMOEBA
    if (!thread_newthread(timer, TIMERSIZE, (char *)0, 0)) {
	panic("cannot start the timer thread");
    }
#endif

    /* Tell the init server that it is okay to carry out one request,
     * if it got one through no fault of its own:
     */
    mu_unlock(&Init_mutex);
    Initialised = 1;

#ifdef AMOEBA
    if (!thread_newthread(admin_requests, STACKSIZE, (char *)0, 0)) {
	panic("main: thread_newthread failed");
    }
    
    mu_init(&hang_me);
    mu_lock(&hang_me);
    mu_lock(&hang_me);
    panic("Escaped the hang_up in main\n");
    /*NOTREACHED*/
#else
    if (signal(SIGINT, interrupted) == SIG_IGN) {
	signal(SIGINT, SIG_IGN);
    }
    if (!fully_initialised()) {
	panic("single threaded soap couldn't initialise");
    } else {
	worker((char *)NULL, 0);
    }
#endif
}
