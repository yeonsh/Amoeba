/*	@(#)file.c	1.4	96/03/01 16:52:44 */
/* file - report on file type.		Author: Andy Tanenbaum */

/* #include <ar.h> */
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#ifdef AMOEBA
#include <amoeba.h>
#include <server/process/proc.h>
#define ARMAG	0177454	/* aal magic number */
#define O_MAGIC	0x0201	/* object file magic number */

#else
/* ar.c header file V7 */
#define ARMAG	0177545l
struct ar_hdr {
	char ar_name[14];
	long ar_date;		/* not Intel format */
	char ar_uid;
	char ar_gid;
	int ar_mode;
	long ar_size;		/* not Intel format */
};
#endif


#define BLOCK_SIZE 1024
#define A_OUT 001401		/* magic number for executables */
#define SPLIT 002040		/* second word on split I/D binaries */
#define XBITS 00111		/* rwXrwXrwX (x bits in the mode) */
#define ENGLISH 25		/* cutoff for determining if text is Eng.*/
char buf[BLOCK_SIZE];

/* Forward */
static void file _ARGS((char *));

main(argc, argv)
int argc;
char *argv[];
{
/* This program uses some heuristics to try to guess about a file type by
 * looking at its contents.
 */

  int i;

  if (argc < 2) usage();
  for (i = 1; i < argc; i++) file(argv[i]);
  exit(0);
}

#ifdef AMOEBA

/* architectures (going to be) supported by Amoeba: */
char *am_arch[] = {
    "i80386",
    "mc68000",
    "mipsel",
    "mipseb",
    "sparc",
    "vax",
    0
};

char *
get_arch(pdbuf)
process_d *pdbuf;
{
    char **archp;

    /* look if one architecture matches the pd_magic field */
    for (archp = &am_arch[0]; *archp != 0; archp++) {
	if (strncmp(pdbuf->pd_magic, *archp, ARCHSIZE) == 0) {
	    /* check that the rest is all '\0's */
	    char *p;

	    for (p = &pdbuf->pd_magic[strlen(*archp)];
		 p <= &pdbuf->pd_magic[ARCHSIZE-1];
		 p++)
	    {
		if (*p != '\0') {
		    return 0;
		}
	    }

	    /* passed all checks */
	    return(*archp);
	}
    }

    /* not found in architecture list */
    return 0;
}

#endif
      
/* Terminating actions when we think found it (like Archimedes) */
#define Eureka(filetype)	{ printf(filetype); return; }
#define Eureka2(filetype)	{ printf(filetype); (void) close(fd); return; }

static void
file(name)
char *name;
{
  int i, fd, n, magic;
  int mode, nonascii, special, funnypct, etaoins;
#ifndef AMOEBA
  int second;
  int symbols;
#endif
  long engpct;
  char c;
  struct stat st_buf;

  printf("%s: ", name);

  n = stat(name, &st_buf);
  if (n < 0) {
      Eureka("cannot stat\n");
  }
  mode = st_buf.st_mode;

  /* Check for directories and special files. */
  if ( S_ISDIR(mode)) {
      Eureka("directory\n");
  }

  if ( S_ISFIFO(mode)) {
      Eureka("fifo\n");
  }

  if ( S_ISCHR(mode)) {
#ifndef AMOEBA
      Eureka("character special file\n");
#else
      Eureka("server capability\n");
#endif
  }

#ifndef AMOEBA
  if ( (mode & S_IFMT) == S_IFBLK) {
      Eureka("block special file\n");
  }
#endif


  /* Open the file, stat it, and read in 1 block. */
  fd = open(name, 0);
  if (fd < 0) {
      Eureka("cannot open\n");
  }

  n = read(fd, buf, BLOCK_SIZE);
  if (n < 0) {
      Eureka2("cannot read\n");
  }

  /* Check to see if file is an archive. */
  magic = ((buf[1] & 0377) << 8) | (buf[0]&0377);
  if (magic == ARMAG) {
      Eureka2("archive\n");
  }

  /* Check to see if file is an executable binary. */
#ifdef AMOEBA
  if (magic == O_MAGIC) {	/* set by all ack assemblers */
      Eureka2("loadable object\n");
  } else {
      process_d pdbuf;
      char *arch;

      /* we deam it to be an amoeba process descriptor when the
       * pd_magic field contains an architecture string known to us.
       */

      if (lseek(fd, 0L, 0) == 0 &&
	  read(fd, (char *) &pdbuf, sizeof(pdbuf)) == sizeof(pdbuf) &&
	  (arch = get_arch(&pdbuf)) != 0)
      {
	  printf("%s executable\n", arch);
	  (void) close(fd);
	  return;
      }
  }
#else
  if (magic == A_OUT) {
	/* File is executable.  Check for split I/D. */
	printf("executable");
	second = (buf[3]<<8) | (buf[2]&0377);
	if (second == SPLIT)
		printf("   separate I & D space");
	else
		printf("   combined I & D space");
	symbols = buf[28] | buf[29] | buf[30] | buf[31];
	if (symbols != 0)
		printf("   not stripped\n");
	else
		printf("   stripped\n");
	(void) close(fd);
 	return;
  }
#endif

  /* Check for ASCII data and certain punctuation. */
  nonascii = 0;
  special = 0;
  etaoins = 0;
  for (i = 0; i < n; i++) {
	c = buf[i];
  	if (c & 0200) nonascii++;
	if (c == ';' || c == '{' || c == '}' || c == '#') special++;
	if (c == '*' || c == '<' || c == '>' || c == '/') special++;
	if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
	if (c == 'e' || c == 't' || c == 'a' || c == 'o') etaoins++;
	if (c == 'i' || c == 'n' || c == 's') etaoins++;
  }	

  if (nonascii == 0) {
	/* File only contains ASCII characters.  Continue processing. */

	if (n == 0) {	/* watch out for division by n */
	    Eureka2("empty file\n");
	}

#ifdef AMOEBA
	/* The (simulated) x-bit is currently set for every file,
	 * so that isn't of much help.
	 * We'll assume it's a shell script when it started with "#!".
	 */
	if (buf[0] == '#' && buf[1] == '!') {
	    Eureka2("shell script\n");
	}
#else
	/* Check to see if file is a shell script. */
	if (mode & XBITS) {
		/* Not a binary, but executable.  Probably a shell script. */
	    Eureka2("shell script\n");
	}
#endif

	funnypct = 100 * special/n;
	engpct = 100L * (long) etaoins/n;
	if (funnypct > 1) {
		printf("C program\n");
	} else {
		if (engpct > (long) ENGLISH)
			printf("English text\n");
		else
			printf("ASCII text\n", engpct);
	}
	(void) close(fd);
	return;
  }

  /* Give up.  Call it data. */
  Eureka2("data\n");
}

usage()
{
  printf("Usage: file name ...\n");
  exit(1);
}

