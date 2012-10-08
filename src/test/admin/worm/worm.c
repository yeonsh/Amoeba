/*	@(#)worm.c	1.3	96/02/27 10:53:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*

"Waiter, there's a worm in my host!"

This worm runs only on Amoeba pool processors.  It works for hosts of
one architecture at a time; the architecture is taken from <defarch.h>.
It is intended to be harmless, but I can't give guarantees; it may
cause host or network crashes simply because of the added load.  Also,
the current version is rather verbose, and pollutes the console of all
affected hosts.  There is a trick to make all worms go away; more about
this later.

The worm's goal is to get a copy of itself running at each pool
processor.  Each worm knows about all pool processors, and checks
regularly whether at least one copy of itself is still runnin at each
of the others.  If it finds a host without a copy of itself, it
propagates itself to that host.  This style of algorithm is known as
"flooding", and used amongst others by USENET news (although there not
every host is connected to every other host).

The worm has two threads: a simple server thread, which answers requests
from the other worms, and a main loop, which polls the others.  A fixed
port is used per machine; currently, this is simply the host name,
truncated or zero-padded to 6 bytes.  When a new worm is started, it
tries to kill all other worms running on the same machine.  The
algorithm used to do thisis a little tricky: it sends a special request
to the port for that machine, and the recipient tries to propagate the
request.  When there are no more worms on the machine, the last one will
time out, and all worms that received the request will exit except the
one that initiated the request.  It is still not clear that no race
conditions exist: two new worms might start killing siblings at
the same time, and each reach only themselves; but it seems better than
it was.

There are two ways for the worm to propagate itself.  The first way,
called "breeding", tries to execute the file named by argv[0] on the target
processor.  If this fails for some reason (except when the target is
down), the second way is tried, called "spawning".  This works even if
the directory and file server (or any other services) are down: the worm
copies all its segments to the target, and then sends an appropriately
patched copy of its own processes descriptor (cleverly gotten from the
getinfo system call) to the target to execute it.  (This is almost the
same as a Unix "fork" operation except that the new worm starts
execution at a different address.)  A worm thus started notices that
this is the case because all variables retain their value; in
particular, 'inited' will be nonzero, so it can skip part of the
initialization.  (It must still redo part of the initialization, to find
out on which host it is executing and to redirect its output there.)

The worm gets its information about suitable victims from the directory
<POOL_DIR>/<arch>  (Some entries are skipped for various reasons.)  Worms
started by spawning do not read this directory but use the table of
hosts built by their originator.

There is a protocol to stop a collection of worms: each worm has a
global variable "stop", which, once set to nonzero, will eventually
cause the worm to stop.  When a worm polls another worm, each sends its
current stop value to the other, and if either is nonzero, the other
will set its stop variable also.  (Thus, the stop flag travels like the
AIDS virus.)  When a worm has detected that it must stop, it will make
one last round of all other hosts, to make sure they have all gotten
the stop flag; then it dies.  This will normally get rid of all worms.
However, worms living in a partition of the network will be
unreachable, and break out when the network is joined again.  For
convenience, a worm started as "worm -k" will set its own stop flag and
thus propagate it to all other worms.

Exercises:

Rationalize the use of timeouts.

Let each worm periodically check the pooldir directory for the
appearance of new hosts.  Let worms communicate such new entries to
other hosts as data in the poll requests.

Let the worms participate in some distributed computation, such as
factoring 100-digit numbers or breaking Unix passwords.

Use the worms to produce a synthesized load.  There should be parameters
to set the percentage of CPU, memory and network utilization, the
average total number of worms and their average lifetime, etc.  Measure
the performance of Amoeba under various loads.  Tune the kernel to
improve its performance under heavy load.  Etc., etc.

Author: Guido van Rossum, CWI, June, 1989

*/

#include <amtools.h>
#include <cmdreg.h>
#include <unistd.h>
#include <ampolicy.h>
#include <defarch.h>
#include <thread.h>
#include <module/proc.h>
#include <module/direct.h>
#include <module/rnd.h>

