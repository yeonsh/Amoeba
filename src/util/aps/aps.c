/*	@(#)aps.c	1.8	96/02/27 13:07:05 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
   In default mode, aps prints a information about a user's processes.
   The session server is asked for a table containing all current processes.

   In "super" mode (specified by the "-s" flag), aps enquires the pool
   processors directly.  This can be used to reveal processes that are
   not under session control.
   For super users it can also show information about pool processors.
   With the option -l the display can be made into a fixed screen format,
   appropriate for continuous updating of a screen display.

   When in super mode, by default it tries all processors in /profile/pool/?*.
   It is also possible to give aps an alternative host directory name
   (e.g., /profile/hosts).
   The "-m host" option can be used to specify a single host.
   For each host it does the following:
   - print a header containing the processor load and uptime;
   - look up "ps";
   - list the contents of "ps";
   - for each process thus found:
     try a std_info(), a pro_getowner(), and a std_info() on the owner,
     and print the results.

   In default mode it performs the last step for all processes known to
   the session server.  It also prints additional information (process group,
   parent pid and state) as delivered by the session server's ses_getpt()
   command.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <amoeba.h>
#include <ampolicy.h>
#include <module/goodport.h>
#include <module/systask.h>
#include <module/name.h>
#include <string.h>
#include <cmdreg.h>
#include <stderr.h>
#include <module/direct.h>
#include <module/proc.h>
#include <module/host.h>
#include <sesstuff.h>
#include <stdobjtypes.h>

extern int optind;
extern char * optarg;

void set_pool_dirs();

int lines_per_machine = 0;	/* See: -l number */
int verbose = 0;		/* See: -v */
int super_user = 0;		/* See: -s */
int all_users = 0;		/* See: -a */
char *only_user = NULL;		/* See: -u user */
int this_user = 0;		/* only_user is in fact current user */
char *progname = "APS";
char *pool_dirs[128];	/* Each entry is the name of a pool dir */
int num_pool_dirs = 0;

int cur_line = 0;
int ideal_line = 0;

#define K 1024
#define PS PROCESS_LIST_NAME

void
usage()
{
	register int i;
	fprintf(stderr,
	      "usage: %s [-l lines-per-host] [-u user] [-v] [-s] [-a] [directory] ...\n",
	      progname);
	fprintf(stderr,
	      "or:    %s [-l lines-per-host] [-u user] [-v] [-s] [-a] -m [hostname] ...\n",
	      progname);
	fprintf(stderr, "\nhosts are contacted when one of [-lms] is specified;\n");
	fprintf(stderr, "otherwise only session servers are used\n");
	fprintf(stderr, "\ndefault for -m: all hosts\n");

	set_pool_dirs(POOL_DIR); /* not done yet */
	fprintf(stderr, "\ndefault directories:");
		for (i = 0; i < num_pool_dirs; i++)
			fprintf(stderr, " %s", pool_dirs[i]);
	fprintf(stderr, "\n");
	exit(2);
	/*NOTREACHED*/
}

char *
plural(n)
	long n;
{
	return (n == 1) ? "" : "s";
}

void
fmtuptime(p, uptime)
	char *p;
	long uptime; /* Up time in seconds */
{
	if (uptime < 5*60) {
		sprintf(p, "%ld sec", uptime);
		return;
	}
	if (uptime < 60*60) {
		sprintf(p, "%ld min %ld sec", uptime/60, uptime%60);
		return;
	}
	
	uptime /= 60; /* Now it's in minutes */
	if (uptime < 24*60) {
		sprintf(p, "%ld hr%s %ld min",
			uptime/60, plural(uptime/60), uptime%60);
		return;
	}
	
	uptime /= 60; /* Now it's in hours */
	if (uptime < 7*24) {
		sprintf(p, "%ld day%s %ld hr%s",
			uptime/24, plural(uptime/24),
			uptime%24, plural(uptime%24));
		return;
	}
	
	uptime /= 24; /* Now it's in weeks */
	sprintf(p, "%ld week%s %ld day%s",
		uptime/7, plural(uptime/7),
		uptime%7, plural(uptime%7));
}

