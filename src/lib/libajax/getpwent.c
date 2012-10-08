/*	@(#)getpwent.c	1.4	96/02/27 10:59:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * get entry from password file
 *
 * By Patrick van Kleef
 *
 */


#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#define  _POSIX_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amoeba.h>
#include <ampolicy.h>
#include <module/prv.h>
#include <module/name.h>
#include <capset.h>
#include <soap/soap.h>
#include <sp_dir.h>
#include <ajax/ajax.h>

#define PRIVATE static
#define	ENVIRON	"/Environ"


PRIVATE char  _pw_file[] = "/etc/passwd";
PRIVATE char  _pwbuf[256];
PRIVATE char  _buffer[1024];
PRIVATE char *_pnt;
PRIVATE char *_buf;
PRIVATE int   _pw = -1;
PRIVATE int   _bufcnt;
PRIVATE struct passwd    _pwd;

setpwent() 
{
	if (_pw >= 0)
		lseek (_pw, 0L, 0);
	else
		_pw = open (_pw_file, 0);

	_bufcnt = 0;
	return (_pw);
}


endpwent () 
{
	if (_pw >= 0)
		close (_pw);

	_pw = -1;
	_bufcnt = 0;
}

static getline () 
{
	if (_pw < 0 && setpwent () < 0)
		return (0);
	_buf = _pwbuf;
	do {
		if (--_bufcnt <= 0){
			if ((_bufcnt = read (_pw, _buffer, 1024)) <= 0)
				return (0);
			else
				_pnt = _buffer;
		}
		*_buf++ = *_pnt++;
	} while (*_pnt != '\n');
	_pnt++;
	_bufcnt--;
	*_buf = 0;
	_buf = _pwbuf;
	return (1);
}

static skip_period () 
{
	while (*_buf != ':')
		_buf++;

	*_buf++ = '\0';
}

static struct passwd  *_scanpwent()
{
	_pwd.pw_name = _buf;
	skip_period ();
	_pwd.pw_passwd = _buf;
	skip_period ();
	_pwd.pw_uid = atoi (_buf);
	skip_period ();
	_pwd.pw_gid = atoi (_buf);
	skip_period ();
	_pwd.pw_gecos = _buf;
	skip_period ();
	_pwd.pw_dir = _buf;
	skip_period ();
	_pwd.pw_shell = _buf;

	return (&_pwd);
}

struct passwd  *getpwent () 
{
	if (getline () == 0)
		return (0);
	return _scanpwent();
}


static int
same_user(c1, c2)
capability *	c1;
capability *	c2;
{
    objnum n1, n2;

    if (PORTCMP(&c1->cap_port, &c2->cap_port))
    {
	n1 = prv_number(&c1->cap_priv);
	n2 = prv_number(&c2->cap_priv);
	return n1 == n2;
    }
    return 0;
}


/*
 * See if user_cap is the current user. If so then look in Environ file
 * for the password.  If it is any other user then return '*'.
 */
static void
getenviron(user_cap, pw)
capability * user_cap;
char * pw;
{
    capability * env_cap;
    capability envfile;
    char * fp;
    char * buf;
    char * fend;
    b_fsize size;
    b_fsize nread;

    pw[0] = '*';
    pw[1] = '\0';
    env_cap = getcap("ROOT");
    if (env_cap != 0 && same_user(env_cap, user_cap)) {
	/* Look up the /Environ file */
	if (name_lookup(ENVIRON, &envfile) != STD_OK)
	    return;
	if (b_size(&envfile, &size) != STD_OK || size <= 0)
	    return;
	if ((fp = (char *) malloc((size_t) size)) == NULL)
	    return;
	if (b_read(&envfile, (b_fsize) 0, fp, size, &nread) != STD_OK ||
								nread != size)
	{
	    free((_VOIDSTAR) fp);
	    return;
	}
	buf = fp;
	fend = fp + size;
	while (fp + 6 < fend)
	{
	    if (strncmp("passwd", fp, 6) == 0)
	    {
		/* Found it */
		fp += 6;
		while (*fp == ' ' || *fp == '\t')
		    fp++;
		while (*fp != '\n')
		    *pw++ = *fp++;
		*pw = '\0';
		free((_VOIDSTAR) buf);
		return;
	    }
	    /* else */
	    while (fp < fend && *fp != '\n')
		fp++;
	}
	free((_VOIDSTAR) buf);
	return;
    }
}


struct passwd  *getpwnam (name)
char   *name;
{
	struct passwd  *pwd;

	setpwent ();
	while ((pwd = getpwent ()) != 0)
		if (!strcmp (pwd -> pw_name, name))
			break;
	endpwent ();
	if (pwd != 0)
		return (pwd);
	else {
		char user_path[PATH_MAX];
		capability user_cap;
		char * password[9];

		/* Not in the passwd file; try to look up this user */
		sprintf(user_path, "%s/%s", DEF_USERDIR, name);
		if (name_lookup(user_path, &user_cap) == STD_OK) {
		    getenviron(&user_cap, password);
		    /* create a passwd file entry in _buf and scan it */
		    sprintf(_buf, "%s:%s:%d:%d:%s:%s:/bin/sh",
			    name, password,
			    (int) prv_number(&user_cap.cap_priv),
			    (int) FAKE_GID, name, user_path);
		    return _scanpwent();
		} else {
		    return NULL;
		}
	}
}


#ifdef __STDC__
static struct passwd *find_user(uid_t uid)
#else
static struct passwd *find_user(uid) uid_t uid;
#endif
{
    SP_DIR *dd;
    capset  cs;
    int     i;
    struct passwd *user_pwd = NULL;

    /* The session server uses the object number of the user's root
     * directory as uid, so here we just scan /public/users until we
     * find the corresponding id.  TODO: in most cases, getpwuid will
     * be called for the user itself.  Trying environment variable USER
     * first may save us several RPCs.
     */
    if (sp_lookup(SP_DEFAULT, DEF_USERDIR, &cs) != STD_OK) {
	return NULL;
    }

    if (sp_list(&cs, &dd) != STD_OK) {
	return NULL;
    }

    for (i = 0; i < dd->dd_nrows; i++) {
	char *this_user = dd->dd_rows[i].d_name;
	uid_t this_uid;
	capset cs_entry;
	capability entry;
	char password[9];

	if (sp_lookup(&cs, this_user, &cs_entry) != STD_OK)
	    continue;

	cs_to_cap(&cs_entry, &entry);
	cs_free(&cs_entry);

	this_uid = (uid_t) prv_number(&entry.cap_priv);
	if (this_uid == uid) {
	    /* create a default passwd file entry in _buf and scan it */
	    getenviron(&entry, password);
	    sprintf(_buf, "%s:%s:%d:%d:%s:%s/%s:/bin/sh",
		    this_user, password, (int) this_uid, (int) FAKE_GID,
		    this_user, DEF_USERDIR, this_user);
	    user_pwd = _scanpwent();
	    break;
	}
    }
    cs_free(&cs);
    sp_closedir(dd);
    return user_pwd;
}

#ifdef __STDC__
struct passwd  *getpwuid (uid_t uid)
#else
struct passwd  *getpwuid (uid) uid_t uid;
#endif
{
	struct passwd  *pwd;

	setpwent ();
	while ((pwd = getpwent ()) != 0)
		if (pwd -> pw_uid == uid)
			break;
	endpwent ();
	if (pwd != 0)
		return (pwd);
	else {
		/* not in the passwd file; scan /public/users instead */
		return find_user(uid);
	}
}
