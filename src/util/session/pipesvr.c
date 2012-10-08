/*	@(#)pipesvr.c	1.6	96/02/27 13:14:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#define STATIC /* public */
/* pipe server, used to implement Posix pipe() function */

/* TO DO: check which of the following TO DO items are already done :-) */
/* TO DO: add proper locking */
/* TO DO: don't allow all threads to block in reading or writing */
/* TO DO: allow variable number of threads >= 2 */
/* TO DO: add capability random word checking? */
/* TO DO: change i to rw or 'side' and add #defines for RSIDE, WSIZE */
/* TO DO: add proper FIFO protocol */
/* TO DO: multiplex all pipes on one port (to save locate messages) */

/* Standard includes and definitions */
#include <amtools.h>
#include <file.h>
#include <semaphore.h>
#include <circbuf.h>
#include <ajax/mymonitor.h>
#include <module/rnd.h>
#include <module/buffers.h>
#include <module/proc.h>

#include "pipelink.h"
#include "share.h"
#include "session.h"

#ifndef STATIC
#define STATIC static
#endif

#define TRACE(x) MON_EVENT(x)
#define TRACENUM(x, y) MON_NUMEVENT(x, y)

#define REQBUFSIZE (29*1024) /* Request buffer size (max write request) */
#define PIPEBUFSIZE REQBUFSIZE /* Pipe buffer size (max buffered data) */
#define PIPESTACKSIZE (3000+PIPEBUFSIZE)

struct pipedata {
	port privport;		  /* Pipe server private port */
	capability cap; 	  /* Capability, to contact other threads */
	int servers;		  /* Number of serving threads */
	int unlinking;		  /* used in cleanup of pipe threads */
	int users[2];		  /* Number of clients for each end */
	struct circbuf *cb;	  /* Circular buffer */
	struct object *shared[2]; /* Shared object administration */
};

static port nullport; /* Constant zero */

/* Forward declarations */
STATIC void pipesvr();
STATIC void close_read_end();
STATIC void close_write_end();

int
startpipesvr(pid, cap0, cap1)
	/*in*/	int pid;
	/*out*/	capability *cap0, *cap1;
{
	struct pipedata *p;
	int err;
	
	MON_NUMEVENT("startpipeserver, pid", pid);
	p = (struct pipedata *)malloc(sizeof(struct pipedata));
	if (p == NULL) {
		MON_EVENT("startpipesvr: no mem for pipedata");
		free((_VOIDSTAR) p);
		return STD_NOSPACE;
	}
	p->cb = cb_alloc(PIPEBUFSIZE);
	if (p->cb == NULL) {
		MON_EVENT("startpipesvr: cb_alloc failed");
		free((_VOIDSTAR) p);
		return STD_NOSPACE;
	}
	uniqport(&p->privport);
	priv2pub(&p->privport, &cap0->cap_port);
	cap1->cap_port = cap0->cap_port;
	/* Read and write end are encoded as 1 and 2,
	   since object number 0 would confuse fstat (yuck!): */
	(void) prv_encode(&cap0->cap_priv, (objnum) 1, PRV_ALL_RIGHTS, &nullport);
	(void) prv_encode(&cap1->cap_priv, (objnum) 2, PRV_ALL_RIGHTS, &nullport);
	p->cap = *cap0;
	p->users[0] = p->users[1] = 1;
	p->unlinking = 0;
	p->servers = 0;
	if ((err = startpipethread(pipesvr, PIPESTACKSIZE, p)) < 0) {
		cb_free(p->cb);
		free((_VOIDSTAR) p);
		return err;
	}
	if (err = (startpipethread(pipesvr, PIPESTACKSIZE, p)) < 0) {
		(void) std_destroy(&p->cap); /* Tell first thread to go away */
		/* The last thread going away also frees p->cb and p */
		return err;
	}
	p->shared[0] = ob_new(pid, close_read_end, (char *)p);
	p->shared[1] = ob_new(pid, close_write_end, (char *)p);
	return STD_OK;
}

STATIC void
cleanup_threads(p)
	struct pipedata *p;
{
	/* To avoid pipe threads hanging around forever in case a program
	 * sharing the pipe dies without closing it, we do a dummy std_info
	 * transaction with one of the pipe threads.
	 * In the pipesvr mainloop it will find out it is no longer needed.
	 * It will also tell the other thread about it.
	 */
	char info[80];
	int len;
		    
