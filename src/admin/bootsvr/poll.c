/*	@(#)poll.c	1.6	96/02/27 10:11:25 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "monitor.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdcom.h"
#include "svr.h"
#include "module/proc.h"
#include "module/stdcmd.h"
#include "stdlib.h"

extern obj_rep objs[MAX_OBJ];

#define MAX_INFO 20	/* How many chars of the info string we remember */

static void
TryBecomeOwner(boot)
    obj_rep *boot;
{
    capability owner;
    errstat err;

    owner.cap_port = putcap.cap_port;
    prv_encode(&owner.cap_priv,
		(objnum) prv_number(&boot->or.or_objcap.cap_priv),
		PRV_ALL_RIGHTS, &boot->or.or_objcap.cap_priv.prv_random);
    err = pro_setowner(&boot->or.or_proccap, &owner);
    if (err == STD_OK) {
	boot->or.or_status |= ST_AMOWNER;
	MON_EVENT("Became owner");
	if (debugging) prf("%nbecame owner of %s\n", boot->or.or_data.name);
    } else if (err == STD_CAPBAD) {
	boot->or.or_status &= ~ST_GOTPROC;
	(void) memset((_VOIDSTAR) &boot->or.or_proccap.cap_port, 0, sizeof(port));
	SaveState = 1;
	if (verbose) prf("%n%s has gone\n", boot->or.or_data.name);
    } else {
	prf("%ncannot become owner of %s (%s)\n",
		boot->or.or_data.name, err_why(err));
    }
} /* TryBecomeOwner */


#define NPOLL_TRIES	10	/* max number of std_info() tries in a row */

static errstat
do_std_info(cap, buf, siz, retlen)
capability *cap;
char       *buf;
int	    siz;
int	   *retlen;
{
    int     try;
    errstat err;

    for (try = 0; try < NPOLL_TRIES; try++) {
	err = std_info(cap, buf, siz, retlen);

	/* Only retry if the rpc was aborted or if it failed */
	if ((err != RPC_ABORTED) && (err != RPC_FAILURE)) {
	    break;
	}
    }

    if (err == STD_OVERFLOW && siz > 0) {
	/* info string returned, but too long:  truncate */
	buf[siz - 1] = '\0';
	*retlen = strlen(buf);
	err = STD_OK;
    }

    return err;
}

/*
 *	Poll a process capability
 */
static int
PollProc(boot, cap)
    obj_rep *boot;
    capability *cap;
{
    int i, len;
    errstat err;
    char infobuf[80];
    static struct {
	errstat error;
	int condition;
    } table[] = {
	{ RPC_FAILURE,	POLL_DOWN },
	{ RPC_NOTFOUND,	POLL_DOWN },
	{ RPC_TRYAGAIN,	POLL_MAYBE },
	{ STD_OK,	POLL_RUNS },
	{ STD_CAPBAD,	POLL_DOWN },
    };

    MON_EVENT("Poll process");
    infobuf[0] = '\0';
    len = 0;
    if (badport(cap)) err = RPC_NOTFOUND;
    else {
	err = do_std_info(cap, infobuf, sizeof(infobuf), &len);
	(void) capsaid(cap, err);
    }
    if (err != STD_OK) {
	sprintf(boot->or.or_errstr, "stdinfo process error: %s", err_why(err));
	if (verbose) prf("%n%s: %s\n", boot->or.or_data.name, boot->or.or_errstr);
    } else {
	if (len < 0 || len >= sizeof(infobuf))
	    (void) strcpy(boot->or.or_errstr, "bad len from stdinfo");
	else {
	    /* Truncate: */
	    if (len > MAX_INFO) len = MAX_INFO;
	    infobuf[len] = 0;
	    (void) strcpy(boot->or.or_errstr + 1, infobuf);
	    /* Quote the info string: */
	    boot->or.or_errstr[0] = boot->or.or_errstr[len + 1] = '"';
	    boot->or.or_errstr[len + 2] = '\0';
	}
    }

    for (i = SIZEOFTABLE(table); i--; )
	if (err == table[i].error) return table[i].condition;
    prf("%nunknown process-server response to std_info: %s\n", err_why(err));
    return POLL_CANNOT;
} /* PollProc */

/*
 *	Poll an object of the service
 */
static int
PollCap(boot, cap)
    obj_rep *boot;
    capability *cap;
{
    int i, len;
    errstat err;
    char infobuf[80];
    static struct {
	errstat error;
	int condition;
    } table[] = {
	{ RPC_FAILURE,	POLL_MAYBE },
	{ RPC_NOTFOUND,	POLL_DOWN },
	{ RPC_TRYAGAIN,	POLL_MAYBE },
	{ STD_OK,	POLL_RUNS },
	{ STD_NOTFOUND,	POLL_DOWN },
	{ STD_CAPBAD,	POLL_MAYBE },
    };

    MON_EVENT("Poll some capability");
    infobuf[0] = '\0';
    len = 0;
    if (!(boot->or.or_status & ST_AMOWNER) && (boot->or.or_status & ST_GOTPROC))
	TryBecomeOwner(boot);
    if (badport(cap)) err = RPC_NOTFOUND;
    else {
	err = do_std_info(cap, infobuf, sizeof(infobuf), &len);
	if (err == STD_OK) {
	    /* Truncate */
	    if (len > MAX_INFO) len = MAX_INFO;
	    infobuf[len] = '\0';
	}
	(void) capsaid(cap, err);
    }
    if (err != STD_OK) {
	sprintf(boot->or.or_errstr, "cappoll failed: %s", err_why(err));
	if (verbose) prf("%n%s: %s\n", boot->or.or_data.name, boot->or.or_errstr);
    } else {
	(void) strcpy(boot->or.or_errstr + 1, infobuf);
	/* Quote the info string: */
	boot->or.or_errstr[0] = boot->or.or_errstr[len + 1] = '"';
	boot->or.or_errstr[len + 2] = '\0';
    }

    for (i = SIZEOFTABLE(table); i--; )
	if (err == table[i].error) return table[i].condition;
    return POLL_CANNOT;
} /* PollCap */

