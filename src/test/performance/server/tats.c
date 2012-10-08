/*	@(#)tats.c	1.4	96/02/27 10:57:35 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* TATS -- Time Amoeba Thread Switches */

/* TO DO:
	organize code better
	adjust scale factors to VAX figures
	measure both one or two server threads
	measure transactions with large data buffer
*/

/* Configuration: */
#define SYS_MILLI	/* Use sys_milli syscall for timing */

/* Determine which of the non-trivial tests to run: */
#define TEST_SYNCH	/* Test thread synchronisation */
#define TEST_SWITCH	/* Test thread switching overhead */
#define TEST_THREAD	/* Test sys_newthread */
#define TEST_MOD	/* Test thread_newthread */
#define TEST_TRANS	/* Test intra-process transactions */
#define TEST_2TRANS	/* Test inter-process transactions */
#define TEST_BULLET	/* Test bullet server speed */

/* Include files: */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <amtools.h>
#include <ampolicy.h>
#include <stderr.h>
#include "bullet/bullet.h"
#include <caplist.h>
#include <module/proc.h>
#include <module/host.h>
#include <module/rnd.h>
#include <module/crypt.h>
#include <thread.h>

#ifndef SYS_MILLI
#include <server/tod/tod.h>
#include <module/tod.h>
#endif

/* A few forward declarations */
#ifdef TEST_SYNCH
void	test_thread_synch();
#endif
#ifdef TEST_SWITCH
void	test_thread_switch();
#endif
#ifdef TEST_TRANS
void	test_trans();
#endif
#ifdef TEST_2TRANS
void	test_2_trans();
#endif
#ifdef TEST_BULLET
void	test_bullet();
#endif

/* MAIN */

int multiplier = 1; /* Number of times to slow down the loops; from argv[1] */

char *progname; /* Program name for error messages */

char *peer_host; /* Optional machine to run inter-process trans test peer */

main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	
	progname = argv[0];
	
	while ((c = getopt(argc, argv, "@im:")) != EOF) {
		switch (c) {
		
		case 'i':
			printf("Type CR to proceed: ");
			while ((c = getchar()) != '\n' && c != EOF)
				;
			break;
		
		case 'm':
			peer_host = optarg;
			break;
		
#ifdef TEST_2TRANS
		case '@':
			server_process();
			exitthread((long *) 0);
			/*NOTREACHED*/
#endif
		
		default:
			fprintf(stderr,
				"usage: %s [-i] [-m machine] [multiplier]\n",
				progname);
			exit(2);
			/*NOTREACHED*/
		
		}
	}
	
	/* Get multiplier from remaining argument */
	if (optind < argc)
		multiplier = atoi(argv[optind]);
	
	timeout(8000L);
	
	test_getms();
	test_null();
	test_call();
	test_timeout();
	test_one_way();
#ifdef TEST_SYNCH
	test_thread_synch();
#endif
#ifdef TEST_SWITCH
	test_thread_switch();
#endif
	test_mutex();
#ifdef TEST_THREAD
	test_newthread();
#endif
#ifdef TEST_MOD
	test_modthread();
#endif
#ifdef TEST_TRANS
	test_trans();
#endif
#ifdef TEST_2TRANS
	test_2_trans();
#endif
#ifdef TEST_BULLET
	test_bullet();
#endif
	
	printf("\nTests done.\n");
	exit(0);
	/*NOTREACHED*/
}


/* TIMER CODE */

/* Get a time in milliseconds.
   This wraps around every 49 days or so, but since the only use is to
   subtract values gotten from the same clock, properties of 2's
   complement arithmetic guarantee that intervals have the correct sign
   (if they are shorter than 49/2 days :-).
   
   (NB: I have fixed the tod server interface for this to actually
   return the milliseconds (in h_status), see tod.h.) */

unsigned long
getms()
{
#ifdef SYS_MILLI
	return sys_milli();
#else
	long seconds;
	int ms, tz, dst;
	int err;
	
	if ((err = tod_gettime(&seconds, &ms, &tz, &dst)) < 0) {
		printf("tod_gettime failed\n");
		exit(1);
	}
	return seconds*1000 + ms;
#endif
}

