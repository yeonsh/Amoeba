/*	@(#)ajmakecapv.c	1.3	94/04/07 10:23:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Build capability list for exec'ed program, conveying information
   about open files etc.  This information is encoded in the capability
   environment, and parsed on entry to the new program by fdinit.
   
   For each open file, one or two entries are added to the capability
   environment, whose names encode information about the file:
   
	_f:<fd>:<flags>:<pos>:<size>:<mode>:<mtime>
		gives the flags etc. and the capability for the file;
	_b:<fd>:<name>
		gives the name of the file and the capability of the
		directory in which it was found, for bullet files;
	_d:<fd>:<fd2>
		means that <fd> is a dup'ed copy of <fd2>.
   
   This code assumes the world stays constant while we're running. */

#include "ajax.h"
#include "fdstuff.h"
#include <caplist.h>

extern struct caplist *capv;

/* Forward */
static char *addcnum();

void
_ajax_makecapv(nfd, fdlist, newcapv, maxcaps, buf, maxbuf)
	int nfd;
	int fdlist[];
	struct caplist *newcapv;
	int maxcaps;
	char *buf;
	int maxbuf;
{
	int i;
	char *name;
	char *bufend = buf + maxbuf;
	struct caplist *capvend = newcapv + maxcaps - 1;
	
	FDINIT();
	
	/* Copy the normal capability environment */
	for (i = 0; (name = capv[i].cl_name) != 0 && newcapv < capvend; ++i) {
		if (name[0] == 'S' && name[1] == 'T' && name[2] == 'D') {
			/* Look for STD{IN,OUT,ERR} */
			static char *stdname[3] = {"IN", "OUT", "ERR"};
			int j;
			for (j = 0; j < 3; ++j) {
				if (strcmp(name+3, stdname[j]) == 0)
					break;
			}
			if (j < 3) { /* Yes, it's one of those */
				struct fd *f;
				static capability nullcap;
				if (fdlist[j] < 0 || fdlist[j] >= NOFILE)
					newcapv->cl_cap = &nullcap;
				else {
					f = FD(fdlist[j]);
					if (f == NILFD || GETCLEXEC(f))
						newcapv->cl_cap = &nullcap;
					else {
						f = CLEANFD(f);
						newcapv->cl_cap = &f->fd_cap;
					}
				}
				newcapv->cl_name = capv[i].cl_name;
				newcapv++;
				continue;
			}
		}
		*newcapv++ = capv[i];
	}
	
	/* Add special entries for open files */
	for (i = 0; i < nfd && newcapv < capvend; ++i) {
		struct fd *f;
		char *p = buf;
		
		if (p + 50 > bufend) {
TRACE("makecapv no space for _f");
			break; /* out of buf space -- sorry */
		}
		if (fdlist[i] < 0 || fdlist[i] >= NOFILE)
			continue;
		f = FD(fdlist[i]);
		if (f == NILFD || GETCLEXEC(f))
			continue;
		f = CLEANFD(f);
		if (f->fd_refcnt > 1) {
			int j;
			
			for (j = 0; j < i; ++j) {
				if (f == FD(j))
					break;
			}
			if (j < i) {
				*p++ = '_';
				*p++ = 'd';
				p = addcnum(p, (long)i);
				p = addcnum(p, (long)j);
				*p++ = '\0';
				newcapv->cl_name = buf;
				newcapv->cl_cap = &f->fd_cap;
				++newcapv;
				buf = p;
				continue;
			}
		}
		*p++ = '_';
		*p++ = 'f';
		p = addcnum(p, (long)i);
		p = addcnum(p, f->fd_flags);
		p = addcnum(p, f->fd_pos);
		p = addcnum(p, f->fd_size);
		p = addcnum(p, (long)f->fd_mode);
		p = addcnum(p, f->fd_mtime);
		*p++ = '\0';
		newcapv->cl_name = buf;
		newcapv->cl_cap = &f->fd_cap;
		++newcapv;
		buf = p;
		if ((f->fd_flags & FBULLET) && (f->fd_flags & FWRITE)) {
			if (newcapv >= capvend) {
TRACE("makecapv no space for newcapv");
				break;
			}
			if (p + 20 + strlen(f->fd_name) > bufend) {
TRACE("makecapv no space for _b");
				break; /* out of buf space -- sorry */
			}
			*p++ = '_';
			*p++ = 'b';
			p = addcnum(p, (long)i);
			*p++ = ':';
			strcpy(p, f->fd_name);
			p = strchr(p, '\0');
			*p++ = '\0';
			newcapv->cl_name = buf;
			newcapv->cl_cap = &f->fd_dircap;
			++newcapv;
			buf = p;
		}
	}
	
	/* Sentinel */
	newcapv->cl_name = NULL;
	newcapv->cl_cap = NILCAP;
}

/* Add a number, return end of string -- recursive */

static char *
addnum(p, x)
	char *p;
	long x;
{
	if (x >= 10)
		p = addnum(p, x/10);
	*p++ = '0' + (x%10);
	return p;
}

/* Add a colon and a number, return end of string */

static char *
addcnum(p, x)
	char *p;
	long x;
{
	*p++ = ':';
	if (x < 0) {
		*p++ = '-';
		x = -x;
	}
	return addnum(p, x);
}
