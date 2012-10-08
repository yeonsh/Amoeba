/*	@(#)time.c	1.3	93/11/18 12:58:25 */
/*
 * time - time a command
 *   Gregory J. Sharp, Oct 1993 - largely poached from pd kornshell and the
 *				  newproc(L) manpage
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <sys/times.h>

extern char **	environ;


static char *
convtime(t)
clock_t t;
{
    static char temp[20];
    register int i;
    register char *cp = temp + sizeof(temp);

    if (CLK_TCK != 100) {           /* convert to 1/100'ths */
	t = (t < 1000000000 / CLK_TCK) ?
				(t * 100) / CLK_TCK : (t / CLK_TCK) * 100;
    }

    *--cp = '\0';
    *--cp = 's';
    for (i = -2; i <= 0 || t > 0; i++) {
	if (i == 0)
	    *--cp = '.';
	*--cp = '0' + (char)(t % 10);
	t /= 10;
    }
    return cp;
}

/* Ignore the following signals */
#define	SIGMASK		(sigmask(SIGQUIT) | sigmask(SIGINT))

int
execute(cmd, envp)
char * cmd[];
char * envp[];
{
    int pid;
    int status;

    pid = newprocp(cmd[0], cmd, envp, -1, (int *) 0, SIGMASK);
    if (pid < 0) {
	perror(cmd[0]);
	exit(1);
    }
    if (waitpid(pid, &status, 0) < 0) {
	perror("waitpid");
	exit(1);
    }
    return status >> 8;
}


main(argc, argv)
int argc;
char *argv[];
{
    struct tms pre_buf, post_buf;
    int status;
    clock_t start_time, end_time;

    if (argc == 1)
	exit(0);

    /* Get time at start of run. */
    start_time = times(&pre_buf);
    status = execute(argv + 1, environ);
    end_time = times(&post_buf);

    /* Print results. */
    fprintf(stderr, "real %s\n", convtime(end_time - start_time));
    fprintf(stderr, "user %s\n",
			convtime(post_buf.tms_cutime - pre_buf.tms_cutime));
    fprintf(stderr, "sys  %s\n",
			convtime(post_buf.tms_cstime - pre_buf.tms_cstime));
    exit(status);
}