/* Return milliseconds elapsed since previous call (or init_last_ms) */

unsigned long
elapsed_ms(last_ms)
unsigned long last_ms;
{
	unsigned long now, ms;
	
	now = getms();
	if (now < last_ms) {
		printf("time wrapped (now = %ld, last = %ld) -- exit.\n",
			now, last_ms);
		exit(1);
	}
	ms = now - last_ms;
	return ms;
}

/* Wait until the clock ticks.
   This is important if it has a low granularity. */

unsigned long
wait_for_ms()
{
	unsigned long last_ms, elapsed;

	last_ms = getms();
	while ((elapsed = elapsed_ms(last_ms)) == 0)
		;
	return last_ms + elapsed;
}


/* TESTING MACHINERY */

long fixed_overhead; /* Overhead for wait_for_ms and elapsed_ms calls */
long loop_overhead; /* Overhead for each time through the loop */

/* Report test outcome */

static mutex report_mu;

#define REPORT(what, number, ms) {				\
	mu_lock(&report_mu);					\
	printf("\n\t%d calls to %s took %d.%03d secs.\n",	\
	    number, what, (ms)/1000, (ms)%1000);		\
	if ((number) > 0)					\
	    printf("\t(average %d.%03d msecs/call)\n\n",	\
		(ms)/(number), (((ms)*1000)/(number)) % 1000);	\
	mu_unlock(&report_mu);					\
}

/* Turn a macro argument into a quoted string */

#ifdef __STDC__ /* ANSI C solution */
#define MKSTR(S) #S
#else /* Reiser CPP (v7 Unix) solution */
#define MKSTR(S) "S"
#endif

/* Generic test: print how long N executions of S take.
   GEN_TEST is what you normally would use; GEN_TEST2 excludes
   the outer braces, for the loop overhead calculation. */

#define GEN_TEST(N, S) { GEN_TEST2(N,S) }

#define GEN_TEST2(N, S)						\
	register int i = 0, n = (N)*multiplier;			\
	unsigned long t, last_ms;				\
	last_ms = wait_for_ms();				\
	do {							\
		S;						\
	} while (++i < n);					\
	t = elapsed_ms(last_ms);				\
	REPORT(MKSTR(S), n, t - fixed_overhead - n*loop_overhead);

#define GEN_CHECKED_TEST(N, S, R)	{			\
	register int i = 0, n = (N)*multiplier;			\
	unsigned long last_ms, t;				\
	last_ms = wait_for_ms();				\
	do {							\
		register long r;				\
		if ((r = S) != R) {				\
			printf("%s failed (%d)\n", MKSTR(S), r); \
			exit(1);				\
		}						\
	} while (++i < n);					\
	t = elapsed_ms(last_ms);				\
	REPORT(MKSTR(S), n, t - fixed_overhead - n*loop_overhead); \
}


/* INDIVIDUAL TESTS */

/* Test how many getms calls per second.
   This must be the first test, as it also sets the fixed overhead. */

test_getms()
{
	int gran;
	int el;
	int ms;
	int calls;
	unsigned long last_ms;
	
	mu_init(&report_mu);

	printf("Testing getms() (fixed overhead); wait...\n");
	
	gran = 32000;
	ms = 0;
	calls = 0;
	
	last_ms = wait_for_ms();
	
	/* Call getms until a given number of millisecs have been accumulated.
	   (The call to getms is implicit in the call to elapsed_ms.) */
	do {
		ms += (el = elapsed_ms(last_ms));
		if (el > 0 && el < gran)
			gran = el;
		last_ms += el;
		++calls;
	} while (ms < 10000);
	
	REPORT(" getms", calls, ms);
	fixed_overhead = ms / calls;
	printf("Fixed overhead: %d\n", fixed_overhead);
	printf("Granularity: %d\n", gran);
	printf("\n");
}

