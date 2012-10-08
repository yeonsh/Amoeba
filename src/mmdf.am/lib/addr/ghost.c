/*	@(#)ghost.c	1.1	92/05/14 10:38:26 */
#include "util.h"
#include "mmdf.h"
#include <pwd.h>
#include "ch.h"
#include "dm.h"
#include "ap.h"


extern struct ll_struct *logptr;
extern char *ch_dflnam;
extern char *blt();
extern char *index();
extern char *rindex();
extern char *locname;
extern char *supportaddr;
extern char *sitesignature;
extern char *locmachine;               /* local machine name           */
extern int mgt_inalias;

char *adr_fulldmn;      /* Name of 'full' domain                */

char *adr_orgspec;       /* original mailbox string      */

LOCVAR char adr_gotone;

char *adr_ghost ();
Domain *dm_h2domain ();



main ()


{

	char instr[1000];
	char thename[1000];
	char * result;
	char tstline[1000];
	char newloc[1000];

	mmdf_init ("GHOST");

	logptr -> ll_level = LLOGFTR ;

	printf ("Locname '%s', Locmachine '%s'\n\n", locname, locmachine);


	while (1)

	{

		printf ("Enter the string to parse: "); 
		fflush(stdout);
		scanf ("%s", instr);

		while (1)
		{
			result = adr_ghost(instr, (short) strlen(instr), thename);

			if (result ==  (char *) NOTOK )
			{
				printf ("Error returned by 'adr_ghost' for '%s'", instr);
				fflush(stdout);
				goto done;
			}
			else
			{
				printf ("Host is '%s'     ", thename); 
				fflush(stdout);
				printf ("Pointer at '%s'\n", result); 
				fflush (stdout);

				if (lexequ(thename, locmachine)) 
				{
					printf ("Let's try again \n") ;
					fflush (stdout);
					continue ;  
				}
	/* keep going until we're out of names or we find one */

				/*  Now, we should have a host left now.  Let's see if it exists */

				if (dm_h2domain (thename, tstline) == (Domain *) NOTOK)
				{
					printf ("Lossage trying to get '%s' in the tables\n", thename); fflush(stdout);
					goto done;
				}
				else
				{
					*result = '\0';		/* terminate the new "local" part */
					printf ("Local is '%s' and Domain is '%s'\n", instr, tstline); fflush(stdout);
				}

			}
		}  /* end WHILE (1)  */
	}

done:  
	printf ("Bye\n"); fflush(stdout);
}

/**/

char *
adr_ghost (buf, len, to)  /* get host field from end of text    */
char   *buf;                      /* the buffer holding the text        */
short     len;                      /* length of the buffer               */
char   *to;                       /* put "hostname" into here           */
{
	extern char *compress ();
	register char  *strptr;
	int    hostlen;
	char   *hostptr;
	char   *endptr;

#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "adr_ghost (%s)", buf);
#endif
	for (strptr = &(buf[len - 1]); isspace (*strptr); strptr--);
	/* skip trailing white space          */
	for (hostptr = endptr = strptr;; hostptr = strptr--)
	{
		if (strptr <= buf)
			return ((char *) NOTOK);       /* reached beginning of string */
		switch (*strptr)
		{
		case '>':             /* a "foo bar <adr @ host>" spec */
			*strptr = '\0';
			for (strptr = buf; ; strptr++)
				switch (*strptr)
				{
				case '\0':
					return ((char *) NOTOK);

				case '<':
					compress (++strptr, buf);
					return (adr_ghost (buf, strlen (buf), to));
				}

		case ')':             /* trailing comment field             */
		{
			short pcount = 1;

			while (--strptr > buf && pcount)
			{
				if (*strptr == '(') pcount--;
				if (*strptr == ')') pcount++;
				if (pcount == 0)
				{
					while (strptr > buf &&
					       isspace (*--strptr));
					*++strptr = '\0';
					break;
				}
			}
		}
			continue;

		default:
			continue;

		case '@':
		case '%':             /* same as @ */
		case '.':             /* same as @ */
			break;

		case ' ':
		case '\t':
			for (strptr--; (strptr > buf) && isspace (*strptr);
			strptr--);
			    switch (*strptr)  /* got one now                        */
			{
			case '@':
			case '%':
			case '.':
				break;

			default:      /* " at "??                           */
				if (equal (&strptr[-1], "AT", 2))
					strptr--;
				else      /* tsk tsk                            */
				return ((char *) NOTOK);
			}
			break;
		}
		break;
	}
	if (to)
	{
		hostlen = endptr - hostptr;
		if (hostlen == 0)
			(void) strcpy (to, locname);
		else
		{
		    *(blt (hostptr, to, hostlen + 1)) = '\0';
		    compress (to, to);
		    if (strlen (to) == 0)       /* null host field */
		    {
			/* default to local host */
			(void) strcpy (to, locname);
		    }
		}
	}
	while (isspace (*--strptr));
	strptr++;                     /* leave pointer just AFTER graphics  */
	return (strptr);
}
/**/

