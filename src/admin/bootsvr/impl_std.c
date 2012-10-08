/*	@(#)impl_std.c	1.6	94/04/06 11:39:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "monitor.h"
#include "svr.h"

/*
 *	Implementations of standard requests
 */

/*ARGSUSED*/
errstat
impl_std_info(_hdr, _obj, str, max_str, n, len)
    header *_hdr;
    obj_ptr _obj;
    char   *str;
    int max_str;
    int n;
    int *len;
{
    char msg_buf[128];

    if (_obj == NULL)
	sprintf(msg_buf, "bootsvr supercap 0x%02x", _hdr->h_priv.prv_rights);
    else
	sprintf(msg_buf, "bootsvr: %s", _obj->or.or_data.name);
    (void) strncpy(str, msg_buf, (size_t) max_str);
    str[max_str - 1] = '\0';
    *len = strlen(str);
    return STD_OK;
} /* impl_std_info */

/*ARGSUSED*/
errstat
impl_std_restrict(_hdr, _obj, mask, newcap)
    header *_hdr;
    obj_ptr _obj;
    long int mask;
    capability *newcap;
{
    rights_bits rights;
    port *random;
    rights = _hdr->h_priv.prv_rights & mask;
    random = (_obj == NULL) ?
	&get_capp->cap_priv.prv_random :
	&_obj->or.or_objcap.cap_priv.prv_random;
    newcap->cap_port = _hdr->h_port;
    if (prv_encode(&newcap->cap_priv, (objnum) prv_number(&_hdr->h_priv),
						rights, random) != 0)
	return STD_SYSERR;
    return STD_OK;
} /* impl_std_restrict */

char *
strapnd(p, q, stop)
    char *p, *q, *stop;
{
    if (p == NULL) return NULL;
    while (p < stop && (*p++ = *q++) != '\0')
	;
    return p - 1;
} /* strapnd */

char *
intapnd(p, n, stop)
    char *p, *stop;
    int n;
{
    char buf[10];
    char *bp;
    if (p == NULL || p >= stop) return NULL;
    if (n < 0) abort();
    if (n == 0) {
	*p++ = '0';
	return p;
    }
    if (stop - p < sizeof(buf)) return NULL;
    bp = buf + sizeof(buf);
    while (n > 0 && bp > buf) {
	*--bp = n % 10 + '0';
	n /= 10;
    }
    while (bp < buf + sizeof(buf)) *p++ = *bp++;
    return p;
} /* intapnd */

/*
 *	Add a status line for 'obj' to the buffer p of size q - p.
 */
/* static */ char *
status_line(obj, p, q)
    obj_ptr obj;
    char *p, *q;
{
    int len;

    /* Print name in a 16 wide field */
    p = strapnd(p, obj->or.or_data.name, q);
    if (p == NULL || p >= q - 4) return NULL;
    if ((len = strlen(obj->or.or_data.name)) < 16) {
	*p++ = '\t';
	if (len < 8) *p++ = '\t';
    }

    /* Flags in a 8 wide field */
    if (obj->or.or_data.flags & BOOT_INACTIVE) *p++ = '0';
    else *p++ = '1';
    if ((obj->or.or_status & ST_POLLOK) == 0) *p++ = 'X';
    if (obj->or.or_status & ST_GOTPROC) *p++ = 'P';
    *p++ = '\t';

    /* Print info, boottime */
    {
	char buf[100];
	if (obj->or.or_boottime != 0) {
	    extern char *ctime();
	    static mutex mu;	/* Protect ctime's internal buffer */
	    mu_lock(&mu);
	    /* Ctime gives you an \n for free... */
	    sprintf(buf, "%-25s %s", obj->or.or_errstr, ctime(&obj->or.or_boottime));
	    MU_CHECK(mu);
	    mu_unlock(&mu);
	    p = strapnd(p, buf, q);
	} else {
	    p = strapnd(p, obj->or.or_errstr, q - 1);
	    if (p != NULL) *p++ = '\n';
	}
    }
    return p;
} /* status_line */

/*ARGSUSED*/
errstat
impl_std_status(_hdr, _obj, str, max_str, n, len)
    header *_hdr;
    obj_ptr _obj; /* act_f return value */
    char str[];
    int max_str;
    int n;
    int *len;
{
    char *p;
    extern char parsstate[];
    int i;

    intr_reset();			/* Actually, this is too late */
    if (_obj != NULL) return STD_DENIED;	/* We want the supercap */
    if (n < 0) {
	return STD_ARGBAD;
    }
    if (n > max_str) {
	n = max_str;
    }
    p = strapnd(str, parsstate, str + n);
    p = strapnd(p, "NAME\t\tFLAGS\tINFO\n", str + n);
    for (i = 0; i < MAX_OBJ && p != NULL; ++i) {
	if (intr_c2s()) return STD_INTR;
	MU_CHECK(objs[i].or_lock);
	if (mu_trylock(&objs[i].or_lock, (interval) -1) == 0) {
	    /* Slot in use? */
	    if (obj_in_use(&objs[i]))
		p = status_line(&objs[i], p, str + n);
	    MU_CHECK(objs[i].or_lock);
	    mu_unlock(&objs[i].or_lock);
	} else MON_EVENT("std_status: trylock interrupted");
    }
    *len = p - str;
    return STD_OK;
} /* impl_std_status */