/* Test how many null statements per second.
   This must be the second test, as it also sets the loop overhead. */

#define NULL_STMT {}

test_null()
{
	printf("Testing null statement (loop overhead); wait...\n");
	{
		GEN_TEST2(20000, NULL_STMT);
		printf("Loop overhead: %d\n\n", t / i);
		/*
		loop_overhead = t / i;
		*/
	}
}

null_call()
{
}

test_call()
{
	printf("Testing null function call; wait...\n");
	GEN_TEST(100000, null_call());
}

test_timeout()
{
	long to;
	timeout(to = timeout(30000L));
	printf("Testing timeout(); wait...\n");
	GEN_TEST(100000, timeout(to));
}

test_one_way()
{
	port p;
	
	uniqport(&p);
	printf("Testing one_way(); wait...\n");
	GEN_TEST(1000, one_way(&p, &p));
}

#ifdef TEST_SYNCH

/* This is programmed as a simple producer/consumer problem. */

static mutex full, empty;	/* Busy if the condition is true */
int data;

void
sleep_thread(param, size)
char *param;
int   size;
{
	int stop;

	do {
		/* Consume one item */
		mu_lock(&empty);
		stop = data;
		mu_unlock(&full);
	} while (!stop);
	exitthread((long *) 0);
}

produce(stop)
	int stop;
{
	/* Produce one item */
	mu_lock(&full);
	data = stop;
	mu_unlock(&empty);
}

void
test_thread_synch()
{
	printf("Testing thread synchronisation using mu_[un]lock(); wait...\n");
	mu_init(&full);
	mu_init(&empty);
	mu_lock(&empty);
	if (!thread_newthread(sleep_thread, 4000, (char *)NULL, 0)) {
		printf("thread_newthread failed\n");
		mu_unlock(&empty);
		return;
	}
	GEN_TEST(10000, produce(0));
	produce(1);
}

#endif /* TEST_SYNCH */

#ifdef TEST_SWITCH

static long child_switch, parent_switch;
static int child_done;

static void
incr_and_switch(count)
int *count;
{
	++*count;
	threadswitch();
}

static void
child_thread(param, size)
char *param;
int   size;
{
	GEN_TEST(10000, incr_and_switch(&child_switch));
	/* printf("child: parent_switch = %ld\n", parent_switch); */
	child_done = 1;
	exitthread((long *) 0);
}

void
test_thread_switch()
{
	printf("Testing thread switching overhead; wait...\n");
	if (!thread_newthread(child_thread, 4000, (char *)NULL, 0)) {
		printf("thread_newthread failed\n");
		return;
	}
	GEN_TEST(10000, incr_and_switch(&parent_switch));
	/* printf("parent: child_switch = %ld\n", child_switch); */
	while (!child_done) {
		sleep(1);
	}
}

#endif /* TEST_SWITCH */

mutex block; /* Also used by test_newthread and test_modthread */

call_mutex()
{
	mu_init(&block);
	mu_lock(&block);
	mu_unlock(&block);
}

test_mutex()
{
	printf("Testing mu_init; wait...\n");
	GEN_TEST(1000000, mu_init(&block));
	
	printf("Testing mu_init+lock+wait; wait...\n");
	GEN_TEST(100000, call_mutex());
}

#ifdef TEST_THREAD

char stack[1000];

void
null_thread()
{
	mu_unlock(&block);
	exitthread((long *) 0);
}

call_thread()
{
	mu_init(&block);
	mu_lock(&block);
	if (sys_newthread(null_thread,
			  (struct thread_data *) (stack + sizeof(stack)),
			  (struct thread_data *) 0) == 0)
		mu_lock(&block);
	else
		printf("sys_newthread failed\n");
}

test_newthread()
{
	printf("Testing sys_newthread+mu__init+lock+unlock+exitthread; wait...\n");
	GEN_TEST(1000, call_thread());
}

#endif /* TEST_THREAD */

#ifdef TEST_MOD