int inited;
long initial_sp;
int stop;
int debugging;

char *progname;

panic(s)
	char *s;
{
	timeout(2000L);
	printf("%s: panic: %s\n", progname, s);
	exit(1);
}

char *worm_argv[] = {"worm", NULL, NULL}; /* worm_argv[1] may become "-d" */

main(argc, argv)
	int argc;
	char **argv;
{
	timeout(10000L);
	if (inited) {
		progname = "SPAWNED WORM";
		/* printf("%s: &argc=%x\n", progname, &argc); */
	}
	else {
		worm_argv[0] = argv[0];
		if (argc > 1) {
			if (strcmp(argv[1], "-k") == 0)
				stop = 1;
			else if (strcmp(argv[1], "-d") == 0)
				debugging = 1;
		}
		progname = "WORM";
		/* printf("%s: &argc=%x\n", progname, &argc); */
		initial_sp = (long)&argc;
		readhosts();
	}
	inited = 1;
	whoami();
	startserver();
	killsiblings();
	mainloop();
	panic("end of program reached");
	/*NOTREACHED*/
}

#define MAXNAMELEN	16
#define MAXNHOSTS	100

struct {
	char name[MAXNAMELEN];
	capability dir;
	port privport, pubport;
} host[MAXNHOSTS];

unsigned int nhosts;
int mine;

readhosts()
{
	char poolname[256];
	capability pool;
	int i;
	char *name;
	struct dir_open *dp;
	int namelen;
	
	sprintf(poolname, "%s/%s", POOL_DIR, ARCHITECTURE);
	timeout(10000L);
	printf("%s: reading %s...\n", progname, poolname);
	if (name_lookup(poolname, &pool) != STD_OK)
		panic("can't find POOL_DIR");
	if ((dp = dir_open(&pool)) == NULL)
		panic("can't open POOL_DIR");
	i = 0;
	while ((name = dir_next(dp)) != NULL) {
		if (mustskip(name))
			continue;
		if (dir_lookup(&pool, name, &host[i].dir) == STD_OK) {
			namelen = strlen(name);
			strncpy(host[i].name, name, MAXNAMELEN-1);
			/* one_way(&host[i].dir, &host[i].privport); */
			strncpy((char *)&host[i].privport,
				 host[i].name + (namelen<=PORTSIZE ? 0 : namelen-PORTSIZE),
				 PORTSIZE);
			priv2pub(&host[i].privport, &host[i].pubport);
			if (++i >= MAXNHOSTS)
				break;
		}
	}
	(void)dir_close(dp);
	if ((nhosts = i) == 0)
		panic("no hosts in POOL_DIR");
	printf("%s: found %d hosts.\n", progname, nhosts);
}

char *skip[] = {".", "..", ".run", NULL};

int
mustskip(name)
	char *name;
{
	char **pp;
	
	for (pp = skip; *pp != NULL; ++pp) {
		if (strcmp(name, *pp) == 0)
			return 1;
	}
	return 0;
}

whoami()
{
	capability myself, procsvr, ttycap;
	int i;
	size_t len;
	char *newname;
	
	timeout(500L);
	printf("%s: looking for myself in host table...\n", progname);
	getinfo(&myself, (process_d *) NULL, 0);
	mine = -1;
	for (i = 0; i < nhosts; ++i) {
		if (dir_lookup(&host[i].dir, "proc", &procsvr) == STD_OK) {
			if (PORTCMP(&procsvr.cap_port, &myself.cap_port)) {
				mine = i;
				break;
			}
		}
	}
	if (mine < 0)
		panic("can't find myself in POOL_DIR");

	len = strlen(host[mine].name) + strlen(progname) + 2;
	newname = (char *) malloc(len);
	if (newname != NULL) {
		strcpy(newname, progname);
		strcat(newname, "@");
		strcat(newname, host[mine].name);
		progname = newname;
	}
	
	printf("%s: I'm at %s.\n", progname, host[mine].name);
	if (!stop && !debugging &&
			dir_lookup(&host[i].dir, "tty:00", &ttycap)== STD_OK) {
		printf("%s: redirecting further output to %s/tty:00.\n",
			progname, host[mine].name);
		progname = host[mine].name;
		set_cd(&host[mine].dir);
		freopen("tty:00", "a", stdout);
		setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
		freopen("tty:00", "a", stderr);
		setvbuf(stderr, (char *)NULL, _IOLBF, BUFSIZ);
	}
	printf("%s: WORM STARTED.\n", progname);
}