errstat
getuptime(phost, uptimep)
	capability *phost;
	long *uptimep;
{
	errstat err;
	char buf[100];
	int size;
	
	err = gp_trans(phost, sys_kstat(phost, 'u', buf, sizeof buf, &size));
	if (err == STD_OK)
		*uptimep = atol(buf);
	return err;
}

errstat
getversion(cap, buf, buflen)
	capability *cap;
	char *buf;
	int buflen;
{
	errstat err;
	int n;
	
	err = gp_trans(cap, sys_kstat(cap, 'v', buf, (bufsize) buflen, &n));
	if (err == STD_OK) {
		if (buf[n-1] == '\n')
			--n;
		buf[n] = '\0';
	} else {
		sprintf(buf, "(can't get version, sys_kstat: %s)",
			err_why(err));
	}
	return err;
}

errstat
ppload(name, hostcap)
	char *name;
	capability *hostcap;
{
	capability proccap;
	capability syscap;
	long load, ips, mem, uptime;
	errstat err;
	int ignore;
	char buf[2000];
	interval savetout;
	
	uptime = -1;

	savetout = timeout((interval)2000);

	/* First, quickly make sure host is up: */
	err = gp_trans(hostcap, std_info(hostcap, (char *)0, 0, &ignore));

	if (err == STD_OK || err == STD_OVERFLOW)
		err = dir_lookup(hostcap, PROCESS_SVR_NAME, &proccap);

	if (err != STD_OK) {
		sprintf(buf, "%s: unreachable (proc lookup: %s)",
			name, err_why(err));
	}
	else if ((err = dir_lookup(hostcap, DEF_SYSSVRNAME, &syscap)) != STD_OK) {
		sprintf(buf, "%s: sys server not found.", name, err_why(err));
	}
	else if ((err = gp_trans(&proccap,
			 pro_getload(&proccap, &ips, &load, &mem))) != STD_OK) {
		sprintf(buf, "%s: no load information (getload: %s)",
			name, err_why(err));
	} else {
		/* Normalize load average.
		 * In a previous version we computed "loadavg = ips/(load+K)"
		 * i.e., the number of Mips available for a new program
		 * starting on the host.  However, users are typically
		 * interested in the *current* load average.
		 */
		load = (100*load + K/2)/K;
		sprintf(buf, "%s: load %ld.%02.2ld, %ld.%ld Mips, %ld.%ld Mb free",
                        name, load / 100, load % 100,
			ips/(1000*1000), ((ips/1000) % 1000) / 100,
			mem/(K*K), ((mem/K) % K) / 100);
		if (getuptime(&syscap, &uptime) == STD_OK) {
			char *p = strchr(buf, '\0');
			(void) strcpy(p, ", up ");
			p = strchr(p, '\0');
			fmtuptime(p, uptime);
		}
	}
	(void) strcat(buf,
	" -------------------------------------------------------------------");
	printf("----- %0.73s\n", buf);
	cur_line++;
	
	if (verbose && err == STD_OK) {
		if (getversion(&syscap, buf, (int) sizeof buf) == STD_OK) {
		    char *cp;

		    /* Ensure that we have one line that isn't too long */
		    for (cp = buf; cp - buf < 80 && *cp && *cp != '\n'; cp++)
			    /* no-op */ ;
		    *cp = '\0';

		    printf("%s\n", buf);
		    cur_line++;
		}
	}
	(void) timeout(savetout);
	return err;
}

/* Caller supplies the character buffers and the capability for a process.
 * This function fills in the fields for those buffers that are not NULL
 * pointers.  Also, it will only fill in the ajax_pid if the caller asks
 * for the owner, since the two fields are obtained from the same transaction.
 * As a special case, if the user field is already filled in (is non-NULL and
 * does not begin with a '?' or '\0'), it is assumed that no information is
 * wanted unless the process belongs to the specified user.  We return non-
 * zero in this case, only for process belonging to the specified user.
 */
