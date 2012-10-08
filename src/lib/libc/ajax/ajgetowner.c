/*	@(#)ajgetowner.c	1.3	94/04/07 10:22:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Retrieve the process' owner.
   This is used mostly by the server that touches the session server,
   but also needed by close(). */

#include "ajax.h"

errstat
_ajax_getowner(owner_ret)
	capability *owner_ret;
{
	int size;
	capability self;
	
	if ((size = getinfo(&self, NILPD, 0)) < 0)
		return (errstat) size;
	return pro_getowner(&self, owner_ret);
}

int
_ajax_session_owner(owner)
	capability *owner;
/* Return in *owner the capability of the owner of this process,
 * if it happens to be the session server, and return 0. Otherwise return -1.
 */
{
	capability *session;

	if ((session = getcap("_SESSION")) == NULL) {
		return(-1);
	}
        if (_ajax_getowner(owner) != STD_OK) {
		return(-1);
	}
        if (!PORTCMP(&session->cap_port, &owner->cap_port)) {
		return(-1);
	}
	return(0);
}	