set_cd(newcd)
	capability *newcd;
{
	capability *work;
	
	work = getcap("WORK");
	if (work == NULL)
		panic("no WORK in capability environment");

	*work = *newcd;
	return STD_OK;
}

#define KILL_SIBLINGS	UNREGISTERED_FIRST_COM

port identity;

/*forward*/ void server();

startserver()
{
	timeout(2000L);
	printf("%s: starting server...\n", progname);
	uniqport(&identity);
	if (thread_newthread(server, 5000, (char *) 0, 0) == 0)
		panic("can't start server thread");
	printf("%s: server started.\n", progname);
}

/*ARGSUSED*/
void
server(param, size)
char *param;
int   size;
{
	header h;
	int err;
	
	timeout(2000L);
	for (;;) {
		h.h_port = host[mine].privport;
		if ((err = getreq(&h, NILBUF, 0)) < 0) {
			if (err == RPC_ABORTED)
				continue;
			panic("getreq failed");
		}
		if (!stop && h.h_command == STD_DESTROY) {
			printf("%s: GOT STOP REQUEST.\n", progname);
			stop = 1;
		}
		if (h.h_command == KILL_SIBLINGS) {
			killsiblings();
			timeout(2000L); /* Reset it */
			if (PORTCMP(&h.h_priv.prv_random, &identity)) {
				printf("%s: won't kill myself\n", progname);
			}
			else {
				printf("%s: KILLED BY SIBLING.\n", progname);
				h.h_status = STD_OK;
				putrep(&h, NILBUF, 0);
				exitprocess(1);
			}
		}
		h.h_status = stop ? STD_DESTROY : STD_OK;
		putrep(&h, NILBUF, 0);
	}
}

killsiblings()
{
	timeout(500L);
	printf("%s: killing siblings...\n", progname);
	poll(mine, KILL_SIBLINGS);
	printf("%s: survived sibling killing.\n", progname);
}

poll(i, cmd)
	int i;
	int cmd;
{
	header h;
	int err;
	
	h.h_port = host[i].pubport;
	h.h_command = stop ? STD_DESTROY : cmd;
	h.h_priv.prv_random = identity;
	err = trans(&h, NILBUF, 0, &h, NILBUF, 0);
	if (err != STD_OK)
		return err;
	if (!stop && h.h_status == STD_DESTROY) {
		printf("%s: POLL GOT STOP BACK.\n", progname);
		stop = 1;
	}
	return STD_OK;
}

mainloop()
{
	unsigned i, h;
	int stopping;
	
	timeout(5000L);
	printf("%s: MAIN LOOP...\n", progname);
	if (!stop)
		sleep((unsigned) (mine*10));
	srand((unsigned) mine);
	for (;;) {
		printf("%s: polling siblings...\n", progname);
		stopping = stop;
		if (stopping)
			printf("%s: STOPPING; LAST ROUND...\n", progname);
		/* start polling at an arbitrary host */
		h = ((unsigned) rand()) % nhosts;
		for (i = 0; i < nhosts; ++i) {
			if (++h >= nhosts)
				h = 0;
			if (h == mine)
				continue;
			if (poll(h, STD_TOUCH) != STD_OK) {
				printf("%s: no siblings at %s (%d)\n",
					progname, host[h].name, h);
				if (!stop)
					breed(h);
			}
			sleep(5);
		}
		if (stopping) {
			printf("%s: STOP.\n", progname);
			exit(0);
		}
		sleep(60);
	}
}

