/*	@(#)mailid.c	1.1	92/05/14 10:42:34 */
/*
 *	M A I L I D . C
 *
 *	This program is designed to be used by other programs and/or
 *	shell scripts to obtain the mail id for a given person.
 *	If no userid is give, the userid of the current real uid
 *	is assumed.
 */
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

main (argc, argv)
int	argc;
char	**argv;
{
	register char *user;
	register char *mailid;
	char *getmailid();

	mmdf_init(argv[0]);
	if (argc < 2) {
		register struct passwd *pw;
		struct passwd *getpwuid();

		if ((pw = getpwuid (getuid())) == NULL) {
			fprintf (stderr, "prmailid: No user with uid %d.\n",
				 getuid());
			exit (9);
		}
		user = pw->pw_name;
	} else
		user = argv[1];

	if ((mailid = getmailid(user)) == NULL) {
		fprintf (stderr, "prmailid: No mailid for user '%s'.\n",
			 user);
		exit (8);
	}
	puts(mailid);
	exit (0);
}
