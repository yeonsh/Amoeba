/*	@(#)impl_ps.c	1.7	96/02/27 10:11:09 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "monitor.h"
#include "exception.h"
#include "svr.h"
#include "proswapproc.h"
#include "module/prv.h"
#include "module/proc.h"
#include "stdlib.h"

/*ARGSUSED*/
errstat
impl_pro_swapproc(_hdr, _obj, oldproc, newproc)
    header *_hdr;
    obj_ptr _obj;
    capability *oldproc, *newproc;
{
    errstat err;
    int len;
    char buf[40];

    if (_obj == NULL) {
	if (debugging) prf("%nswapproc(supercap) !?!?\n");
	return STD_SYSERR;
    }
    if (prv_number(&oldproc->cap_priv) != prv_number(&_obj->or.or_proccap.cap_priv)
		|| !PORTCMP(&oldproc->cap_port, &_obj->or.or_proccap.cap_port)) {
	MON_EVENT("swap_proc: inconsistent swapproc");
	if (verbose || debugging)
	    prf("%nswapproc: old cap is not the right one\n");
	return STD_ARGBAD;
    }
    _obj->or.or_proccap = *newproc;
    SaveState = 1;
    err = std_info(newproc, buf, (int) (sizeof(buf) - 1), &len);
    if (err == STD_OVERFLOW) { /* ignore the rest */
	len = sizeof(buf) - 1;
	err = STD_OK;
    }
    /* make sure the string is null-terminated */
    if (err == STD_OK) buf[len] = '\0';
    if (debugging)
	prf("%nPro_swapproc(%s): %s\n", _obj->or.or_data.name,
		err == STD_OK ? buf : err_why(err));
    return STD_OK;
} /* impl_pro_swapproc */

/*ARGSUSED*/
errstat
impl_ps_checkpoint(_hdr, _obj, pdbuf, max_pdbuf, pdlen, cause, detail)
    header *_hdr;
    obj_ptr _obj;
    char *pdbuf; /* inout */
    int max_pdbuf;
    int *pdlen; /* inout (h_size) */
    int cause; /* in (h_extra) */
    long int detail; /* in (h_offset) */
{
    process_d *pd;

    if (_obj == NULL) {
	if (debugging) prf("%nps_checkpoint(supercap) !?!?\n");
	return STD_SYSERR;
    }
    if (!_obj->or.or_status & ST_AMOWNER) {
	MON_EVENT("checkpoint but not owner");
	return STD_NOTNOW;
    }
    if (*pdlen < 0 || *pdlen > max_pdbuf) {
	return STD_ARGBAD;
    }

    /*
    ** The process descriptor is in network byte order - we have to
    ** unpack it first
    */
    if ((pd = pdmalloc(pdbuf, pdbuf + *pdlen)) == 0) {
	MON_EVENT("pdmalloc failed");
	return STD_NOMEM;
    }
    if (buf_get_pd(pdbuf, pdbuf + *pdlen, pd) == 0) {
	MON_EVENT("buf_get_pd failed");
	free((_VOIDSTAR) pd);
	return STD_SYSERR;
    }

    if (prv_number(&pd->pd_self.cap_priv) !=
					prv_number(&_obj->or.or_proccap.cap_priv)) {
	MON_EVENT("checkpoint: inconsistent proccap");
	free((_VOIDSTAR) pd);
	return STD_CAPBAD;
    }

    switch (cause) {
    case TERM_NORMAL:
	if (detail) MON_EVENT("checkpoint: exitstatus != 0");
	else MON_EVENT("checkpoint: exitstatus == 0");
	if (verbose || debugging)
	    prf("%n%s: exit %d\n", _obj->or.or_data.name, detail);
	break;
    case TERM_STUNNED:
	MON_EVENT("checkpoint: stunned");
	if (verbose || debugging)
	    prf("%n%s: stunned %d\n", _obj->or.or_data.name, detail);
	if (detail < 0)		/* Negative stun - dump core */
	    dumpcore(_obj, pd, *pdlen);
	break;
    case TERM_EXCEPTION:
	MON_EVENT("checkpoint: exception");
	prf("%n%s: exception %s\n", _obj->or.or_data.name, exc_name((signum) detail));
	dumpcore(_obj, pd, *pdlen);
	break;
    default:
	MON_EVENT("checkpoint: WHAT'S THE CAUSE PLEASE!?");
	prf("%n%s died of unknown cause %d - detail %d\n",
				_obj->or.or_data.name, cause, detail);
	break;
    }

    *pdlen = 0;	/* Don't continue */
    _obj->or.or_status &= ~ST_GOTPROC;	/* Just believe any checkpoint? */
    _obj->or.or_status |= ST_ISIDLE;
    (void) memset((_VOIDSTAR) &_obj->or.or_proccap.cap_port, 0, sizeof(port));
    _obj->or.or_nextpoll = my_gettime();/* Request timer to handle this soon */
    SaveState = 1;
    free((_VOIDSTAR) pd);
    return STD_OK;
} /* impl_ps_checkpoint */