int
get_info_fields(proccap, ajax_pid, status, user, owner, cmd, showother)
	capability proccap;
	char *ajax_pid, *status, *user, *owner, *cmd;
	int showother;
{
	char ajax_pid_buff[9], status_buff[40], user_buff[40], cmd_buff[80];
	interval savetout;
	errstat err;
	char infobuf[100];
	int n;
	
	if (!status) status = status_buff;
	(void) strcpy(status,   "?");

	if (!user)
		user = user_buff, *user = '\0';
	if (user[0] == '\0' || user[0] == '?')
		(void) strcpy(user,     "?        ");

	if (!cmd) cmd = cmd_buff;
	(void) strcpy(cmd,      "?");

	if (!ajax_pid) ajax_pid = ajax_pid_buff;
	(void) strcpy(ajax_pid, "     ");

        savetout = timeout((interval)2000);
	err = gp_std_info(&proccap, infobuf, sizeof infobuf - 1, &n);
	(void) timeout(savetout);

	if (err != STD_OK) {
		sprintf(cmd, "(std_info: %s) ", err_why(err));
	} else {
		int found_status = 0;
		size_t status_len = 3;

		infobuf[n] = '\0';
		switch (infobuf[0]) {
		    case 'R':
			if (strncmp(infobuf, "Run", status_len)==0)
				found_status = 1;
			break;
		    case 'S':
			if (strncmp(infobuf, "Stp", status_len)==0)
				found_status = 1;
			break;
		    case 'D':
			if (strncmp(infobuf, "Die", status_len)==0)
				found_status = 1;
			break;
		}

		if (found_status) {
			char *after_status;
			char *after_user_name;
			/* Print the status of the process as a single
			 * upper-case letter, and pad the user name to
			 * a standard width: */
			status[0] = infobuf[0];
			status[1] = '\0';
			after_status = strchr(infobuf + status_len, ':');
			if (after_status == NULL) {
				after_status = "";
			}
			else {
				after_status++;
				while (isspace(*after_status))
					after_status++;
			}
			if ((after_user_name =
				      strchr(after_status, ':')) != 0) {
				*after_user_name++ = '\0';
				if (user[0] && user[0] != '?') {
				    /* If we only want a particular user: */
				    if (!showother &&
					    strcmp(user, after_status) != 0)
					return 0;
				} else
				    sprintf(user, "%-9s", after_status);

				while (isspace(*after_user_name))
					after_user_name++;
				sprintf(cmd, "%s", after_user_name);
			} else
				sprintf(cmd, "%s", after_status);
		} else
			sprintf(cmd, "%s", infobuf);
	}
	
	if (owner) {
		capability ownercap;
		char tmp_owner[80];

		(void) strcpy(owner,    "?");
		savetout = timeout((interval)2000);
		err = gp_trans(&proccap, pro_getowner(&proccap, &ownercap));
		(void) timeout(savetout);

		if (err != STD_OK) {
			sprintf(tmp_owner, "(getowner: %s)", err_why(err));
		}
		else if (NULLPORT(&ownercap.cap_port)) {
			sprintf(tmp_owner, "(no owner)");
		}
		else {
		    savetout = timeout((interval)2000);
		    err = gp_std_info(&ownercap,
			infobuf, sizeof infobuf - 1, &n);
		    (void) timeout(savetout);

		    if (err != STD_OK)
			sprintf(tmp_owner, "(%s)", err_why(err));
		    else {
			register char *cp;
			char *comma_p;
			size_t user_len;

			infobuf[n] = '\0';
			if (!strncmp(infobuf, "session server for", 18)
				      && (comma_p = strchr(infobuf, ','))!=0) {
			    *comma_p = '\0';
			    /* After the comma comes: " pid nnn" */
			    sprintf(ajax_pid, "%5s", comma_p+6);
			}
			(void) strcpy(tmp_owner, infobuf);

			/* If owner field contains "for $USER", delete
			 * it, since the same info is in the user field: */

			/* First get nonblank length of username: */
			user_len = strlen(user);
			if ((cp = strchr(user, ' ')) != 0)
			    user_len = cp - user;

			for (cp = tmp_owner; *cp; cp++) {
			    if (*cp == 'f' && cp[1] == 'o' &&
					cp[2] == 'r' && cp[3] == ' ') {
				/* If user field is currently "?", override it
				 * from value found in owner info: */
				if (user[0] == '?') {
				    (void) strcpy(user, cp+4);
				    *cp = '\0';
				}
				else if (strncmp(cp+4, user, user_len) == 0) {
				    *cp = '\0';
				}
				break;
			    }
			}
		    }
		}
		sprintf(owner, "%-18s", tmp_owner);
	}
	return 1;
}

