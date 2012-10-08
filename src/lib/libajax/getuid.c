/*	@(#)getuid.c	1.3	94/04/07 09:47:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* getuid() POSIX 4.2.1 
 * last modified apr 20 93 Ron Visser 
 */

#include "ajax.h"
#include "sesstuff.h"

#define SETIFUNSET(old,new)	(old == -1 ? old = new : old)

uid_t
getuid()
{

  if(_ajax_uid == -1) 
	(void) _ajax_init_ids();

  return _ajax_uid;
}

int
_ajax_init_ids()
{
/* this function initializes the [ug]id world from the environment
 * variable _IDS if it is set, otherwise the session server is consulted.
 * the environment contains a string of four numbers, 
 * resp uid, gid, euid, egid.
 */ 
  capability session;
  uid_t uid, euid;
  gid_t gid, egid;
  char *id_str;

  if((id_str = getenv(ID_ENV)) == NULL
  || sscanf(id_str,ID_TEMPL,&uid,&gid,&euid,&egid) != 4)
  {
	PUTS(id_str == NULL ? "no ids in environ" : "illegal ids in environ");
	_ajax_session_owner(&session);
	if(ses_getid(&session,&uid,&euid,&gid,&egid) != STD_OK)
	{
		PUTS("ses_getid failed");
		uid = FAKE_UID;
		euid = uid;
		gid = FAKE_GID;
		egid = gid;
	}
  }

  SETIFUNSET(_ajax_uid,uid);
  SETIFUNSET(_ajax_euid,euid);
  SETIFUNSET(_ajax_gid,gid);
  SETIFUNSET(_ajax_egid,egid);
}
