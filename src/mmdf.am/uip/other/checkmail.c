/*	@(#)checkmail.c	1.1	91/11/21 11:45:29 */
/*
 *			CHECKMAIL
 *
 *  Checks mail queues for messages from invoker (or all for mail su's)
 *
 *  Oct 84	H. Walter		initial version
 *  Nov 85	C. Partridge		fix arginit() to not dereference 0!
 *  Dec 85	D. Kingston		swallowed functions of checkmsgs
 */

#include "util.h"
#include "mmdf.h"
#include <sys/stat.h>
#include <pwd.h>
#include "ch.h"
#include "msg.h"
#include "adr_queue.h"

extern	char	*quedfldir,		/* home directory for mmdf	*/
		*aquedir,		/* subordinate address directory */
		*mquedir;		/* subordinate msg directory	*/

DIR	*ovr_dirp;			/* handle on the queue directory */
char	msg_sender[ADDRSIZE + 1];	/* return address of current message */
int	realuid;			/* Invoker uid */
int	alladdrs = 0;
int	allmsgs = 0;
int	fast = 0;
char	bufout[BUFSIZ];

extern	int	errno;			/* system error number */

main(argc, argv)
int argc;
char *argv[];
{
	setbuf(stdout, bufout);
	siginit();			/* standard interrupt initialization */

	/* distinguish different delivers */
	arginit(argc, argv);

	/* get the real uid */
	realuid = getuid();

	if (allmsgs)
		setuid(realuid); /* Let unix protection do the hard work */

	mmdf_init(argv[0]);
	mn_dirinit();		/* get right working directory */

	ovr_queue();		/* do the entire mail queue */

	exit(RP_OK);		/* "normal" end, even if pgm_bakgrnd */
}

LOCFUN
arginit(argc, argv)
int	argc;
char	**argv;
{
	register	int	ch;

	while ((ch = getopt(argc, argv, "cfm")) != EOF) {
		switch (ch) {
		case 'a':
			alladdrs++;
			break;

		case 'f':
			fast++;		/* Don't get subject line */
			break;

		case 'm':
			allmsgs++;
			break;

		default:
			fprintf(stderr, "Usage: checkmail [-afm]\n");
			exit(1);
		}
	}
}

LOCFUN
mn_dirinit()		/* get to the working directory */
{
	if (chdir(quedfldir) < OK) {
		printf("can't chdir to %s\n", quedfldir);
		exit(-1);
	}
}

ovr_queue()			/* Process entire message queue */
{
	struct	dirent	*dp;

	if ((ovr_dirp = opendir(aquedir)) == NULL) {
		printf("can't open address queue\n");
		exit(-1);
	}

	while ((dp = readdir(ovr_dirp)) != NULL) {
		/* straight linear processing */
		if (equal("msg.", dp->d_name, 4)) {
			msg_proc(dp->d_name);
		}
	}

	closedir(ovr_dirp);
}

/* msg_cite() is defined in adr_queue.h */

msg_proc(thename)		/* get, process & release a msg */
char thename[];
{
	FILE		*msg_tfp;
	Msg		themsg;
	struct	stat	sb;
	char		msgname[LINESIZE];
	char		linebuf[LINESIZE];

	strcpy(themsg.mg_mname, thename);
	sprintf(msgname, "%s%s", mquedir, thename);

	if (stat(msgname, &sb) < 0) {
		perror(msgname);
		return;
	}
	/* Invoker's file? */
	if (allmsgs || sb.st_uid == realuid)
		if (mq_rinit((Chan *) 0, &themsg, msg_sender) == OK) {
			printf("Message %s: posted %.24s\n",
			    thename, ctime(&themsg.mg_time));
			if (allmsgs)
				printf("Return address: %s\n", msg_sender);
			if (!fast) {
				if ((msg_tfp = fopen(msgname, "r")) == NULL)
					printf("can't open msg file '%s'\n",msgname);
				else {
					while (fgets(linebuf, sizeof(linebuf), msg_tfp) != NULL) {
						if (prefix("subject:", linebuf) == TRUE) {
							printf("%s", linebuf);
							break;
						}
					}
					fclose(msg_tfp);
				}
			}
			adr_each();
			mq_rkill(OK);
		}
		else {
			printf("Message %s:  busy\n", thename);
		}
}

adr_each()			/* do each address */
{
	struct	adr_struct	theadr;

	for (;;) {
		switch (mq_radr(&theadr)) {
		case NOTOK:	/* rest of list must be skipped */
		case DONE:	/* end of list */
		default:
			printf("\n");
			return;

		case OK:
			if (theadr.adr_delv != ADR_DONE) {
				if (theadr.adr_host && theadr.adr_host[0] !=(char) '\0')
					printf("   %s(via %s): not yet sent\n",
					    theadr.adr_local, theadr.adr_host);
				else
					printf("   %s: not yet sent\n", theadr.adr_local);
				fflush(stdout);
			} else if (alladdrs) {
				if (theadr.adr_host && theadr.adr_host[0] !=(char) '\0')
					printf("   %s(via %s): sent\n",
					    theadr.adr_local, theadr.adr_host);
				else
					printf("   %s: sent\n", theadr.adr_local);
				fflush(stdout);
			}
		}
	}
}
