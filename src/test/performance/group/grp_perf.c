/*	@(#)grp_perf.c	1.5	96/02/27 10:56:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <group.h>
#include <stderr.h>
#include <stdio.h>
#include <assert.h>
#include <caplist.h>
#include <fcntl.h>
#include <server/ax/server.h>
#include <semaphore.h>
#include <thread.h>
#include <module/buffers.h>
#include <module/syscall.h>
    

/* Program to test the performance of group communication. It should be
 * started by gax.
 */

/* Values stored in h_command: */
#define JOIN	1
#define DATA	2
#define LEAVE   3
#define RESET	4
#define FINISH  5
    
/* Note: the group code itself also uses some bytes from 1 << LOGDATA
 * to send over a header.  For safety we subtract 64.
 */
#define GRP_HEADER_SIZE	64
#define LOGDATA 	17		/* 2 log maximum size msg */
#define MAXDATA		((1 << LOGDATA) - GRP_HEADER_SIZE)  /* max size msg */

#define MAXGROUP 	128
#define LOGBUF 		7		/* 2 log buffers */
#define REPLYTIMEOUT	1000		/* timeout before retrying sending */
#define SYNCTIMEOUT	8000		/* time between synchronization of
					 * all the group members.
					 */
#define STACKSIZE	10000

char sbuf[MAXDATA];			/* buffer used for sending */
char rbuf[MAXDATA];			/* buffer used for receiving */
port group;				/* group port; extracted from cap env */
capability *groupcap, *me;		/* pointers in group env */
long large = 0;				/* method 1 or method 2 */
g_indx_t ncpu = 0;			/* total number of group members */
g_indx_t cpu = 0;			/* my cpu number */
int nsender = 0;			/* number of senders */
int size = 0;				/* size of the msg to be sent */
int nmess = 0;				/* total # msg to be sent */
g_id_t gd;
int MoreMess = 0;
int SenderWaiting = 0;
int resilience = 0;			/* resilience degree */
int checking = 0;			/* check contents of receive buffer? */
int recovery = 0;			/* recover after failure? */
int debug_level = 0;			/* HACK to set debug level in kernel */

g_indx_t memlist[MAXGROUP];		/* list of members */
g_indx_t member[MAXGROUP];		/* same list; use after failure */
g_indx_t total;				/* total number of members */
int memstate[MAXGROUP];			/* state of each member: JOIN etc. */
grpstate_t state;			/* to be passed to grp_info */
g_index_t my_id;			/* my member id */
static int got_msg[MAXGROUP];		/* #messages we got from each member */
static int n_total = 0;

semaphore sem_recovery;			/* synchronization var for recovery */
semaphore sem_done;			/* synchronization var for termination
					*/

char *progname;	
FILE *file = stdout;

static void panic(str)
    char *str;
{
    fprintf(file, "%d: PANIC: %s\n", cpu, str);
    exit(1);
}


static void checkbuf(b, cnt)
    char *b;
    int   cnt;
{
    int32 i;
    int32 tmp;
    char *end = b + cnt;

    for(i=0; i < cnt / sizeof(int32); i++) {
	b = buf_get_int32(b, end, &tmp);
	if(tmp != i) {
	    fprintf(file, "cpu: %d wrong: %d %d\n", cpu, i, tmp);
	}
    }
}


static void printstate(state, member)
    grpstate_p state;
    g_indx_t *member;
{
    int i;
    
    fprintf(file, "%d: group size %d; members: ", cpu, state->st_total);
    for(i=0; i < state->st_total; i++)
	fprintf(file, "%d ", member[i]);
    fprintf(file, "\n");
}


static void rebuildgroup(state, member)
    grpstate_p state;
    g_indx_t *member;
{
    int new_n_total = 0;
    int i, j;
    
    fprintf(file, "%d: rebuild group: %d\n", cpu, state->st_total);
    total = state->st_total;
    for(i=0; i < MAXGROUP; i++) {
	if(memstate[i] == JOIN || memstate[i] == FINISH) {
	    for(j = 0; j < state->st_total; j++)
		if(member[j] == i) break;
	    if(j == state->st_total) {
		fprintf(file, "%d died\n", i);
		memstate[i] = RESET;
	    }
	} else if (memstate[i] == RESET) {
	    fprintf(file, "%d already dead\n", i);
	}
	if (memstate[i] == RESET && got_msg[i] > 0) {
	    /* pretend we got all messages from the crashed member */
	    new_n_total += nmess;
	} else {
	    new_n_total += got_msg[i];
	}
    }
    fprintf(file, "%d: n_total was %ld, now %ld\n", cpu, n_total, new_n_total);
    n_total = new_n_total;
    sema_up(&sem_recovery);
}

