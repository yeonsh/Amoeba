/*	@(#)sys_profile.c	1.4	96/02/27 11:19:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
sys_profile.c

Created Nov 6, 1991 by Philip Homburg

stub routines for the profile server.
*/

#include <stddef.h>

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <server/profile/profile.h>
#include <module/systask.h>

bufsize sys_profile(cap, buf, nbytes, interv)
capability *cap;
char *buf;
size_t nbytes;
unsigned long interv;
{
	header	hdr;
	bufsize	t;

	hdr.h_port = cap->cap_port;
	hdr.h_priv = cap->cap_priv;
	hdr.h_command = PROFILE_CMD;
	hdr.h_size= nbytes;
	hdr.h_offset = interv;

	t = trans(&hdr, NILBUF, 0, &hdr, buf, nbytes);
	if (ERR_STATUS(t))
		return ERR_CONVERT(t);

	return hdr.h_status;
}
