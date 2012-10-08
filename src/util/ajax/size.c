/*	@(#)size.c	1.3	96/02/27 12:44:43 */
/* size - tell size of an object file		Author: Andy Tanenbaum */


#include <stdlib.h>

#ifndef AMOEBA
#define HLONG            8	/* # longs in the header */
#define TEXT             2
#define DATA             3
#define BSS              4
#define CHMEM            6
#define MAGIC       0x0301	/* magic number for an object file */
#define SEPBIT  0x00200000	/* this bit is set for separate I/D */
#endif

int print_name = 0;		/* true when filename should be printed also */
int heading;			/* set when heading printed */
int error;

main(argc, argv)
int argc;
char *argv[];
{
  int i;

  if (argc == 1) {
	size("a.out");
	exit(error);
  }

  print_name = argc > 2;
  for (i = 1; i < argc; i++)
	size(argv[i]);
  exit(error);
}

#ifdef AMOEBA

#include <amoeba.h>
#include <stderr.h>
#include <stdio.h>
#include <errno.h>
#include <module/name.h>
#include <module/proc.h>

/* next structure represents information about segments of an amoeba binary */
struct seg_info {
    char *name;
    int type;	/* bitmap of segment type info */
} seg_info[] = {
    {"text",	MAP_GROWNOT  | MAP_TYPETEXT | MAP_READONLY  },
    {"data",	MAP_GROWNOT  | MAP_TYPEDATA | MAP_READWRITE },
    {"bss",	MAP_GROWUP   | MAP_TYPEDATA | MAP_READWRITE },
    {"stack",	MAP_GROWDOWN | MAP_TYPEDATA | MAP_READWRITE | MAP_SYSTEM}
 /* {"symbol",	MAP_SYMTAB } */
};

#define EXTRABSS (16*1024)	/* don't count extra bss, it gives no info */

size(name)
char *name;
{
    capability progcap;
    process_d *pd;
    segment_d *sd;
    int i, fd;
    long tot_size = 0;
    
    /* first see if we can read it */
    if ((fd = open(name, 0)) < 0) {
	fprintf(stderr, "size: %s: cannot open\n", name);
	error++;
	return;
    } else {
	close(fd); /* pd_read does a direct b_read() */
    }

    /* Note: We examine the amoeba process descriptor rather than the
     * a.out header, because the latter is not necessarily present.
     * (And even if it is, it's more system dependent).
     */
    if (name_lookup(name, &progcap) == STD_OK &&
	(pd = pd_read(&progcap)) != 0) {

	if (heading++ == 0) {
	    printf("text\tdata\tbss\tstack\tdec\thex\n");
	}

	/* Loop over segments, but ignore symbol table (the sd_len field
	 * for it isn't currently set right anyway).
	 * Do show the stack size; it might be interesting because it is
	 * currently static.
	 */
	sd = PD_SD(pd);
	for (i = 0; i < pd->pd_nseg && i < 4; i++) {
	    if ((sd->sd_type & seg_info[i].type) != seg_info[i].type) {
		/* sanity check */
		printf("<bad>\t");
	    } else {
		printf("%x\t", sd->sd_len);
		if (i == 2 && sd->sd_len > EXTRABSS) {	/* don't add EXTRABSS*/
		    tot_size += sd->sd_len - EXTRABSS;
		} else if (i != 3) {			/* don't add stack */
		    tot_size += sd->sd_len;
		}
	    }
	    sd++;
	}
	printf("%ld\t%lx", tot_size, tot_size);
	if (print_name) {
	    printf("\t%s\n", name);
	} else {
	    printf("\n");
	}
    } else {
	fprintf(stderr, "size: %s: not an object file\n", name);
	error++;
    }
}

#else

size(name)
char *name;
{
  int fd, separate;
  long head[HLONG], dynam, allmem;

  if ( (fd = open(name, 0)) < 0) {
	stderr3("size: can't open ", name, "\n");
	return;
  }

  if (read(fd, head, sizeof(head)) != sizeof(head) ) {
	stderr3("size: ", name, ": header too short\n");
  	error = 1;
	close(fd);
	return;
  }

  if ( (head[0] & 0xFFFFL) != MAGIC) {
	stderr3("size: ", name, " not an object file\n");
	close(fd);
	return;
  }

  separate = (head[0] & SEPBIT ? 1 : 0);
  dynam = head[CHMEM] - head[TEXT] - head[DATA] - head[BSS];
  if (separate) dynam += head[TEXT];
  allmem = (separate ? head[CHMEM] + head[TEXT] : head[CHMEM]);
  if (heading++ == 0) 
	printf("  text\t  data\t   bss\t stack\tmemory\n");
  printf("%6ld\t%6ld\t%6ld\t%6ld\t%6ld\t%s\n",
		head[TEXT], head[DATA], head[BSS],
		dynam, allmem, name);
  close(fd);
}


stderr3(s1, s2, s3)
char *s1, *s2, *s3;
{
  std_err(s1);
  std_err(s2);
  std_err(s3);
  error = 1;
}

#endif
