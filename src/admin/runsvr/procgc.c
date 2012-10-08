/*	@(#)procgc.c	1.5	96/02/27 10:18:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Process garbage collection -- kill orphaned processes */

#include "runsvr.h"

/*
Kill orphaned processes for a particular host.
This lists the host's process server directory and, for each process
that it finds, sees whether it should still live.
The only reason why a process shouldn't live any more is when it has
a non-null owner capability which refers to a non-existent server.
The server is polled with a std_info() call, but most errors from
this call are OK -- only an RPC_NOTFOUND indicates the server doesn't
exist any more.
This situation can occur when the server has crashed.  Also because
of network partitions or when the server is temporarily busy, but a
fairly long time-out is used to avoid drawing unwarranted
conclusions.
When a process is found that should die, it is stunned with stun code
-666 -- should the owner be reachable again by the time the stun is
done, at least the cause of the checkpoint it receives may be guessed
from this number.  If the stun succeeds (this does not mean the
process is dead -- just that a stun has been initiated), a message is
printed (unconditionally) on stdout.
*/

static void
hostgc(p)
	struct hostinfo *p;
{
	char *name;
	capability ps;
	capability process;
	capability owner;
	char buf[256];
	int n;
	int err;
	interval savetout;
	struct dir_open *dp;
	
#ifdef DEBUG
	if (debugging)
		printf("hostgc: inspect host %s\n", p->hi_name);
#endif
	if (dir_lookup(&p->hi_hostcap, PROCESS_LIST_NAME, &ps) != STD_OK) {
		MON_ERROR("hostgc: dir_lookup of ps failed");
		return; /* Forget it */
	}
	if ((dp = dir_open(&ps)) == 0) {
		MON_ERROR("hostgc: dir_open of ps failed");
		return; /* Forget it */
	}
	savetout = timeout((interval)10000L);
	
	while ((name = dir_next(dp)) != NULL) {
#ifdef DEBUG
		if (debugging)
			printf("hostgc: inspect process %s on host %s\n",
							name, p->hi_name);
#endif
		err = dir_lookup(&ps, name, &process);
		if (err != STD_OK) {
			MON_NUMERROR("hostgc: dir_lookup failed", err);
			continue;
		}
		err = std_info(&process, buf, (int) sizeof(buf), &n);
		if (err == STD_OVERFLOW) { /* truncate rest */
			n = sizeof(buf);
			err = STD_OK;
		}
		if (err != STD_OK){
			MON_NUMERROR("hostgc: process std_info failed", err);
			continue;
		}
		buf[n] = '\0';
		if (must_live(buf)) {
			MON_EVENT("hostgc: process allowed to live");
			continue;
		}
		if ((err = pro_getowner(&process, &owner)) != STD_OK) {
			MON_NUMERROR("hostgc: pro_getowner failed", err);
			continue;
		}
		if (NULLPORT(&owner.cap_port)) {
			MON_EVENT("hostgc: null owner, let live");
			continue;
		}
		err = std_info(&owner, buf, (int) sizeof(buf), &n);
		if (err == STD_OVERFLOW) { /* truncate rest */
			n = sizeof(buf);
			err = STD_OK;
		}
		if (err == STD_OK) {
			MON_EVENT("hostgc: owner lives, process may live");
			continue;
		}
		if (err != RPC_NOTFOUND) {
			MON_NUMERROR("hostgc: std_info error", err);
			continue;
		}
		if ((err = pro_stun(&process, -666L)) == STD_OK) {
			MON_EVENT("hostgc: process killed");
		}
		else {
			if (err == STD_NOTNOW) {
				MON_EVENT("hostgc: process already stunned");
			}
			else {
				MON_NUMERROR("hostgc: pro_stun error", err);
			}
		}
		if (err == STD_OK || err == RPC_FAILURE) {
			long now;
			long time();
			char *ctime();

			(void) time(&now);
			printf("Run server (%.15s): ", ctime(&now)+4);
			if (err == RPC_FAILURE)
				printf("maybe ");
			printf("killed process %s on host %s\n",
							name, p->hi_name);
			(void) fflush(stdout);
		}
	}
	
	(void) timeout(savetout);
	
	(void)dir_close(dp);
}

/* Do some checks on a process's std_info string to see if it is an
   exception and must live even if its owner is (or appears) dead.
   This is the case for stopped processes: the owner may be single-
   threaded, like tad, and not respond to std_info; but when the owner
   dies the process will surely be killed since the checkpoint
   transaction will return RPC_FAILED.
   It is also the case for certain daemon processes, recognized by
   the string "Daemon" in the std_info message.
*/

int
must_live(str)
	char *str; /* Process std_info string */
		/* Example: "running process: username: dir -l" */
{
	if (strncmp(str, "running", 7) != 0) {
		MON_EVENT("must_live: not running");
		return 1; /* Probably stopped -- don't bother to kill it */
	}
	for (; *str != '\0'; str++) {
		if (*str == 'D' && strncmp(str, "Daemon", 6) == 0) {
			MON_EVENT("must_live: Daemon process");
			return 1;
		}
	}
	MON_EVENT("must_live: mere mortal");
	return 0;
}

/* Call hostgc() above for each host that's up in the list given as argument,
   then reschedule ourselves a minute from now.
   To get this started, main() calls us with a NULL list argument, so this
   first call just reschedules (always with 'First_host' as list argument).
*/

void
procgcjob(arg)
	char *arg;
{
	struct hostinfo *p;
	
	MON_EVENT("begin process garbage collection");
	for (p = (struct hostinfo *)arg; p != NULL; p = p->hi_next) {
		if (p->hi_up)
			hostgc(p);
	}
	MON_EVENT("end process garbage collection");
	
	/* Reschedule in queue for down jobs because it has low priority */
	addjob(downjobs, procgcjob, First_host, 60000L,
		"can't reschedule process gc job");
}