	if (std_info(&p->cap, info, 80, &len) == STD_OK) {
		MON_EVENT("invited pipe thread to go away");
	} else {
		MON_EVENT("pipe thread didn't respond to std_info");
	}
}

STATIC void
close_read_end(p)
	struct pipedata *p;
{
	if (p->users[0] == 0) {
		warning("pipesvr: read end already closed");
	}
	else {
		MON_EVENT("pipesvr: really closing read end");
		p->users[0] = 0;
		cb_close(p->cb);
		if (p->users[1] == 0 && p->unlinking == 0) {
			/* let pipethreads find out they are no longer needed*/
			cleanup_threads(p);
		}
	}
}

STATIC void
close_write_end(p)
	struct pipedata *p;
{
	if (p->users[1] == 0) {
		warning("pipesvr: write end already closed");
	}
	else {
		MON_EVENT("pipesvr: really closing write end");
		p->users[1] = 0;
		cb_close(p->cb);
		if (p->users[0] == 0 && p->unlinking == 0) {
			/* let pipethreads find out they are no longer needed*/
			cleanup_threads(p);
		}
	}
}

/* The thread_newthread interface defines that the first argument passed to
   the function must be malloc'ed memory, and that it is automatically
   freed when the thread exits.  This means that we can't pass p
   directly as argument: it would be freed once for every exiting thread.
   herefore, for each thread, we allocate a small buffer in which we
   store p.  Ugly, but it works.
   The threads keep a server count, so the last one can free p. */

STATIC int
startpipethread(proc, stacksize, p)
	void (*proc)();
	int stacksize;
	struct pipedata *p;
{
	struct pipedata **pp;
	
	pp = (struct pipedata **) malloc(sizeof(struct pipedata *));
	if (pp == NULL) {
		MON_EVENT("startpipethread: no mem for pp");
		return STD_NOSPACE;
	}
	*pp = p;
	p->servers++;
	if (!thread_newthread(proc, stacksize, (char *)pp, (int)sizeof *pp)) {
		free((_VOIDSTAR) pp);
		p->servers--;
		MON_EVENT("startpipethread: can't start new thread");
		return STD_NOSPACE;
	}
	return STD_OK;
}

/* Strings returned for STD_INFO requests */
static char *info[2] = {
	"pipe read end in session svr",
	"pipe write end in session svr",
};

/* There is one server procedure which runs in at least two threads.
   There must be at least two threads serving input and two threads
   serving output: link/unlink requests may come in while another thread
   for the same end of the pipe is blocked waiting for the buffer to
   become full or empty.  But, assuming that at most one process
   performs reads on a pipe and at most one performs writes (at any one
   time), and observing that if the write end is blocked the read end
   won't block and vice versa, two threads that each serve both read and
   write requests are sufficient.  The read and write end are trivially
   distinguished by the object number, so read requests on the write end
   of the pipe or vice versa can be rejected. */

