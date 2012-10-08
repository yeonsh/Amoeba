/*	@(#)dread.c	1.4	96/02/27 10:24:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <module/disk.h>
#include <module/name.h>

extern int optind;
extern char *optarg;

disk_addr	start_block = 0L;
long		num_block   = 1L;
int		blocksize   = 512L;
bufptr		buf         = NULL;

usage(cmd)
char *cmd;
{
   fprintf(stderr, 
	"Usage: %s [-sstart_block] [-nnum_block] [-bblocksize] capability\n", 
	cmd);
   fprintf(stderr, "     : defaults: start_block %d\n", start_block);
   fprintf(stderr, "               : num_block %d\n", num_block);
   fprintf(stderr, "               : blocksize %d\n", blocksize);
   exit(1);
}


main(argc, argv)
char *argv[];
{
   capability cap;
   errstat err;
   int opt, i;
   int l2vblksiz;

   while ((opt = getopt(argc, argv, "s:n:b:")) != EOF)
   switch (opt) {
      case 's': 
	start_block = strtol(optarg, (char **) 0, 0);
	break;
      case 'n': 
	num_block   = strtol(optarg, (char **) 0, 0);
        break;
      case 'b': 
	blocksize   = (int) strtol(optarg, (char **) 0, 0);
	break;
      default:
        usage(argv[0]);
   }

   if (optind != argc - 1) 
      usage(argv[0]);

   if (strcmp(argv[optind], "-") == 0) {
      if (read(fileno(stdin), (char *)&cap, sizeof(cap)) != sizeof(cap)) {
         perror("read - stdin");
         exit(1);
      }
   } 
   else {
      if ((err = name_lookup(argv[optind], &cap)) != STD_OK) {
         fprintf(stderr, "%s: can't get capability for %s: %s\n",
					argv[0], argv[optind], err_why(err));
         exit(1);
      }
   }

   l2vblksiz = 0;
   while (((1<<l2vblksiz) & blocksize) == 0)
      l2vblksiz++;

   if ((blocksize & ~(1<<l2vblksiz)) != 0) {
      fprintf(stderr, "%s: only blocksizes which are a power of two are allowed\n", argv[0]);
      exit(1);
   }

   if ((buf = (bufptr) malloc((size_t) blocksize)) == NULL) {
      perror("malloc");
      exit(1);
   }

   for (i = 0; i != num_block; i++) {
      err = disk_read(&cap, l2vblksiz, start_block + i, (disk_addr) 1, buf);
      if (err != STD_OK) {
	 fprintf(stderr, "%s: disk_read error: %s\n", argv[0], err_why(err));
	 exit(1);
      }

      if (write(fileno(stdout), (char *) buf, blocksize) != blocksize) {
	 perror("write - stdout");
	 exit(1);
      }
   }
   exit(0);
}