static handle(hdr, buf, cnt)
    header_p hdr;
    char *buf;
    int   cnt;
{
    errstat r;

    /* printf("handle %d from %d\n", hdr->h_command, hdr->h_size);
     */
    switch(hdr->h_command) {
    case JOIN:
	r = grp_info(gd, &group, &state, memlist, (g_indx_t) MAXGROUP);
	if(ERR_STATUS(r)) {
	    fprintf(file, "%d: handle: grp_info failed %d\n", cpu,
		    ERR_CONVERT(r));
	}
	break;
    case DATA:
	if(checking) checkbuf(buf, cnt);
	if (hdr->h_extra < ncpu) {
	    if (got_msg[hdr->h_extra] != hdr->h_offset) {
		fprintf(file, "%d: %d sent seqno %ld, expected %ld\n",
			cpu, hdr->h_extra, hdr->h_offset,
			got_msg[hdr->h_extra]);
	    }
	    got_msg[hdr->h_extra] = hdr->h_offset + 1;
	} else {
	    fprintf(file, "%d: handle: message from %d?!\n",
		    cpu, hdr->h_extra);
	}
	break;
    case LEAVE:
	break;
    case RESET:
	r = grp_info(gd, &group, &state, member, (g_indx_t) MAXGROUP);
	if(ERR_STATUS(r)) {
	    fprintf(file, "%d: handle: grp_info failed %d\n", cpu,
		    ERR_CONVERT(r));
	}
	printstate(&state, member);
	rebuildgroup(&state, member);
	break;
    case FINISH:
	memstate[hdr->h_size] = FINISH;
	break;
    default:
	fprintf(file, "unknown type %d\n", hdr->h_command);
	break;
    }
}

/*ARGSUSED*/
static void daemon(param, psize)
    char *param;
    int psize;
{
    bufsize s;
    header rhdr;
    int totalmsg = nsender * nmess;
    errstat r;
    
    rhdr.h_port = group;
    while(1) {
	s = grp_receive(gd, &rhdr, (bufptr) rbuf, (uint32) size, &MoreMess);
	if(ERR_STATUS(s)) {
	    fprintf(file, "%d: receive failed: %d\n", cpu, ERR_CONVERT(s));
	    if(recovery) {
		rhdr.h_port = group;
		rhdr.h_command = RESET;
		rhdr.h_size = cpu;
		rhdr.h_extra = my_id;
		r = grp_reset(gd, &rhdr, (g_indx_t) 1);
		if(ERR_STATUS(r)) {
		    fprintf(file, "%d: daemon: grp_reset failed %d\n", cpu, 
			   ERR_CONVERT(r));
		    exit(1);
		}
	    } else {
		exit(1);
	    }
	} else {
	    if(s != size) {
		if(rhdr.h_command != LEAVE && rhdr.h_command != RESET &&
		   rhdr.h_command != JOIN)
		    fprintf(file, "%d: WARNING: received %s bytes (%d)\n", cpu,
			    s, rhdr.h_command);
	    }
	    if(checking || recovery) {
		handle(&rhdr, rbuf, size);
	    }
	    n_total++;
	    if(n_total >= totalmsg) sema_up(&sem_done); /* work is done. */
	    if(rhdr.h_command == LEAVE && rhdr.h_size == cpu) {
		thread_exit();
	    }
	}
    }
}