errstat
list_one_host(hostname, phostcap)
	char *hostname;
	capability *phostcap;
{
	capability ps;
	capability proccap;
	struct dir_open *dp;
	char *pid;
	interval savetout;
	errstat err;
	char * hostp;
	int pr_done = 0;
	
	/* Only show the ppload info, if user asked for screen-oriented
	 * output by giving the -l flag: */
	if (lines_per_machine > 0)
		if (ppload(hostname, phostcap) != STD_OK)
			goto done;
	
	savetout = timeout((interval)1000);
	err = dir_lookup(phostcap, PS, &ps);
	(void) timeout(savetout);
	if (err != STD_OK) {
		printf("%s: can't find %s (%s)\n", hostname, PS, err_why(err));
		cur_line++;
		goto done;
	}
	
	if ((dp = dir_open(&ps)) == NULL) {
		printf("%s: can't open %s\n", hostname, PS);
		cur_line++;
		goto done;
	}

	/*
	 * Make sure that a hostname isn't too long - eg. because someone
	 * did aps -m /super/hosts/foodledo
	 */
	if ((hostp = strrchr(hostname, '/')) == NULL)
		hostp = hostname;
	else
		hostp++;
	
	while ((pid = dir_next(dp)) != NULL) {
		char ajax_pid[9], status[40], user_buff[40], owner[80], cmd[80];
		char host_n_ps[20];
		char *user = all_users ? user_buff : only_user;
		if (lines_per_machine == 0)
			sprintf(host_n_ps, "%-10.10s%3.3s", hostp, pid);
		else
			sprintf(host_n_ps, "%3.3s ", pid);
		if ((err = dir_lookup(&ps, pid, &proccap)) != STD_OK) {
			cur_line++;
			printf("%s (%s)\n", host_n_ps, err_why(err));
			continue;
		}
		user_buff[0] = '\0';  /* Set user to "", iff doing all_users */
		if (verbose) {
			if (get_info_fields(proccap, ajax_pid, status, user,
					    owner, cmd, all_users))
			{
				cur_line++;
				if (!pr_done && hostp != hostname) {
				    printf("%s\n", hostname);
				    pr_done = 1;
				}
				if (all_users)
					printf("%s %s %s %s %s %s\n",
						host_n_ps, ajax_pid,
						status, user, owner, cmd);
				else
					printf("%s %s %s %s %s\n",
						host_n_ps, ajax_pid,
						status, owner, cmd);
			}
		} else {
			if (get_info_fields(proccap, (char*)NULL, status, user,
					    (char *)NULL, cmd, all_users))
			{
				cur_line++;
				if (all_users)
					printf("%s %s %s %s\n",
						host_n_ps, status, user, cmd);
				else
					printf("%s %s %s\n",
						host_n_ps, status, cmd);
			}
		}
	}
	
	(void)dir_close(dp);
	err = STD_OK;
	
done:
	ideal_line += lines_per_machine;
	while (cur_line < ideal_line) {
		printf("\n");
		cur_line++;
	}
	
	return err;
}