static char *
poll_to_s(x)
    int x;
{
    switch (x) {
    case POLL_DOWN: return "down";
    case POLL_CANNOT: return "cannot";
    case POLL_MAYBE: return "maybe";
    case POLL_RUNS: return "up";
    }
    abort();
    /*NOTREACHED*/
} /* poll_to_s */

static int
waitfordependencies(boot)
    obj_rep *boot;
{
    int i;

    if (boot->or.or_data.depname[0]==0)
	return 0;
    for (i=0; i<MAX_OBJ; i++) {
	if (strncmp(boot->or.or_data.depname, objs[i].or.or_data.name,
							sizeof(ident))==0)
	    return !(objs[i].or.or_status & ST_POLLOK);
    }
    prf("%nCannot find dependency %s\n", boot->or.or_data.depname);
    MON_EVENT("Cannot find dependency");
    return 0;	/* Should never happen */
}

/*
 *	Poll a service.
 *	This (should) return either of:
 *	- The process runs
 *	- The process may run
 *	- The process doesn't run
 *	- The process cannot be made runnable / should not be polled at all
 */
int
Poll(boot)
    obj_rep *boot;
{
    capability cap;
    errstat err;
    int ret;

    if (boot->or.or_data.flags & BOOT_INACTIVE)
	return POLL_CANNOT;
    if (waitfordependencies(boot)) {
	if (debugging)
	    prf("%nDon't poll(%s)\n", boot->or.or_data.name);
	sprintf(boot->or.or_errstr, "After %s", boot->or.or_data.depname);
	boot->or.or_nextpoll =
			add_time(my_gettime(), boot->or.or_data.pollrate);
	return POLL_CANNOT;
    }
    if (debugging)
	prf("%nPoll(%s)\n", boot->or.or_data.name);
    if (boot->or.or_data.flags & BOOT_POLLREF) {
	if ((err = ref_lookup(&boot->or.or_data.pollcap, &cap, boot)) != STD_OK) {
	    sprintf(boot->or.or_errstr, "cannot lookup pollcap (%s)", err_why(err));
	    boot->or.or_nextpoll =
			add_time(my_gettime(), boot->or.or_data.pollrate);
	    return POLL_MAYBE;
	}
	ret = PollCap(boot, &cap);
	switch (ret) {
	case POLL_RUNS:
	    MON_EVENT("Poll cap: runs");
	    break;
	case POLL_DOWN:
	    MON_EVENT("Poll cap: down");
	    if (boot->or.or_status & ST_GOTPROC) {
		int x;
		x = PollProc(boot, &boot->or.or_proccap);
		if (x != POLL_DOWN) {
		    MON_EVENT("Poll cap: down, BUT poll proc not down");
		    if (verbose || debugging)
			prf("%n%s: cap poll failed, but poll proc says %s\n",
			    boot->or.or_data.name, poll_to_s(x));
		    MON_EVENT("poll: poll-cap down, process not down...");
		    return POLL_MAYBE;
		}
	    }
	    break;
	case POLL_MAYBE:
	    MON_EVENT("Poll cap: maybe");
	    break;
	case POLL_CANNOT:
	    MON_EVENT("Poll cap: cannot");
	    break;
	default:
	    abort();
	}
    } else {
	if (boot->or.or_status & ST_ISIDLE) return POLL_DOWN;
	if (!(boot->or.or_status & ST_GOTPROC)) {
	    if (boot->or.or_errstr[0] != '\0')
		sprintf(boot->or.or_errstr, "Cannot poll - got no proccap");
	    return POLL_MAYBE;
	}
	if (NULLPORT(&boot->or.or_proccap.cap_port)) {
	    MON_EVENT("proccap: null port");
	    return POLL_CANNOT;
	}
	ret = PollProc(boot, &boot->or.or_proccap);
	switch (ret) {
	case POLL_RUNS:
	    MON_EVENT("Poll proc: runs");
	    break;
	case POLL_DOWN:
	    MON_EVENT("Poll proc: down");
	    break;
	case POLL_MAYBE:
	    MON_EVENT("Poll proc: maybe");
	    break;
	case POLL_CANNOT:
	    MON_EVENT("Poll proc: cannot");
	    break;
	default:
	    abort();
	}
    }

    boot->or.or_nextpoll = add_time(my_gettime(), boot->or.or_data.pollrate);

    return ret;
} /* Poll */