/*ARGSUSED*/
void
null_modthread(param, size)
char *param;
int   size;
{
	mu_unlock(&block);
}

call_newthread()
{
	mu_init(&block);
	mu_lock(&block);
	if (thread_newthread(null_modthread, 4000, (char*)NULL, 0))
		mu_lock(&block);
	else
		printf("thread_newthread failed\n");
}

test_modthread()
{
	printf("Testing thread_newthread+mu_lock+mu_unlock+thread_exit; wait...\n");
	GEN_TEST(1000, call_newthread());
}

#endif /* TEST_MOD */

#if defined(TEST_TRANS) || defined(TEST_2TRANS)

port aport;

/*ARGSUSED*/
void
server_thread(param, size)
char *param;
int   size;
{
	header h;
	short n;
	static char buf[30000];
	
	for (;;) {
	        h.h_port = aport;
		n = getreq(&h, buf, sizeof buf);
		if (n < 0) {
			printf("server_thread: getreq: error %d\n", n);
			break;
		}
		putrep(&h, buf, h.h_size);
		if (h.h_command != 0)
			break;
	}
	exitthread((long *) 0);
}

transcalls()
{
	header h, hreply;
	static char buf[30000];
	bufptr nilbuf = NILBUF;
	
	priv2pub(&aport, &h.h_port);
	h.h_command = 0;
	h.h_size = 0;
	/* Added to circumvent locate: */
	trans(&h, NILBUF, 0, &hreply, NILBUF, 0);

	GEN_CHECKED_TEST(10000, trans(&h, nilbuf, 0, &hreply, nilbuf, 0), 0);
	GEN_CHECKED_TEST(10000, trans(&h, buf, 1, &hreply, nilbuf, 0), 0);
	h.h_size = 1;
	GEN_CHECKED_TEST(10000, trans(&h, nilbuf, 0, &hreply, buf, 1), 1);
	GEN_CHECKED_TEST(10000, trans(&h, buf, 1, &hreply, buf, 1), 1);
	h.h_size = 0;
	GEN_CHECKED_TEST(1000, trans(&h, buf, 30000, &hreply, nilbuf, 0), 0);
	h.h_size = 30000;
	GEN_CHECKED_TEST(1000,trans(&h, nilbuf, 0, &hreply, buf, 30000),30000);

	/* Get rid of server threads */
	h.h_command = 1;
	trans(&h, NILBUF, 0, &hreply, NILBUF, 0);
	trans(&h, NILBUF, 0, &hreply, NILBUF, 0);
}

#endif /* TEST_TRANS || TEST_2TRANS */

#ifdef TEST_TRANS

void
test_trans()
{
	printf("Testing intra-process transactions; wait...\n");
	uniqport(&aport);
	if (!thread_newthread(server_thread, 4000, (char *)NULL, 0)) {
		printf("thread_newthread failed\n");
		return;
	}
	if (!thread_newthread(server_thread, 4000, (char *)NULL, 0)) {
		printf("second thread_newthread failed, continuing crippled\n");
	}
	transcalls();
}

#endif

#ifdef TEST_2TRANS

server_process()
{
	capability *cap = getcap("APORT");
	if (cap == NULL) {
		printf("server_process: APORT not in cap env\n");
		exit(1);
	}
	aport = cap->cap_port;
	if (!thread_newthread(server_thread, 4000, (char *)NULL, 0))
		printf("server_process: thread_newthread failed, using 1 thread\n");
	server_thread((char *) NULL, 0);
}

char *args[] = {"tats"/*overwritten*/, "-@", NULL}; /* Argument list */

/* Capability environment: */
struct caplist caps[] = {
	{"STDOUT", 0},
	{"ROOT", 0},
	{"APORT", 0},
	{NULL, 0}
};