errstat
list_hosts_by_name(argc, argv)
	int argc;
	char **argv;
{
	int i;
	errstat err;
	
	err = STD_OK;
	for (i = optind; i < argc; ++i) {
		capability hostcap;
		errstat err1;

		err1 = host_lookup(argv[i], &hostcap);
		if (err1 != STD_OK) {
			/* XXX need to integrate with cur_line stuff? */
			fprintf(stderr, "%s: can't find %s (%s)\n",
					    progname, argv[i], err_why(err1));
			(void) fflush(stderr);
		}
		else {
			err1 = list_one_host(argv[i], &hostcap);
		}
		if (err1 != STD_OK)
			err = err1;
	}
	return err;
}

errstat
list_one_dir(path)
	char *path;
{
	errstat err;
	capability hostlist;
	char *hostname;
	struct dir_open *dp;
	
	if ((err = name_lookup(path, &hostlist)) != STD_OK) {
		fprintf(stderr, "%s: can't find %s (%s)\n",
			progname, path, err_why(err));
		(void) fflush(stderr);
		return err;
	}
	if ((dp = dir_open(&hostlist)) == NULL) {
		fprintf(stderr, "%s: can't open directory %s\n",
			progname, path);
		(void) fflush(stderr);
		/* Probable cause is that it isn't a directory, so: */
		return STD_COMBAD;
	}
	while ((hostname = dir_next(dp)) != NULL) {
		capability hostcap;
		if (strchr(hostname, '.') != NULL)
			continue;
		err = dir_lookup(&hostlist, hostname, &hostcap);
		if (err != STD_OK)
			printf("%s: not found (%s)\n", hostname, err_why(err));
		else
			err = list_one_host(hostname, &hostcap);
		/* XXX report err to outside? integrate with cur_line? */
	}
	(void)dir_close(dp);
	return STD_OK;
}

errstat
list_host_dirs(argc, argv)
	int argc;
	char **argv;
{
	int i;
	errstat err;
	
	err = STD_OK;
	for (i = 0; i < argc; ++i) {
		errstat err1;
		err1 = list_one_dir(argv[i]);
		if (err1 != STD_OK)
			err = err1;
	}
	return err;
}


int
is_heterogeneous(dir)
char *dir;
{
	struct dir_open *dp;
	capability poolcap;
	char *subdir;
	int found_arch;

	if (name_lookup(dir, &poolcap) != STD_OK ||
	    (dp = dir_open(&poolcap)) == NULL)
	{
		fprintf(stderr, "%s: can't open %s\n", progname, dir);
		exit(1);
	}

	found_arch = 0;
	while ((subdir = dir_next(dp)) != NULL) {
		if (pd_known_arch(subdir)) {
			found_arch = 1;
			break;
		}
	}
	(void)dir_close(dp);
	return found_arch;
}

void
add_pool_dir(dir)
char *dir;
{
	if (num_pool_dirs < (sizeof pool_dirs)/(sizeof pool_dirs[0])) {
		pool_dirs[num_pool_dirs] = dir;
		num_pool_dirs++;
	} else {
		fprintf(stderr, "%s: too many pool dirs; %s ignored",
			progname, dir);
	}
}

