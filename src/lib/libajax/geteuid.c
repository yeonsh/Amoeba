/*	@(#)geteuid.c	1.3	94/04/07 09:43:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* geteuid() POSIX 4.2.1 
 * last modified apr 22 93 Ron Visser
 */

#include "ajax.h"

uid_t
geteuid()
{

  if(_ajax_euid == -1) 
	_ajax_init_ids();

  return _ajax_euid;
}