breed(i)
	int i;
{
	capability procsvr;
	errstat err;
	
	if (debugging)
		worm_argv[1] = "-d";
	printf("%s: breeding at %s...\n", progname, host[i].name);
	if (dir_lookup(&host[i].dir, "proc", &procsvr) != STD_OK) {
		printf("%s: can't find %s/proc\n", progname, host[i].name);
		return;
	}
	err = exec_file(NILCAP, &procsvr, NILCAP, 0, worm_argv,
			(char **) NULL, (struct caplist *) NULL, NILCAP);
	if (err == STD_OK) {
		printf("%s: bred at %s.\n", progname, host[i].name);
		return;
	}
	printf("%s: can't breed at %s (%s)\n",
		progname, host[i].name, err_why(err));
	printf("%s: spawning at %s...\n", progname, host[i].name);
	err = spawn(&procsvr);
	if (err == STD_OK)
		printf("%s: spawned at %s\n", progname, host[i].name);
	else
		printf("%s: can't spawn at %s (%s)\n",
			progname, host[i].name, err_why(err));
}

int
spawn(procsvr)
	capability *procsvr;
{
	char pdbuf[2000];
	process_d *pd = (process_d *)pdbuf; /* Constant */
	int i;
	segment_d *sd;
	int err;
	thread_d *td;
	thread_idle *tdi;
	capability process;
	
	getinfo(NILCAP, pd, sizeof pdbuf);
	
	pd->pd_nthread = 1;
	td = PD_TD(pd);
	td->td_state = 0;
	td->td_extra = TDX_IDLE;
	td->td_len = sizeof(thread_d) + sizeof(thread_idle);
	tdi = (thread_idle *)(td+1);

	/* Patch the process descriptor to let the new process start at
	 * the default entry point.  For most architectures/compilers we
	 * can just use the start of the text segment for this.
	 */
#if defined(mc68000) && defined(sun) || defined(__mc68000) && defined(__GNUC__)
	tdi->tdi_pc = 0x2020;
#else
	/* start at beginning of text segment */
	sd = PD_SD(pd);
	if (sd->sd_type & MAP_TYPETEXT) {
		tdi->tdi_pc = sd->sd_addr;
	} else {
		printf("%s: spawn: no text segment?!\n", progname);
		return STD_SYSERR;
	}
#endif
	tdi->tdi_sp = initial_sp;
	
	for (i = 0, sd = PD_SD(pd); i < pd->pd_nseg; ++i, ++sd) {
		if ((sd->sd_type & MAP_TYPEMASK) == 0)
			continue;
		if (!NULLPORT(&sd->sd_cap.cap_port))
			continue;
		/*
		printf("%s: seg %d, addr x%x, len x%x, type x%x\n",
			progname, i,sd->sd_addr, sd->sd_len, sd->sd_type);
		*/
		err = ps_segcreate(procsvr, sd->sd_len, NILCAP, &sd->sd_cap);
		if (err != STD_OK) {
			printf("%s: ps_segcreate failed\n", progname);
			return err;
		}
		err = ps_segwrite(&sd->sd_cap, 0L,
				  (char *) sd->sd_addr, sd->sd_len);
		if (err < 0) {
			printf("%s: ps_segwrite failed\n", progname);
			return err;
		}
		sd->sd_type |= MAP_AND_DESTROY;
	}
	
	/* Zero the owner cap so that the process descriptor doesn't
	 * get sent when the new process exits.
	 */
	CAPZERO(&pd->pd_owner);
	
	err = pro_exec(procsvr, pd, &process);
	if (err != STD_OK)
		printf("%s: pro_exec failed\n", progname);
	else
		(void) pro_setcomment(&process, "worm (spawned)");
	return err;
}