test()
{
    long ms, ms1;
    int i, more, r, s;
    errstat res;
    int32 c;
    header hdr;
    char *b;
    
    hdr.h_command = JOIN;
    hdr.h_size = cpu;
    hdr.h_port = group;
    hdr.h_extra = -1;	/* member id not known yet */

    /* Initialize send buffer. */
    b = sbuf;
    for(c=0; c < size/sizeof(int32); c++) {
	b = buf_put_int32(b, sbuf + MAXDATA, c);
    }
    assert(b);

    /* Start group. */
    if(cpu == 0) {
	uint32 logdata = 1;

	/* determine log of data buffer size required */
	while ((1 << logdata) < size + GRP_HEADER_SIZE) {
	    logdata++;
	}
	/* kernel forces the next lowerbound: */
	if (logdata < 10) {
	    logdata = 10;
	}
	gd = grp_create(&group, (g_indx_t) resilience, ncpu,
			(uint32) LOGBUF, logdata);
	if(gd < 0) {
	    fprintf(file, "create failed (%d)\n", (int) gd);
	    exit(1);
	}
    } else {
	if((gd=grp_join(&hdr)) < 0) {
	    fprintf(file, "%d: join failed %d\n", cpu, gd);
	    exit(1);
	}
    }
    /* Set some default values; alive timeout value is overloaded with debug.
     * (this convenient for kernel debugging).
     */
    res = grp_set(gd, &group, (interval) SYNCTIMEOUT, (interval) REPLYTIMEOUT,
		  (interval) debug_level, (uint32) large);
    if(ERR_STATUS(res)) {
	fprintf(file, "%d: test: grp_set failed %d\n", cpu, ERR_CONVERT(res));
    }

    /* Find out who is in the group. */
    res = grp_info(gd, &group, &state, memlist, (g_indx_t) MAXGROUP);
    if(ERR_STATUS(res)) {
	fprintf(file, "%d: test: grp_info failed %d\n", cpu, ERR_CONVERT(res));
    }
    my_id = state.st_myid;
    
    /* Wait until the group is complete. */
    while (state.st_total < ncpu) {
	r = grp_receive(gd, &hdr, (bufptr) 0, (uint32) 0, &more);
	if(ERR_STATUS(r)) {
	    fprintf(file, "%d: test: grp_receive failed: %d\n", cpu,
		    ERR_CONVERT(r));
	    exit(1);
	}
	handle(&hdr, (char *) 0, 0);
    }
    /* Group is now complete */

    total = state.st_total;
    assert(total == ncpu);
    for(i=0; i < ncpu; i++) memstate[i] = JOIN;
    SenderWaiting = 0;
    MoreMess = 0;

    thread_newthread(daemon, STACKSIZE, (char *) 0, 0);

    /* Run test */
    if(cpu >= ncpu - nsender) {
	hdr.h_command = DATA;
	hdr.h_size = cpu;
	hdr.h_extra = my_id;
	ms = sys_milli();
	for(i=0; i < nmess; i++) {
	    hdr.h_offset = i;
	    s = grp_send(gd, &hdr, (bufptr) sbuf, (uint32) size);
	    if(ERR_STATUS(s)) {
		fprintf(file, "%d: test: grp_send failed %d\n", cpu,
			ERR_CONVERT(s));
		if(s == BC_ABORT || s == BC_FAIL) {
		    sema_down(&sem_recovery);
		    i--;	/* retry the last one */
		}
	    }
	    /* Give the daemon thread a change to run. */
	    threadswitch();
	}
	ms1 = sys_milli();
	if (ms1 > ms) {
	    fprintf(file, "%d: time %d msec; throughput %.0f bytes/sec\n",
		    cpu, ms1 - ms,
		    (((float) (size * nmess))/(ms1 - ms) * 1000));
	} else {
	    fprintf(file, "%d: unmeasurable througput; ", cpu);
	    fprintf(file, "increase size and/or nmess\n");
	}
    }

    /* Wait until all the work is done. */
    sema_down(&sem_done);
    
    /* leave the group. */
    hdr.h_command = LEAVE;
    hdr.h_size = cpu;
    hdr.h_extra = my_id;
    s = grp_leave(gd, &hdr);
    if(ERR_STATUS(s)) {
	fprintf(file, "%d: test: grp_send failed %d\n", cpu, ERR_CONVERT(s));
	if(s == BC_ABORT) sema_down(&sem_recovery);
    }
    /* Give the daemon thread a change to run. */
    
}


static void usage()
{
    fprintf(stderr, "Usage: gax %s ncpu nsender size nmess large resilience\
 checking recovery debug_level\n", progname);
}


main(argc, argv)
    int argc;
    char **argv;
{
    extern struct caplist *capv;
    capability *filecap;
    int fd;
    
    progname = argv[0];
    if (argc != 11) {
	usage();
	exit(1);
    }
    cpu = atoi(argv[1]);
    ncpu = atoi(argv[2]);
    nsender = atoi(argv[3]);
    size = atoi(argv[4]);
    nmess = atoi(argv[5]);
    large = atoi(argv[6]);
    resilience = atoi(argv[7]);
    checking = atoi(argv[8]);
    recovery = atoi(argv[9]);
    debug_level = atoi(argv[10]);
    if (ncpu < 1 || ncpu > MAXGROUP) {
	printf("ncpu (%d) must be in range 1..%d\n", ncpu, MAXGROUP);
	exit(1);
    }
    if (nsender < 1 || nsender > ncpu) {
	printf("nsender (%d) must be in range 1..ncpu (%d)\n", nsender, ncpu);
	exit(1);
    }
    if (size > MAXDATA) {
	printf("size (%d) must be smaller than %d\n", size, MAXDATA);
	exit(1);
    }
    if (resilience < 0 || resilience > ncpu) {
	printf("resilience (%d) must be in range 1..ncpu (%d)\n",
	       resilience, ncpu);
	exit(1);
    }
    sema_init(&sem_recovery, 0);
    sema_init(&sem_done, 0);

    if((groupcap = getcap(GROUPCAP)) == NULL)
	panic("no GROUPCAP in cap env (use gax)");
    group = groupcap->cap_port;
    if((me = getcap(MEMBERCAP)) == NULL)
	panic("no MEMBERCAP in cap env (use gax)");

    /* to make sure that I/O redirection works under UNIX */
    if((filecap = getcap("STDOUT")) == NULL)
	panic("no STDOUT capability");
    if((fd = opencap(filecap, O_RDWR)) < 0)
	panic("opencap failed");
    if((file = fdopen(fd, "w")) == NULL)
	panic("fdopen failed");
    setvbuf(file, (char *) NULL, _IOLBF, 0);

    test();
    fflush(file);
    thread_exit();
    /*NOTREACHED*/
}