void
test_2_trans()
{
	struct caplist *cl;
	capability cap, cap2;
	capability *hostcap = NULL;
	errstat err;
	
	printf("Testing inter-process transactions; wait...\n");
	uniqport(&aport);
	cap.cap_port = aport;
	for (cl = caps; cl->cl_name != NULL; ++cl) {
		if ((cl->cl_cap = getcap(cl->cl_name)) == NULL)
			cl->cl_cap = &cap;
	}
	args[0] = progname;
	if (peer_host != NULL) {
		if (host_lookup(peer_host, &cap2) != STD_OK ||
			dir_lookup(&cap2, PROCESS_SVR_NAME, &cap2) != STD_OK) {
			printf("tats: can't find machine %s, using default\n",
				peer_host);
		}
		else
			hostcap = &cap2;
	}
	else
		printf("warning: no machine specified, using default\n");
	if ((err = exec_file(
			NILCAP,		/*prog*/
			hostcap,	/*host*/
			NILCAP,		/*owner*/
			0,		/*stacksize*/
			args,		/*argv*/
			(char **) 0,	/*envp*/
			caps,		/*caplist*/
			NILCAP		/*process_ret*/
		)) != 0) {
		printf("tats: can't start process: %s\n", err_why(err));
		return;
	}
	transcalls();
}

#endif /* TEST_2TRANS */

#ifdef TEST_BULLET

capability bullet; /* Bullet file server */

void
test_bullet()
{
	char *p;
	
	if (name_lookup(DEF_BULLETSVR, &bullet) != STD_OK) {
		printf("tats: no bullet server; skipping bullet tests\n");
		return;
	}
	
	printf("DON'T INTERRUPT--THE BULLET FILE SERVER WOULD FILL UP\n\n");
	
	if (multiplier > 5)
		multiplier = 5; /* Or it would take forever */
	
	/*SMALL*/
	if ((p = (char *) malloc(100)) != NULL) {
	    printf("Testing creation of small bullet files; wait...\n");
	    GEN_CHECKED_TEST(1000, create_bullet_file(p, 100), STD_OK);
	
	    printf("Testing reading of small bullet files; wait...\n");
	    GEN_TEST(1000, read_bullet_file(p, 100));

	    free((_VOIDSTAR) p);
	    delete_bullet_files();
	}

	/*MEDIUM*/
	if ((p = (char *) malloc(50000)) != NULL) {
	    printf("Testing creation of medium-sized bullet files; wait...\n");
	    GEN_CHECKED_TEST(100, create_bullet_file(p, 50000), STD_OK);
	
	    printf("Testing reading of medium-sized bullet files; wait...\n");
	    GEN_TEST(100, read_bullet_file(p, 50000));

	    free((_VOIDSTAR) p);
	    delete_bullet_files();
	}
	
	/*LARGE*/
	if ((p = (char *) malloc(500000)) != NULL) {
	    printf("Testing creation of large bullet files; wait...\n");
	    GEN_CHECKED_TEST(10, create_bullet_file(p, 500000), STD_OK);
	
	    printf("Testing reading of large bullet files; wait...\n");
	    GEN_TEST(20, read_bullet_file(p, 500000));
	
	    free((_VOIDSTAR) p);
	
	    delete_bullet_files();
	}
}

capability bcaplist[1000];
int nbcaps = 0;

int
create_bullet_file(p, size)
	char *p;
	int size;
{
	if (nbcaps >= 1000)
		--nbcaps;
	return b_create(&bullet, p, (b_fsize) size, BS_COMMIT,
		        &bcaplist[nbcaps++]);
}

int
read_bullet_file(p, size)
	char *p;
	int size;
{
	static int i;
	b_fsize n;
	errstat err;
	
	if (i == 0)
		i = nbcaps;
	err = b_read(&bcaplist[--i], (b_fsize) 0, p, (b_fsize) size, &n);
	if (err != STD_OK) {
		printf("b_read failed (%d)\n", err);
		n = 0;
	}
	else if (n != size)
		printf("b_read read %d instead of %d\n", n, size);
	return n;
}

delete_bullet_files()
{
	while (nbcaps > 0)
		(void) std_destroy(&bcaplist[--nbcaps]);
}

#endif /* TEST_BULLET */
