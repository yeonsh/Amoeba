/*	@(#)cp.c	1.2	93/10/07 16:06:34 */
/* cp - copy files	Author: Andy Tanenbaum */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _VOIDSTAR
#define	_VOIDSTAR	void *
#endif

#ifdef AMOEBA
#define TRANSFER_UNIT    (29*1024)
#define CHECKONCLOSE
#endif

#ifndef TRANSFER_UNIT
#define TRANSFER_UNIT    16384
#endif

char cpbuf[TRANSFER_UNIT];
int isfloppy;			/* set to 1 for cp x /dev/fd? */


main(argc, argv)
int argc;
char *argv[];
{
  int fd1, fd2, m, s;
  struct stat sbuf, sbuf2;

  if (argc < 3) usage();

  /* Get the status of the last named file.  See if it is a directory. */
  s = stat(argv[argc-1], &sbuf);
  m = sbuf.st_mode & _S_IFMT;
  if (s >= 0 && m == _S_IFDIR) {
	/* Last argument is a directory. */
	cp_to_dir(argc, argv);
  } else if (argc > 3) {
	/* More than 2 arguments and last one is not a directory. */
	usage();
  } else if (s < 0 || m==_S_IFREG || m==_S_IFCHR || m==_S_IFBLK){
	/* Exactly two arguments.  Check for cp f1 f1. */
	if (equal(argv[1], argv[2])) {
		std_err("cp: cannot copy a file to itself\n");
		exit(-1);
	}

	/* Command is of the form cp f1 f2. */
	fd1 = open(argv[1], 0);
	if (fd1 < 0) {stderr3("cannot open ", argv[1], "\n"); exit(1);}
	fstat(fd1, &sbuf);
 	m = sbuf.st_mode & _S_IFMT;
	if (m == _S_IFDIR) {
		stderr3("<", argv[1], "> directory\n");
		exit(1);
	}
	fd2 = creat(argv[2], sbuf.st_mode & 0777);
	if (fd2 < 0) {stderr3("cannot create ", argv[2], "\n"); exit(2);}
	fstat(fd2, &sbuf2);
	if ( S_ISBLK(sbuf2.st_mode)) isfloppy = 1;
	copyfile(fd1, fd2, argv[2]);
  } else {
	stderr3("cannot copy to ", argv[2], "\n");
	exit(3);
  }
  exit(0);
}





cp_to_dir(argc, argv)
int argc;
char *argv[];
{
  int i, mode, fd1, fd2;
  char dirname[256], *ptr, *dp;
  struct stat sbuf;

  for (i = 1; i < argc - 1; i++) {
	fd1 = open(argv[i], 0);
	if (fd1 < 0) {
		stderr3("cannot open ", argv[i], "\n");
		continue;
	}

	ptr = argv[argc-1];
	dp = dirname;
	while (*ptr != 0) *dp++ = *ptr++;

	*dp++ = '/';
	ptr = argv[i];

	/* Concatenate dir and file name in dirname buffer. */
	while (*ptr != 0) ptr++;	/* go to end of file name */
	while (ptr > argv[i] && *ptr != '/') ptr--;	/* get last component*/
	if (*ptr == '/') ptr++;
	while (*ptr != 0) *dp++ = *ptr++;
	*dp++ = 0;
	fstat(fd1, &sbuf);
	mode = sbuf.st_mode & _S_IFMT;
	if (mode == _S_IFDIR) {
		stderr3("<", argv[i], "> directory\n");
		(void) close(fd1);
		continue;
	}
	fd2 = creat(dirname, sbuf.st_mode & 0777);
	if (fd2 < 0) {
		stderr3("cannot create ", dirname, "\n");
		continue;
	}
	copyfile(fd1, fd2, dirname);
  }
}





copyfile(fd1, fd2, name)
int fd1, fd2;
char *name;
{
  int n, m, mode;
  struct stat sbuf;

  do {
	n = read(fd1, cpbuf, TRANSFER_UNIT);
	if (n < 0) {std_err("cp: read error\n"); break;}
	if (n > 0) {
		m = write(fd2, cpbuf, n);
		if (m != n) {
			/* Write failed.  Don't keep truncated regular file. */
			perror("cp");
			fstat(fd2, &sbuf);	/* check for special files */
			mode = sbuf.st_mode & _S_IFMT;
			if (mode == _S_IFREG) unlink(name);
			exit(1);
		}
		if (isfloppy) sync();	/* purge the cache all at once */
	}
  } while (n == TRANSFER_UNIT);
  (void) close(fd1);
#ifndef CHECKONCLOSE
  (void) close(fd2);
#else /* CHECKONCLOSE */
  if (close(fd2) < 0) {
     perror("close");
     exit(1);
  }
#endif /* CHECKONCLOSE */
}

usage()
{
  std_err("Usage:  cp f1 f2;  or  cp f1 ... fn d2\n");
  exit(-1);
}


int equal(s1, s2)
char *s1, *s2;
{
  struct stat sb1, sb2;

 /* same file, different name? */
  stat(s1, &sb1);
  stat(s2, &sb2);
  if (memcmp((_VOIDSTAR)&sb1, (_VOIDSTAR)&sb2, sizeof(struct stat)) == 0)
      return(1);
 /* same file, same name? */
  while (1) {
	if (*s1 == 0 && *s2 == 0) return(1);
	if (*s1 != *s2) return(0);
	if (*s1 == 0 || *s2 == 0) return(0);
	s1++;
	s2++;
  }
}


stderr3(s1, s2, s3)
char *s1, *s2, *s3;
{
  std_err("cp: ");
  std_err(s1);
  std_err(s2);
  std_err(s3);
}
