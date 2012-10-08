/*	@(#)mv.c	1.2	93/10/07 16:08:39 */
/* A mv that relies totally on rename() to check for errors. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef errno /* just in case errno is a macro? */
extern int errno;
#endif

extern int optind;
extern char *optarg;

char *progname = "MV";

void
usage()
{
	fprintf(stderr, "usage: %s f1 f2; or %s f1 [f2] ... dir\n",
							progname, progname);
	exit(2);
}

void
myperror(from, to)
	char *from, *to;
{
	int save = errno;
	
	fprintf(stderr, "%s %s ", progname, from);
	fflush(stderr);
	errno = save;
	perror(to);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	/* Assign `basename argv[0]` to progname */
	if (argc > 0 && argv[0] != NULL) {
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
			p = argv[0];
		else
			++p;
		if (*p != '\0')
			progname = p;
	}
	
	/* Scan command line options */
	for (;;) {
		int opt = getopt(argc, argv, "");
		if (opt == EOF)
			break;
		switch (opt) {
		default:
			usage();
		}
	}
	
	if (optind+1 >= argc) {
		fprintf(stderr, "%s: not enough arguments\n", progname);
		usage();
	}
	
	{
		int sts = 0;
		
		if (isdir(argv[argc-1])) {
			int i;
			for (i = optind; i < argc-1; ++i)
				if (movetodir(argv[i], argv[argc-1]) < 0)
					sts = 1;
		}
		else {
			if (optind+2 < argc) {
				fprintf(stderr,
					"%s: %s is not a directory\n",
					progname, argv[argc-1]);
				usage();
			}
			if (rename(argv[optind], argv[optind+1]) < 0) {
				myperror(argv[optind], argv[optind+1]);
				sts = 1;
			}
		}
		return sts;
	}
}

int
movetodir(file, dir)
	char *file, *dir;
{
	char *dest;
	int dlen;
	char *base;
	int err;
	
	base = strrchr(file, '/');
	if (base == NULL)
		base = file;
	else
		base++;
	
	dlen = strlen(dir);
	dest = malloc(dlen + strlen(base) + 2);
	if (dest == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		return -1;
	}
	strcpy(dest, dir);
	if (dest[dlen-1] != '/')
		dest[dlen++] = '/';
	strcpy(dest+dlen, base);
	err = rename(file, dest);
	if (err < 0)
		myperror(file, dest);
	free(dest);
	return err;
}

int
isdir(path)
	char *path;
{
	struct stat sb;
	
	if (stat(path, &sb) < 0)
		return 0; /* Not an error -- probably doesn't exist */
	else
		return S_ISDIR(sb.st_mode);
}
