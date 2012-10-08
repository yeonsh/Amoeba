/*	@(#)which.c	1.1	91/04/24 17:39:39 */
/* 29/11/89 - gregor@cs.vu.nl modified to use strchr instead of index  */

#define DELIMETER ':'

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
char *getenv();

int
main(ac,av)
char **av;
{
    char *path, *cp;
    char buf[400];
#ifndef AMOEBA
    char prog[400];
#endif
    char patbuf[512];
    int quit, none;

    if (ac < 2) {
        fprintf(stderr, "Usage: %s cmd [cmd, ..]\n", *av);
        exit(1);
    }
    av[ac] = 0;
    for(av++ ; *av; av++) {

        quit = 0;
        none = 1;
	if ((path = getenv("PATH")) == NULL) {
	    fprintf(stderr, "Null path.\n");
	    exit(0);
	}
        strcpy(patbuf, path);
        path = patbuf;
        cp = path;

        while(1) {
            cp = strchr(path, DELIMETER);
            if (cp == NULL) 
                quit++;
            else
                *cp = '\0';

	    if (strcmp(path,"") == 0 && quit == 0) {
		sprintf(buf, "./%s", *av);
	    } else 
		sprintf(buf, "%s/%s", path, *av);
	
            path = ++cp;

            if (access(buf, 1) == 0) {
                printf("%s\n", buf);
#ifdef AMOEBA
		break;
#else
                none = 0;
#endif
            }
	    
#ifndef AMOEBA
            sprintf(prog, "%s.%s", buf, "prg");
            if (access(prog, 1) == 0) {
                printf("%s\n", prog);
                none = 0;
            }
	    
            sprintf(prog, "%s.%s", buf, "ttp");
            if (access(prog, 1) == 0) {
                printf("%s\n", prog);
                none = 0;
            }
	    
            sprintf(prog, "%s.%s", buf, "tos");
            if (access(prog, 1) == 0) {
                printf("%s\n", prog);
                none = 0;
            }
#endif
            if (quit) {
                if (none)
                    printf("No %s in %s\n", *av, getenv("PATH"));
                break;
            }
        }
    }
    exit(0);
}