void
set_pool_dirs(dir)
char *dir;
{
	struct dir_open *dp;
	char *subdir, *dirname;
	capability poolcap;
	errstat err;

	if (!is_heterogeneous(dir)) {
		/* simple: homogeneous pool dir */
		add_pool_dir(dir);
	} else {
		/* heterogeneous pool: add subdirectories */
		if (name_lookup(dir, &poolcap) != STD_OK ||
    		    (dp = dir_open(&poolcap)) == NULL)
		{
			fprintf(stderr, "%s: can't open %s\n", progname, dir);
			exit(1);
		}

		while ((subdir = dir_next(dp)) != NULL) {
			char info[30];
			int len;
			capability cap;

			/* only add it if it's a directory */
			err = dir_lookup(&poolcap, subdir, &cap);
			if (err != STD_OK) {
				fprintf(stderr, "%s: can't lookup %s/%s\n",
					progname, dir, subdir);
				continue;
			}
			err = std_info(&cap, info, (int) sizeof(info), &len);
			if ((err == STD_OK || err == STD_OVERFLOW) && (len > 0)
			    && (strncmp(info, OBJSYM_DIRECT, 1) == 0))
			{
				dirname = (char*) malloc(strlen(dir) +
							 strlen(subdir) + 3);
				if (dirname == NULL) {
					fprintf(stderr, "%s: out of memory\n",
						progname);
					exit(1);
				}
				sprintf(dirname, "%s/%s", dir, subdir);
				add_pool_dir(dirname);
			}
		}

		(void)dir_close(dp);
	}
}

static int
state_string(s)
char *s;
/* simple sanity check to avoid printing garbage in case of errors caused
 * by outdated ail stubs.
 */
{
	int i;

	for (i = 0; i < 10 && s[i] != '\0'; i++) {
		if (!isupper(s[i]) && !islower(s[i])) {
			return 0;
		}
	}
	return (1 <= i) && (i < 10);
}

errstat
list_session_procs(user, this_user)
char *user;
int   this_user;
{
	static int header_listed = 0;
	char session_path[256];
	struct proctab pt[MAXNPROC];
	int i, nproc;
	capability sescap;
	errstat err;
	interval savetout;

	if (this_user) {
		/* When listing the user's own processes, look up the session
		 * cap without going through the generic user directories.
		 */
		(void) sprintf(session_path, "%s", DEF_SESSION_CAP);
	} else {
		(void) sprintf(session_path, "%s/%s/%s",
			       DEF_USERDIR, user, DEF_SESSION_CAP);
	}
	err = name_lookup(session_path, &sescap);
	if (err != STD_OK) {
		if (all_users) {
			/* don't mind; user is just not logged in currently */
			return STD_OK;
		} else {
			fprintf(stderr, "%s: %s has no session server\n",
				progname, user);
			return err;
		}
	}

	savetout = timeout((interval)1000);
	nproc = MAXNPROC;
	err = ses_getpt(&sescap, pt, &nproc);
	(void) timeout(savetout);
	
	if (err != STD_OK) {
		if (err == RPC_NOTFOUND) {
			/* probably just an old session capability */
			return STD_OK;
		} else {
			fprintf(stderr,
				"%s: can't get process table for %s (%s)\n",
				progname, user, err_why(err));
			return err;
		}
	}
	
	if (!header_listed) {
		if (all_users) {
			printf("USER      ");
		}
		if (verbose) {
			printf("  PID  PPID  PGRP STATE      T OWNER              COMMAND\n");
		} else {
			printf("  PID STATE      COMMAND\n");
		}
		header_listed = 1;
	}

	for (i = 0; i < nproc; ++i) {
		char status[40], owner[80], cmd[80];

		if (all_users) {
			printf("%-9s ", user);
		}
		if (verbose) {
			printf("%5d %5d %5d %-10s",
			       pt[i].pid, pt[i].ppid, pt[i].pgrp, pt[i].state);
		} else {
			printf("%5d %-10s", pt[i].pid, pt[i].state);
		}

		/* Don't get and print info about execing processes.
		 * They are typically in a bad state, because the current
		 * capability hasn't been reported to the session server yet.
		 * Dead processes also have a bad capability in general.
		 */
		if (state_string(pt[i].state) &&
		    strcmp(pt[i].state, "EXECING") != 0 &&
		    strcmp(pt[i].state, "DEAD") != 0 &&
		    get_info_fields(pt[i].proc, (char *)NULL, status,
				    user, owner, cmd, 1))
		{
			if (verbose) {
				printf(" %s %s %s\n", status, owner, cmd);
			} else {
				printf(" %s\n", cmd);
			}
		} else {
			printf(" ? unknown\n");
		}
	}
	return STD_OK;
}

