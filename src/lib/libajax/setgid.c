/*	@(#)setgid.c	1.3	94/04/07 09:50:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setgid() POSIX 4.2.2 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

int
setgid(gid)
gid_t gid;
{
  if(gid < 0)
	ERR(EINVAL, "setgid invalid gid");

  if(_ajax_euid == -1 || _ajax_gid == -1)
        _ajax_init_ids();

  if(_ajax_euid != SUPER_USER && _ajax_gid != gid)
	ERR(EPERM, "setgid denied");
	
  ERR(ENOSYS, "setgid not supported");
}
