/*	@(#)share.c	1.4	94/04/07 16:04:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Shared objects interface */

/* XXX make it fast when many processes and objects exist */
/* XXX finer locking granularity (only if sesimpl does it too) */
/* XXX add function to destroy object directly (why?) */

#include <amtools.h>
#include <ajax/mymonitor.h>

#include "session.h"
#include "share.h"

struct link {
	struct link *ln_next;
	int ln_pid;
};

static struct link *
newlink(pid, next)
	int pid;
	struct link *next;
{
	struct link *l;
	/*printf("newlink(pid=%d, next=%x)\n", pid, next);*/
	if ((l = (struct link *) malloc(sizeof(struct link))) == NULL) {
		warning("cannot allocate link for shared object");
		return NULL;
	}
	l->ln_next = next;
	l->ln_pid = pid;
	/*printf("newlink->l=%x\n", l);*/
	return l;
}

static struct object *objlist;
static mutex objmu;

struct object *
ob_new(pid, close, data)
	int pid;
	void (*close)();
	char *data;
{
	struct object *p;
	
	/*printf("ob_new(pid=%d, close=%x, data=%x)\n", pid, close, data);*/
	if ((p = (struct object *) malloc(sizeof(struct object))) == NULL) {
		warning("cannot allocate new shared object descriptor");
		return NULL;
	}
	if ((p->ob_link = newlink(pid, (struct link *)NULL)) == NULL) {
		free((_VOIDSTAR) p);
		return NULL;
	}
	p->ob_close = close;
	p->ob_data = data;
	mu_lock(&objmu);
	p->ob_next = objlist;
	objlist = p;
	mu_unlock(&objmu);
	/*printf("ob_new->p=%x\n", p);*/
	return p;
}

int
ob_share(p, pid)
	struct object *p;
	int pid;
{
	struct link *l;
	
	/*printf("ob_share(p=%x, pid=%d)\n", p, pid);*/
	if ((l = newlink(pid, p->ob_link)) == NULL)
		return STD_NOSPACE;
	p->ob_link = l;
	/*printf("ob_share->STD_OK\n");*/
	return STD_OK;
}

void
ob_allshare(pid, newpid)
	int pid;
	int newpid;
{
	struct object *p;
	struct link *l;
	
	/*printf("ob_allshare(pid=%d, newpid=%d)\n", pid, newpid);*/
	mu_lock(&objmu);
	for (p = objlist; p != NULL; p = p->ob_next) {
		for (l = p->ob_link; l != NULL; l = l->ln_next) {
			if (l->ln_pid == pid)
				ob_share(p, newpid);
		}
	}
	mu_unlock(&objmu);
	/*printf("ob_allshare->.\n");*/
}

static int
unshare(p, pid)
	struct object *p;
	int pid;
{
	struct link **pl, *l;
	
	/*printf("unshare(p=%x, pid=%d)\n", p, pid);*/
	for (pl = &p->ob_link; (l = *pl) != NULL; ) {
		if (l->ln_pid == pid) {
			*pl = l->ln_next;
			free((_VOIDSTAR) l);
		}
		else
			pl = &l->ln_next;
	}
	
	if (p->ob_link == NULL) {
		if (p->ob_close != NULL)
			(*p->ob_close)(p->ob_data);
		p->ob_close = NULL;
	/*printf("unshare->STD_OK\n");*/
		return STD_OK;
	}
	else {
	/*printf("unshare->STD_EXISTS\n");*/
		return STD_EXISTS;
	}
}

void
ob_unshare(p, pid)
	struct object *p;
	int pid;
{
	/*printf("ob_unshare(p=%x, pid=%d)\n", p, pid);*/
	if (unshare(p, pid) == STD_OK) {
		struct object **pp;
		struct object *q;
		mu_lock(&objmu);
		for (pp = &objlist; (q = *pp) != NULL; pp = &q->ob_next) {
			if (q == p) {
				*pp = q->ob_next;
				free((_VOIDSTAR) q);
				mu_unlock(&objmu);
				/*printf("ob_unshare->.\n");*/
				return;
			}
		}
		/* XXX should panic?!? */
		mu_unlock(&objmu);
		warning("ob_unshare: object not found for pid %d?!", pid);
	}
}

void
ob_allunshare(pid)
	int pid;
{
	struct object **pp;
	struct object *p;
	
	/*printf("ob_allunshare(pid=%d)\n", pid);*/
	mu_lock(&objmu);
	for (pp = &objlist; (p = *pp) != NULL; ) {
		if (unshare(p, pid) == STD_OK) {
			*pp = p->ob_next;
			free((_VOIDSTAR) p);
		}
		else
			pp = &p->ob_next;
	}
	mu_unlock(&objmu);
	/*printf("ob_allunshare->.\n");*/
}