errstat
list_sessions()
{
	if (only_user != NULL) {
		return list_session_procs(only_user, this_user);
	} else {
		errstat err;
		char *path = DEF_USERDIR;
		capability userlist;
		char *username;
		struct dir_open *dp;
	
		/* Ask the session servers of all known users about their
		 * processes.
		 */
		if ((err = name_lookup(path, &userlist)) != STD_OK) {
			fprintf(stderr, "%s: can't find %s (%s)\n",
				progname, path, err_why(err));
			return err;
		}
		if ((dp = dir_open(&userlist)) == NULL) {
			fprintf(stderr, "%s: can't open directory %s\n",
				progname, path);
			/* Probable cause is that it isn't a directory, so: */
			return STD_COMBAD;
		}

		err = STD_OK;
		while ((username = dir_next(dp)) != NULL) {
			errstat new_err = list_session_procs(username, 0);

			if (new_err != STD_OK) {
				err = new_err;
			}
		}
		(void)dir_close(dp);
		return err;
	}
}

main(argc, argv)
	int argc;
	char **argv;
{
	int mflag = 0;
	errstat err;
	
	if (argc > 0 && argv[0] != NULL) {
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
			p = argv[0];
		else
			++p;
		if (*p != '\0')
			progname = p;
	}
	
	for (;;) {
		int opt = getopt(argc, argv, "ml:u:vas");
		if (opt == EOF)
			break;
		switch (opt) {
		case 'm': /* -m: arguments are machines, not directories */
			mflag++;
			super_user++; /* -m only makes sense with -s */
			break;
		case 'l': /* -l number: number of screen lines per machine */
			lines_per_machine = atoi(optarg);
			super_user++; /* -l only makes sense with -s */
			break;
		case 'u': /* -u user: show only processes of specified user */
			only_user = optarg;
			break;
		case 'a': /* -a: show all processes (not just the USER's) */
			all_users++;
			break;
		case 'v': /* -v: show machine version, process owners, etc. */
			verbose++;
			break;
		case 's': /* -s: act as super user: look at hosts directly */
			super_user++;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	if (!all_users && !only_user) {
		if ((only_user = getenv("USER")) == NULL &&
			(only_user = getenv("LOGNAME")) == NULL) {
			/* Oops, no current user, so turn on -a: */
			fprintf(stderr, "%s: no $USER or $LOGNAME. Using -a\n",
								progname);
			all_users++;
		} else {
			/* remember that only_user is in fact current user */
			this_user = 1;
		}
	}
	/* Assertion: all_users == 1 || only_user != NULL */
	
	if (super_user) {
		if (lines_per_machine == 0) {	/* Not using screen format */
			if (verbose) {
				printf("              POSIX S\n");
				printf("HOST       PS   PID T%s OWNER              COMMAND\n",
				       all_users ? " USER     " : "");
			} else {
				printf("              S\n");
				printf("HOST       PS T%s COMMAND\n", 
				       all_users ? " USER     " : "");
			}
		}

		if (mflag) {
			if (optind >= argc)
				err = list_one_dir(HOST_DIR);
			else
				err = list_hosts_by_name(argc, argv);
	    	}
		else {
			if (optind >= argc) {
				set_pool_dirs(POOL_DIR);
				err = list_host_dirs(num_pool_dirs, pool_dirs);
			} else {
				for (; optind < argc; optind++) {
					set_pool_dirs(argv[optind]);
				}
				err = list_host_dirs(num_pool_dirs, pool_dirs);
			}
		}
	}
	else {
		/* Non-super users only may use the information revealed
		 * by session servers.
		 */
		err = list_sessions();
	}
	
	if (err == STD_OK)
		exit(0);
	else
		exit(1);
	/*NOTREACHED*/
}