/*ARGSUSED*/
STATIC void
pipesvr(arg, size)
	char *arg;
	int size;
{
	struct pipedata *p = *(struct pipedata **)arg; /* Constant */
	header h;
	char buf[REQBUFSIZE];
	int i;
	bufsize n;
	errstat err;
	char *reply;
	int replylen;
	int pid;
	char *bufp,*end;
	
	while (p->users[0] + p->users[1] > 0) {
		h.h_port = p->privport;
		n = getreq(&h, buf, sizeof buf);
		if (ERR_STATUS(n)) {
			err = ERR_CONVERT(n);
			TRACENUM("pipesvr: getreq error", err);
			if (err == RPC_ABORTED)
				continue;
			break;
		}
		reply = NULL;
		replylen = 0;
		i = prv_number(&h.h_priv) - 1;
		if (i < 0 || i > 1)
			h.h_status = STD_CAPBAD;
		else
		switch (h.h_command) {
		
		case STD_INFO:
			MON_EVENT("pipesvr: info");
			reply = info[i];
			h.h_size = replylen = strlen(reply) + 1;
			h.h_status = STD_OK;
			break;
		
		case PIPE_UNLINK:
			if (n != CAPSIZE) {
				MON_EVENT("pipsvr: oldfashioned unlink");
				h.h_status = STD_ARGBAD;
				break;
			}
			pid = prv_number(&((capability *)buf)->cap_priv);
			if (i == 0) {
			    MON_NUMEVENT("pipesvr: unlink read end, pid", pid);
			}
			else {
			    MON_NUMEVENT("pipesvr: unlink write end, pid", pid);
			}

			/* cleanup_threads() not needed in this case */
			p->unlinking++;
			ob_unshare(p->shared[i], pid);
			p->unlinking--;
			h.h_status = STD_OK;
			break;
		
		case PIPE_LINK:
			if (n != CAPSIZE) {
				MON_EVENT("pipsvr: oldfashioned link");
				h.h_status = STD_ARGBAD;
				break;
			}
			pid = prv_number(&((capability *)buf)->cap_priv);
			if (i == 0) {
			    MON_NUMEVENT("pipesvr: link read end, pid", pid);
			}
			else {
			    MON_NUMEVENT("pipesvr: link write end, pid", pid);
			}
			ob_share(p->shared[i], pid);
			h.h_status = STD_OK;
			break;
		
		case STD_GETPARAMS:
			bufp = buf;
			end = buf + h.h_extra;

			MON_EVENT("pipsvr: STD_GETPARAMS");

			/* name\0 */
			bufp = buf_put_string(bufp,end,"buffer size");

			/* type\0 */
			bufp = buf_put_string(bufp,end,"integer");

			/* description\0 */
			bufp = buf_put_string(bufp,end,"size of the pipe buffer");

			/* value\0 */
			if ((bufp != NULL) && (end - bufp >= 10)) {
				sprintf(bufp, "%ld", PIPEBUFSIZE);

				/* skip value until after nul byte */
				bufp = strchr(bufp, '\0');
				bufp++;
			}

			if (bufp == NULL)
				h.h_status =  STD_NOSPACE;
			else
				h.h_status = STD_OK;

			replylen = h.h_extra =  bufp - buf;
			reply = buf;
			h.h_size = 1;
			break;

		case STD_DESTROY:
			MON_EVENT("pipesvr: destroy");
			p->users[0] = p->users[1] = 0;
			h.h_status = STD_OK;
			break;
		
		case FSQ_READ:
			if (p->users[1] == 0 && cb_full(p->cb) == 0) {
				MON_EVENT("pipesvr: read hits EOF right away");
				h.h_size = 0;
				h.h_extra = FSE_NOMOREDATA;
			}
			else {
				replylen = h.h_size;
				if (replylen > REQBUFSIZE)
					replylen = REQBUFSIZE;
				/* TO DO: use cb_getp here */
				TRACE("pipsvr read: call cb_gets");
				replylen = cb_gets(p->cb, buf,
					MIN(replylen, 1), replylen);
				TRACENUM("pipsvr read: cb_gets", replylen);
				reply = buf;
				h.h_size = replylen;
				/* Set (NO)MOREDATA.
				   This is no EOF flag, just an indicator of
				   immediate availability of more data.
				   EOF is indicated by returning 0 bytes. */
				if (cb_full(p->cb) == 0)
					h.h_extra = FSE_NOMOREDATA;
				else
					h.h_extra = FSE_MOREDATA;
			}
			h.h_status = STD_OK;
			break;
		
		case FSQ_WRITE:
			if (p->users[0] == 0) {
				MON_EVENT("pipesvr: write gets broken pipe");
				h.h_status = STD_NOTNOW; /* Broken pipe */
			}
			else {
				if (h.h_size > REQBUFSIZE) {
					h.h_size = REQBUFSIZE;
					TRACENUM("pipesvr write: truncated",
						 h.h_size);
				}
				else {
					TRACENUM("pipesvr write: size",
						 h.h_size);
				}
				cb_puts(p->cb, buf, h.h_size);
				TRACE("pipesvr write: cb_puts done");
				h.h_status = STD_OK;
			}
			break;
		
		default:
			MON_NUMEVENT("pipesvr: bad command", h.h_command);
			h.h_status = STD_COMBAD;
			break;
		
		}
		(void) putrep(&h, reply, (bufsize) replylen);
	}
	
	/* Kill other thread(s): each thread kills the next one,
	   except the last one, which frees the pipe data. */
	/* TO DO: add a mutex here */

	if (--p->servers == 0) {
		TRACE("pipesvr: last one, free(p)");
		cb_free(p->cb);
		free((_VOIDSTAR) p);
	}
	else {
		TRACE("pipesvr: killing other");
		(void) timeout((interval)5000);
		if ((err = std_destroy(&p->cap)) != STD_OK) {
			TRACENUM("pipesvr: other wouldn't die, err", err);
		}
	}
}
