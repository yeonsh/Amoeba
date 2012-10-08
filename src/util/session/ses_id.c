/*	@(#)ses_id.c	1.3	96/02/27 13:14:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* last modified apr 20 93 Ron Visser */
#include <ajax/ajax.h>
#include <amtools.h>
#include <semaphore.h>
#include <circbuf.h>
#include <monitor.h>
#include <ampolicy.h>
#include <sys/types.h>
#include <pwd.h>

#include <sp_dir.h>

/* the server stores the user and group id of the owner,
 * these values are determined at boot time and never changed
 * if a process does an exec call the user and group id's are passed
 * via the environment, any changes due to setuid setgid or the
 * executuion of a set[ug]id program are made in the environment.
 */
uid_t uid;
gid_t gid;


#ifdef USE_PASSWD_FILE
static int
name2id(name, uid, gid)
char *name;
uid_t *uid;
gid_t *gid;
{
  struct passwd *pw;
  int succ= 0;

  if((pw = getpwnam(name)) != (struct passwd *) 0)
  {
	*uid = pw->pw_uid;
	*gid = pw->pw_gid;
	succ = 1;
  }else
	PUTS("session server: name2id: getpwnam failed");

  return succ;
}
#endif


static int
capequ(cap1, cap2)
capability *cap1, *cap2;
{
  objnum c1_num = prv_number(&cap1->cap_priv);
  objnum c2_num = prv_number(&cap2->cap_priv);

  return PORTCMP(&cap1->cap_port, &cap2->cap_port) && (c1_num == c2_num);
}


/* this function covers the capability to uid and gid translation
 * given a capability of a users home directory, it looks up
 * the users name in /users/public, this name is used to find
 * the uid and gid in /etc/passwd
 */
static int
cap2id(cap, uid, gid)
capability *cap;
uid_t *uid;
gid_t *gid;
{
  SP_DIR *dd;
  errstat err;
  capset cs, cs_entry;
  int i;
  capability entry;

  /*
   * HACK ALERT:
   * The following lookup appears useless but is needed when coldstarting.
   * There is a race and this kills off the race.  Sometimes the
   * session svr is started too early and cannot find the soap svr
   * which is still starting.  If this fails the next one certainly
   * won't since the delay is enough to let soap finish initializing.
   */
  (void) sp_lookup(SP_DEFAULT, "/", &cs);

  if((err = sp_lookup(SP_DEFAULT, DEF_USERDIR, &cs)) != STD_OK)
  {
	PUTNUM("cap2id: sp_lookup failed", (int) err);
	return 0;
  }

  if ((err = sp_list(&cs, &dd)) != STD_OK) 
  {
	PUTNUM("cap2id: sp_list failed", (int) err);
	return 0;
  }

  for(i = 0; i < dd->dd_nrows; i++)
  {
	if(sp_lookup(&cs,dd->dd_rows[i].d_name,&cs_entry) != STD_OK)
		continue;

	cs_to_cap(&cs_entry,&entry);
	cs_free(&cs_entry);

#ifdef USE_PASSWD_FILE
	if(capequ(cap,&entry) && name2id(dd->dd_rows[i].d_name,uid,gid))
	{
		cs_free(&cs);
		sp_closedir(dd);
		return 1;
	}
#else
	if (capequ(cap,&entry)) {
		/* For now, we'll avoid using the dummy /etc/passwd file
		 * for the purpose of determining the user id.  We'll just
		 * use the object number of the user's root cap instead.
		 * Instead of FAKE_GID, the gid could be based on the
		 * presence of /super and the rights of /public.
		 */
		*uid = prv_number(&cap->cap_priv);
		*gid = FAKE_GID;
		return 1;
	}
#endif
  }
  cs_free(&cs);
  sp_closedir(dd);
  return 0;
}



void
whoami()
{
  capability cap;

  if (name_lookup("/", &cap) != STD_OK)
  {
	uid = FAKE_UID + 2;
	gid = FAKE_GID;
	PUTS("name_lookup failed returning fake values");
	return;
  }
  if(!cap2id(&cap,&uid,&gid))
  {
	uid = FAKE_UID + 1;
	gid = FAKE_GID;
	PUTS("cap2id failed: returning fake values");
	return;
  }
}

