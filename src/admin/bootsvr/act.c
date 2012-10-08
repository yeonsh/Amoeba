/*	@(#)act.c	1.5	94/04/06 11:37:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "monitor.h"
#include "svr.h"

obj_rep objs[MAX_OBJ];


/*
 *	Return a fresh and locked slot in objs
 */
int
new_obj()
{
    int n;

    for (n = 0; n < MAX_OBJ; ++n) {
	if (!obj_in_use(&objs[n])) {
	    /* Lock it */
	    mu_lock(&objs[n].or_lock);
	    if (!obj_in_use(&objs[n])) {
		(void) memset((_VOIDSTAR) &objs[n].or, 0, sizeof(objs[n].or));
		objs[n].or.or_nextpoll = objs[n].or.or_nextboot = my_gettime();
		return n;
	    }
	    mu_unlock(&objs[n].or_lock);
	}
    }
    MON_EVENT("No free slot in objs[]");
    return -1;
} /* new_obj */


/*
 *	Check a capability, set *objpp.
 *	*objpp is NULL for the supercap.
 */
errstat
boot_act(p, objpp)
    private *p;
    obj_ptr *objpp;
{
    rights_bits r;
    objnum n;

    n = prv_number(p);
    if (n == 0) {	/* Our supercap */
	*objpp = NULL;
	if (prv_decode(p, &r, &putcap.cap_priv.prv_random)) {
	    if (debugging) prf("%nboot_act: supercap checkword mismatch\n");
	    return STD_CAPBAD;
	}
	return STD_OK;
    }
    n = FindNum(n);
    if (n >= SIZEOFTABLE(objs) || n < 0 || !obj_in_use(&objs[n])) {
	if (debugging) prf("%nboot_act: object no longer in use\n");
	MON_EVENT("act: object doesn't exist (anymore)");
	return STD_CAPBAD;
    }
    *objpp = &objs[n];
    if (prv_decode(p, &r, &objs[n].or.or_objcap.cap_priv.prv_random) == 0)
	return STD_OK;

    printf("boot_act:object %d: got %s, ", n, ar_priv(p));
    printf("expected %s\n", ar_priv(&objs[n].or.or_objcap.cap_priv));

    MON_EVENT("act: checkword mismatch");
    if (debugging)
	prf("%nboot_act: checkword mismatch on %s\n", objs[n].or.or_data.name);
    return STD_CAPBAD;
} /* boot_act */

/*ARGSUSED*/
void
deact(obj)
    obj_ptr obj;
{
    /* This one is called just after a putrep() */
    if (closing) {
	if (debugging) prf("%nServerthread exit\n");
	thread_exit();
    }
} /* deact */
