/*	@(#)gpwlnam.c	1.3	96/02/27 09:57:36 */
#include "util.h"
#include <pwd.h>
#include <sys/stat.h>
#include "ampolicy.h"

/* modified getpwnam(), to do lexical (case-independent) comparison */

struct passwd *
gpwlnam(name)
char *name;
{
    struct passwd *getpwent(), *getpwnam();
    char lownam[30];
    register struct passwd *p;
    register int ind;
    char userdir[80];
    struct stat buf;



    if ((p = getpwnam(name)) != NULL)	/* optimization for hashed passwd */
	return(p);

        
    for (ind = 0; ind < ((sizeof lownam) - 1) && !isnull (name[ind]); ind++)
	lownam[ind] = uptolow (name[ind]);
    lownam[ind] = '\0';

    sprintf(userdir, "%s/%s", DEF_USERDIR, lownam);
    if (stat(userdir, &buf) == 0){
      p = getpwnam("anybody");	/* optimization for hashed passwd */
      strcpy(p->pw_name, lownam);
      return(p);
    }


    for (setpwent(); (p = getpwent()) != NULL; )
    {
	for (ind = 0; !isnull (name[ind]); ind++)
	    p -> pw_name[ind] = uptolow (p -> pw_name[ind]);

	if (strcmp (lownam, p->pw_name) == 0)
	    break;
    }
    endpwent();
    return(p);
}
