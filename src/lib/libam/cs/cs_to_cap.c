/*	@(#)cs_to_cap.c	1.3	94/04/07 09:59:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "module/stdcmd.h"
#include "module/goodport.h"

/* Get a useable capability from a capset, setting *cap.
 * Returns the first capability in the set for which std_info returns STD_OK.
 * If there are no caps in the set for which std_info returns STD_OK,
 * then set *cap to the last cap in the set and return the err status std_info.
 * Uses the goodport module to avoid attempting a transaction with a port that
 * has previously returned RPC_NOTFOUND.
 */
errstat
cs_goodcap(cs, cap)
capset *cs;
capability *cap;
{
	register int i;
	register suite *s;
	register capability *lastcap = 0;
	int ignore;
	errstat err = STD_SYSERR;	/* Returned for empty capset */
	interval savetimeout;

	for (i = 0; i < cs->cs_final && err != STD_OK; i++) {
		s = &cs->cs_suite[i];
		if (s->s_current) {
			lastcap = &s->s_object;

			savetimeout = timeout((interval)4000);
			err = gp_std_info(lastcap, (char *)0, 0, &ignore);
			/* we expect to overflow the null buffer, so: */
			if (err == STD_OVERFLOW) {
				err = STD_OK;
			}
			(void)timeout(savetimeout);
		}
	}

	if (lastcap)
		*cap = *lastcap;
	return err;
}

/* Get a capability from a capset giving preference to a working capability.
 * If there is only one cap in the set, set *cap to that cap.  If and only
 * if there is more than one, try std_info on each of them to obtain one
 * that is useable, setting *cap to that one.  If none of the multiple
 * caps work, set *cap to the last one.  Callers who need to know whether the
 * cap is useable should use cs_goodcap(), above.  Returns STD_OK, unless
 * the capset has no caps, in which case, returns STD_SYSERR.
 */
errstat
cs_to_cap(cs, cap)
capset *cs;
capability *cap;
{
	register int i;
	register suite *s, *s_save;
	int setsize = 0;

	for (i = 0; i < cs->cs_final; i++) {
		s = &cs->cs_suite[i];
		if (s->s_current) {
			++setsize;
			s_save = s;
		}
	}
	if (setsize == 0)
		return STD_SYSERR;
	if (setsize == 1)
		*cap = s_save->s_object;
	else
		(void)cs_goodcap(cs, cap);
	return STD_OK;
}
