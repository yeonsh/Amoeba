/*	@(#)getoptx.c	1.2	94/04/07 14:50:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**     @ (#)getopt.c    2.5 (smail) 9/15/87
*/

/*
 *  NOTE: this source has been hacked to allow * in the optstring,
 *  meaning "an optional argument which must follow the option
 *  character without a blank." Read the comments following with 
 *  that in mind.
 *    bill davidsen (davidsen@crdos1.crd.ge.com) June 1989
 */

#define OptArgFlg '*'

/*
 * Here's something you've all been waiting for:  the AT&T public domain
 * source for getopt(3).  It is the code which was given out at the 1985
 * UNIFORUM conference in Dallas.  I obtained it by electronic mail
 * directly from AT&T.  The people there assure me that it is indeed
 * in the public domain.
 * 
 * There is no manual page.  That is because the one they gave out at
 * UNIFORUM was slightly different from the current System V Release 2
 * manual page.  The difference apparently involved a note about the
 * famous rules 5 and 6, recommending using white space between an option
 * and its first argument, and not grouping options that have arguments.
 * Getopt itself is currently lenient about both of these things White
 * space is allowed, but not mandatory, and the last option in a group can
 * have an argument.  That particular version of the man page evidently
 * has no official existence, and my source at AT&T did not send a copy.
 * The current SVR2 man page reflects the actual behavor of this getopt.
 * However, I am not about to post a copy of anything licensed by AT&T.
 */

/* for SysV use strchr instead of index */
#define USG
#ifdef USG
#define index  strchr
#define rindex strrchr
#endif

#include "getoptx.h"

/*LINTLIBRARY*/
#define NULL   0
#define ERR(s, c)      if(opterr){\
       extern int write();\
       char errbuf[2];\
       errbuf[0] = c; errbuf[1] = '\n';\
       (void) write(2, argv[0], strlen(argv[0]));\
       (void) write(2, s, strlen(s));\
       (void) write(2, errbuf, 2);}

extern char *index();

int    opterr = 1;
int    optind = 1;
int    optopt;
char   *optarg;

int
getopt(argc, argv, opts)
int    argc;
char   **argv, *opts;
{
       static int sp = 1;
       register int c, ch;
       register char *cp;

       if(sp == 1)
               if(optind >= argc ||
                  argv[optind][0] != '-' || argv[optind][1] == '\0')
                       return(EOF);
               else if(strcmp(argv[optind], "--") == NULL) {
                       optind++;
                       return(EOF);
               }

       optopt = c = argv[optind][sp];
       if(c == ':' || c == OptArgFlg || (cp=index(opts, c)) == NULL) {
               ERR(": illegal option -- ", c);
               if(argv[optind][++sp] == '\0') {
                       optind++;
                       sp = 1;
               }
               return('?');
       }
       if((ch = *++cp) == OptArgFlg) {
               if(argv[optind][sp+1] != '\0')
                       optarg = &argv[optind++][sp+1];
               else {
                       optarg = "";
                       optind++;
		       /* sp = 1; */
               }
               sp = 1; /* fixed, CV */
       }
       else if(ch == ':') {
               if(argv[optind][sp+1] != '\0')
                       optarg = &argv[optind++][sp+1];
               else if(++optind >= argc) {
                       ERR(": option requires an argument -- ", c);
                       sp = 1;
                       return('?');
               } else
                       optarg = argv[optind++];
               sp = 1;
       } else {
               if(argv[optind][++sp] == '\0') {
                       sp = 1;
                       optind++;
               }
               optarg = NULL;
       }
       return(c);
}

