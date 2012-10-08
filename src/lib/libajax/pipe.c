/*	@(#)pipe.c	1.2	94/04/07 09:49:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* pipe(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"
#include "sesstuff.h"

int
pipe(arg)
	int arg[2];
{
	capability *session;
	struct fd *f0, *f1;
	int i0, i1;
	/* (Hint: 0 is reader, 1 is writer, like 0=stdin, 1=stdout) */
	
	SESINIT(session);
	if ((i0 = _ajax_newfd(&f0)) < 0)
		return -1;
	if ((i1 = _ajax_newfd(&f1)) < 0) {
		JUNKNEWFD(i0, f0);
		return -1;
	}
	if (ses_pipe(session, &f0->fd_cap, &f1->fd_cap) < 0) {
		JUNKNEWFD(i0, f0);
		JUNKNEWFD(i1, f1);
		ERR(EIO, "pipe: ses_pipe failed");
	}
	f0->fd_flags = FREAD | FUNLINK;
	f0->fd_mode = _S_IFREG;
	f0->fd_size = -1;
	f0->fd_pos = 0;
	f0->fd_mtime = 0;
	f1->fd_flags = FWRITE | FUNLINK;
	f1->fd_mode = _S_IFREG;
	f1->fd_size = -1;
	f1->fd_pos = 0;
	f1->fd_mtime = 0;
	FD(i0) = f0;
	FD(i1) = f1;
	arg[0] = i0;
	arg[1] = i1;
	if (i0 < 3)
		_ajax_setstdcap(i0, &f0->fd_cap);
	if (i1 < 3)
		_ajax_setstdcap(i1, &f1->fd_cap);
	return 0;
}
