/*	@(#)lintlib.c	1.7	96/02/27 11:22:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <thread.h>
#include <machdep.h>
#include <exception.h>
#include <fault.h>
#include <module/signals.h>
#include <module/syscall.h>
#include <module/proc.h>
#include <module/rpc.h>
#include <group.h>
#include <stderr.h>

/*ARGSUSED*/
bufsize
getreq(h, buf, size)
header *h;
bufptr buf;
bufsize size;
{
    return 0;
}

/*ARGSUSED*/
bufsize
trans(h1, buf1, siz1, h2, buf2, siz2)
header *h1, *h2;
bufptr buf1, buf2;
bufsize siz1, siz2;
{
    return 0;
}

/*ARGSUSED*/
void
putrep(h, buf, size)
header *h;
bufptr buf;
bufsize size;
{
}

/*ARGSUSED*/
interval
timeout(itv)
interval itv;
{
    return 0;
}

/*ARGSUSED*/
long
rpc_trans(h1, buf1, siz1, h2, buf2, siz2)
header *h1, *h2;
bufptr buf1, buf2;
f_size_t siz1, siz2;
{
    return 0;
}

/*ARGSUSED*/
long
rpc_getreq(h, buf, size)
header *h;
bufptr buf;
f_size_t size;
{
    return 0;
}

/*ARGSUSED*/
long
rpc_putrep(h, buf, size)
header *h;
bufptr buf;
f_size_t size;
{
    return 0;
}

/*ARGSUSED*/
int
getinfo(retcap, procbuf, len)
capability *retcap;
process_d *procbuf;
int len;
{
    return 0;
}

/*ARGSUSED*/
segid
_sysseg_map(cap, addr, len, flags)
capability *cap;
vir_bytes addr;
vir_bytes len;
long flags;
{
    return 0;
}

/*ARGSUSED*/
errstat
_sysseg_unmap(id, cap)
segid id;
capability *cap;
{
    return 0;
}

/*ARGSUSED*/
errstat
_sysseg_grow(id, newsize)
segid id;
vir_bytes newsize;
{
    return 0;
}

/*ARGSUSED*/
unsigned long
sys_milli()
{
    return 0;
}

#ifdef sparc

/*ARGSUSED*/
int
_mu_trylock(mu, maxdelay)
mutex *mu;
interval maxdelay;
{
    return 0;
}

/*ARGSUSED*/
void
_mu_unlock(mu)
mutex *mu;
{
}

/*ARGSUSED*/
int
__ldstub(p)
char * p;
{
    /* sparc indivisible load/store unsigned byte instruction */
    return 0;
}

#endif

/*ARGSUSED*/
int
mu_trylock(mu, maxdelay)
mutex *mu;
interval maxdelay;
{
    return 0;
}

/*ARGSUSED*/
void
mu_unlock(mu)
mutex *mu;
{
}

/*ARGSUSED*/
struct thread_data *
sys_getlocal()
{
    return 0;
}

/*ARGSUSED*/
int
sys_newthread(func, td1, td2)
void (*func)();
struct thread_data *td1, *td2;
{
    return 0;
}

/*ARGSUSED*/
void
exitthread(ready)
long *ready;
{
}

/*ARGSUSED*/
void
threadswitch()
{
}

/*ARGSUSED*/
void
sig_raise(sig)
signum sig;
{
}

/*ARGSUSED*/
void
sys_setvec(v, n)
sig_vector *v;
int n;
{
}

/*ARGSUSED*/
void
sys_sigret(us)
thread_ustate *us;
{
}

/*ARGSUSED*/
void
exitprocess(status)
int status;
{
}

/*ARGSUSED*/
void
sys_cleanup()
{
}

/*ARGSUSED*/
g_id_t
grp_create(p, resilience, maxgroup, nbuf, maxmess)
port    *p;
g_indx_t resilience;
g_indx_t maxgroup;
uint32   nbuf;
uint32   maxmess;
{
    return 0;
}

/*ARGSUSED*/
errstat
grp_forward(gid, p, memid)
g_id_t   gid;
port    *p;
g_indx_t memid;
{
    return STD_OK;
}

/*ARGSUSED*/
errstat
grp_info(gid, p, state, memlist, size)
g_id_t     gid;
port      *p;
grpstate_p state;
g_indx_t  *memlist;
g_indx_t   size;
{
    return STD_OK;
}

/*ARGSUSED*/
g_id_t
grp_join(hdr)
header_p hdr;
{
    return STD_OK;
}

/*ARGSUSED*/
errstat
grp_leave(gid, hdr)
g_id_t   gid;
header_p hdr;
{
    return STD_OK;
}

/*ARGSUSED*/
int32
grp_receive(gid, hdr, buf, cnt, more)
g_id_t   gid;
header_p hdr;
bufptr   buf;
uint32   cnt;
int     *more;
{
    return 0;
}

/*ARGSUSED*/
g_indx_t
grp_reset(gid, hdr, n)
g_id_t   gid;
header_p hdr;
g_indx_t n;
{
    return 0;
}

/*ARGSUSED*/
errstat
grp_send(gid, hdr, buf, cnt)
g_id_t   gid;
header_p hdr;
bufptr   buf;
uint32   cnt;
{
    return STD_OK;
}

/*ARGSUSED*/
errstat
grp_set(gid, p, sync, reply, alive, large)
g_id_t   gid;
port    *p;
interval sync;
interval reply;
interval alive;
uint32   large;
{
    return STD_OK;
}

/*ARGSUSED*/
void
thread_en_preempt(local_ptr)
struct thread_data **local_ptr;
{
}

void
thread_get_max_prio(max)
long *max;
{
	*max = 0;
}


#include "protocols/flip.h"
#include "module/rawflip.h"

/*ARGSUSED*/
int
rawflip(p)
struct rawflip_parms * p;
{
    return 0;
}
