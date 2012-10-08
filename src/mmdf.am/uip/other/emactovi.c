/*	@(#)emactovi.c	1.1	92/05/14 10:43:14 */
/*
 * emactovi - convert from EMAC-style two file input to single file for vi
 *
 * When EMACS is presented with 2 files, it puts them both on the screen
 * in seperate windows. Vi, in the same situation, starts up on only the
 * first file. This filter copies the 2 files together, adding a reference
 * line and inclusion marks (i.e. '>').
 *
 * usage:
 *	emactovi editor file1 file2
 *
 * Emactovi processes file1 into file2 and then exec's editor on file2.
 *
 * Author
 *	Edward C. Bennett <edward@engr.uky.edu>
 */
#include	<stdio.h>
#ifdef SYS5
#include	<string.h>
#else	/* SYS5 */
#include	<strings.h>
#endif	/* SYS5 */
#include	<ctype.h>

main(argc, argv)
int argc;
char **argv;
{
	register	FILE	*fpi, *fpo;
	register	char	*p;
	char			 buf[BUFSIZ];
	FILE			*fopen();
	char			*program;

	if (argc != 3)
		exit(1);

	program = "emactovi";	/* Notice that argv[0] is our editor of choice,
					NOT the name of this program. */

	/*
	 * These should never fail, but let's be careful out there.
	 */
	if ((fpi = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: cannot open %s\n", program, argv[1]);
		exit(1);
	}
	if ((fpo = fopen(argv[2], "w")) == NULL) {
		fprintf(stderr, "%s: cannot open %s\n", program, argv[2]);
		exit(1);
	}

	while (fgets(buf, BUFSIZ, fpi) != (char *)NULL) {
		if (strncmp(buf, "Date:", 5) == 0) {
			if ((p = index(buf, '\n')) != (char *)NULL)
				*p = '\0';	/* remove new-line */

			p = &(buf[5]);
			while (isspace(*p))
				p++;

			fprintf(fpo, "In your letter dated %s, you wrote:\n>\n", p);
			break;
		}
		/* End of header, no Date: line found */
		if (buf[0] == '\n') {
			fprintf(fpo, "You recently wrote:\n>\n");
			break;
		}
	}

	if (fseek(fpi, 0, 0) != 0) {	/* rewind input file */
		fprintf(stderr, "%s: fseek() error on %s\n", program, argv[1]);
		exit(1);
	}
	while (fgets(buf, BUFSIZ, fpi) != (char *)NULL)
		fprintf(fpo, ">%s", buf);

	fclose(fpi);
	fclose(fpo);

	execlp(argv[0], argv[0], argv[2], (char *)0);
	exit(1);
}
