/*	@(#)direct.c	1.11	96/02/27 14:13:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef ROMKERNEL

/*
 * The Kernel Directory Server
 *
 *	This thread responds to all directory requests for the kernel.
 *	It provides the normal security that soap provides.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#include "assert.h"
INIT_ASSERT
#include "capset.h"
#include "direct/direct.h"
#include <sys/types.h>
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "kerneldir.h"
#include <stdlib.h>
#include "sys/proto.h"
#include "module/prv.h"
#include "module/cap.h"


/* defined in sp_impl.c: */
extern sp_tab *sp_first_update();
extern sp_tab *sp_next_update();
extern void sp_clear_updates();
extern void sp_free_mods();
extern errstat sp_create_rootdir();
extern void sp_change_chkfield _ARGS(( objnum o, port * p ));

#define BUFFERSIZE SP_BUFSIZE

#ifndef DIRTHREAD_STKSIZ
#define	DIRTHREAD_STKSIZ	0 /* default size */
#endif /* DIRTHREAD_STKSIZ */

static capability	dircap;
static port		original_rootdir_checkfield;
static objnum		rootdir_objnum;

#define SP_ENTRIES	4
#define NCAPCACHE	4

sp_tab *	_sp_table;
int		_sp_entries;
capability	_sp_generic;
capset		_sp_rootdir;
port		_sp_getport;


long sp_time()
{
    return 0;
}

char *
sp_alloc(size, n)
unsigned size;
unsigned n;
{
    return (char *) calloc(n, size);
}

void
sp_begin(){}

void
sp_end(){}

errstat
sp_commit()
{
    sp_tab *st, *next_st;

    for (st = sp_first_update(); st != NULL; st = next_st) {
	next_st = sp_next_update();
	sp_free_mods(st);
    }

    sp_clear_updates();
    return STD_OK;
}

void
sp_abort()
{
    sp_tab *st, *next_st;

    for (st = sp_first_update(); st != NULL; st = next_st) {
	next_st = sp_next_update();
	(void) sp_undo_mod_dir(st);
        sp_free_mods(st);
    }

    sp_clear_updates();
}

errstat
sp_init()
{
    /* The # column names here must equal NROOTCOLUMNS in kerneldir.h */
    static char *colnames[] = { "owner", "group", "other", 0 };
    static int sp_initialized;
    errstat err;

    if (sp_initialized) {
	return STD_OK;
    }
    _sp_entries = SP_ENTRIES;
    _sp_table = (sp_tab *) calloc(SP_ENTRIES, sizeof(*_sp_table));
    if (_sp_table == 0 || !cc_init(NCAPCACHE)) {
	return STD_NOSPACE;
    }

    _sp_generic = dircap;
    _sp_getport = dircap.cap_port;
    priv2pub(&_sp_getport, &_sp_generic.cap_port);
    sp_initialized = 1;

    err = sp_create_rootdir(colnames, NROOTCOLUMNS,
				&original_rootdir_checkfield, &_sp_rootdir);
    if (err == STD_OK) {
	suite * s;

	s = &_sp_rootdir.cs_suite[0];
	assert(s->s_current);
	rootdir_objnum = prv_number(&s->s_object.cap_priv);
	assert(PORTCMP(&original_rootdir_checkfield,
			&s->s_object.cap_priv.prv_random));
    }
    return err;
}

errstat
dirappend(name, cap)
char *name;
capability *cap;
{
    capset cs;
    errstat err;
    static long cols[] = { 0xFF, 0, 0 };

    if (!cs_singleton(&cs, cap)) {
	return STD_NOSPACE;
    }

    err = sp_append(&_sp_rootdir, name, &cs, NROOTCOLUMNS, cols);
    if (err != STD_OK) {
	printf("dirappend: append failed for '%s': %s\n", name, err_why(err));
	stacktrace();
	exitkernel();
    }
    cs_free(&cs);
    return STD_OK;
}

errstat
dirnappend(name, n, cap)
char *name;
int n;
capability *cap;
{
    char buf[64];

    if (bprintf(buf, buf+64, "%s:%02X", name, n) == 0)
	return STD_NOMEM;
    return dirappend(buf, cap);
}

errstat
dirchmod(name, ncols, cols)
char *	name;
int	ncols;
long	cols[];
{
    return sp_chmod(&_sp_rootdir, name, ncols, cols);
}

errstat
dirnchmod(name, n, ncols, cols)
char *	name;
int	ncols;
long	cols[];
{
    char buf[64];

    if (bprintf(buf, buf+64, "%s:%02X", name, n) == 0)
	return STD_NOMEM;
    return sp_chmod(&_sp_rootdir, buf, ncols, cols);
}


/*
 * dirreplace allows a capability to be replaced in the kernel directory
 * so that we can have some attempt at security
 */
errstat
dirreplace(name, cap)
char * name;
capability * cap;
{
    capset cs;
    errstat err;

    if (!cs_singleton(&cs, cap)) {
	return STD_NOSPACE;
    }

    err = sp_replace(&_sp_rootdir, name, &cs);
    cs_free(&cs);
    return err;
}


/*
 * Two routines to support the kernel directory check field changing.
 * They are called from the syssvr.  They support the host reservation
 * system by allowing it to change its check field temporarily.
 */

int
dir_chgcap(checkfield)
port *	checkfield;
{
    sp_change_chkfield(rootdir_objnum, checkfield);
    _sp_rootdir.cs_suite[0].s_object.cap_priv.prv_random = *checkfield;

}


int
dir_restore_cap()
{
    sp_change_chkfield(rootdir_objnum, &original_rootdir_checkfield);
    _sp_rootdir.cs_suite[0].s_object.cap_priv.prv_random =
						original_rootdir_checkfield;
}


static void
dirthread _ARGS((void))
{
    register bufsize n;
    header hdr;
    static char inbuf[BUFFERSIZE], outbuf[BUFFERSIZE];

    for (;;)
    {
	hdr.h_port = dircap.cap_port;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, (bufptr) inbuf, BUFFERSIZE);
#else /* USE_AM6_RPC */
	n = getreq(&hdr, (bufptr) inbuf, BUFFERSIZE);
#endif /* USE_AM6_RPC */
	compare((short) n, >=, 0);
#ifdef NO_KERNEL_SECURITY
	/*
	** To protect the kernel directory from outsiders we strip
	** the delete and write rights bits if it is the kernel
	** directory
	*/
	hdr.h_priv.prv_rights &= ~(SP_DELRGT | SP_MODRGT);
#endif

	n = sp_trans(&hdr, inbuf, (int) n, outbuf, BUFFERSIZE);
	if (ERR_STATUS(n))
	{
	    hdr.h_status = n;
	    n = 0;
	}

#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, (bufptr) outbuf, n);
#else /* USE_AM6_RPC */
	putrep(&hdr, (bufptr) outbuf, n);
#endif /* USE_AM6_RPC */
    }
}


void
init_directory()
{
#ifndef NO_KERNEL_SECURITY
    kparam_port(&dircap.cap_port);
    kparam_chkf(&original_rootdir_checkfield);
#else
    kernel_port((char *) &dircap.cap_port);
    kernel_port((char *) &original_rootdir_checkfield);
#endif
    if (sp_init() != STD_OK)
	panic("sp_init failed");
}

void
init_dirtask()
{
    NewKernelThread(dirthread, (vir_bytes) DIRTHREAD_STKSIZ);
}

#endif /* ROMKERNEL */
