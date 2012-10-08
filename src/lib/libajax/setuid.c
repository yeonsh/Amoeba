/*	@(#)setuid.c	1.3	94/04/07 09:51:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* setuid() POSIX 4.2.2 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

int
setuid(uid)
uid_t uid;
{
  if(uid < 0)
	ERR(EINVAL, "setuid invalid uid");

  if(_ajax_euid == -1 || _ajax_uid == -1)
	_ajax_init_ids();

  if(_ajax_euid != SUPER_USER && _ajax_uid != uid)
	ERR(EPERM, "setuid denied");
	
  ERR(ENOSYS, "setuid not supported");
}
