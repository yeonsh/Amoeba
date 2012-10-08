/*	@(#)s_get.c	1.2	92/05/14 10:43:38 */
/*
**		S _ G E T
**
**
**	R E V I S I O N  H I S T O R Y
**
**	03/31/83  GWH	Split the SEND program into component parts
**			This module contains getuinfo, gethead, prefix,
**			getaddr, and addrcat.
**
**	04/15/83  DPK	Changed address format from "user at host" to
**			"user@host". Also added call to getmailid() in get
**			getuinfo();
**
**	05/20/83  GWH	Modified getuinfo() to allow a number of user-
**			selectable options. These include the use of a
**			".sendrc" file on the user's home directory. With
**			the use of the "rc" file the user can set such things
**			default editor, local-alias-file, signature block,
**			and a directory for draft files. Also included is a
**			provision for keeping file copies of messages that
**			are successfully copied (incomplete at this time).
**
**	06/22/83  GWH	Added the required code to addrcat to enable local
**			alias file feature.
**
**	10/21/83  GWH	Fixed a bug in the alias file code (in addrcat).
**			Prevents segmentation faults when loops are found
**			in the alias file definitions.
**
**	11/16/83  DPK	Merged in DPK signature checking and Geo loop catcher
**
**	11/21/83  GWH	Added 'directedit' option.
**
**	03/18/85  DPK	Fixed usage of parsadr function so that after parsing
**		  VAK	we now use the components instead of original string.
**
*/


#include "stdio.h"
#include "./s.h"
#include "./s_externs.h"

extern char *getmailid();
extern char *malloc();
extern char *strdup();
extern struct passwd *getpwuid();
extern char *mktemp();
extern char *index();
extern char *getenv();
extern char *whoami();

extern char *dfleditor;
extern char *dflveditor;
extern char *dflchecker;

#define END	(char *)0

struct RCOPTION rcoptions[] = {
	"copyfile", COPYFILE,
	"draftdir", DRAFTDIR,
	"signature", SIGNATURE,
	"nosignature", NOSIGNATURE,
	"replyto", REPLYTO,
	"aliases", ALIASES,
	"subargs", SUBARGS,
	"editor", EDITOR,
	"veditor", VEDITOR,
	"file", CFILE,
	"fileonquery", QFILE,
	"nofile", NOFILE,
	"directedit", DIREDIT,
	"checker", CHECKER,
	"header", ADDHEADER,
	"paging", PAGING,
	END
};

int picko();

struct passwd *pw;
char pwname[256];
char pwgecos[256];
char pwdir[256];
char midp[256];
char *d_subargs = "vm";
char drft_tmplt[] = "drft.XXXXXX";
char linebuf[128];
char tmpline[128];

struct ALIAS *als, alias_ptr;
struct ALIAS *old_als;
FILE *afd;

getuinfo() {
	FILE *fp;
	int realid, effecid;
	char *av[NARGS];
	char rcfilename[128];
	register char *p;

	int i;

	/* build a draft file name */

	mktemp(drft_tmplt);
	/* Who are we this time */

	getwho(&realid, &effecid);

	/* If nothing found return false. */

	pw = (struct passwd *)calloc(1,sizeof(struct passwd));
	strcpy(pwname,(char *)whoami());
	strcpy(pwgecos,pwname);
	sprintf(pwdir,"/home");
	pw->pw_name = &pwname[0];
	pw->pw_gecos = &pwgecos[0];
	pw->pw_dir = &pwdir[0];
	strcpy(midp,pwname);

	/* Build a name for this user */
#ifdef PWNAME
	if (pw->pw_gecos != NULL && *pw->pw_gecos != '\0') {
		if (p = index(pw->pw_gecos, ','))
			*p = '\0';
		if (p = index(pw->pw_gecos, '<'))
			*p = '\0';
		if (p = index(pw->pw_gecos, '&')) {
			/* Deal with Berkeley folly */
			*p = 0;
			sprintf(from, "%s%c%s%s <%s@%s.%s>", pw->pw_gecos,
				 lowtoup(pw->pw_name[0]),
				 pw->pw_name+1, p+1, midp, locname, locdomain);
		} else
			sprintf(from, "%s <%s@%s.%s>", pw->pw_gecos, midp,
				 locname, locdomain, locdomain);
	} else
#endif
	sprintf(from, "%c%s@%s.%s",	/* default from text */
		lowtoup(midp[0]), &(midp[1]), locname, locdomain);
	/* Set default values for those options that need defaults */

	strcpy(editor, ((p = getenv("EDITOR")) && *p) ? p : dfleditor);
	strcpy(veditor, ((p = getenv("VISUAL")) && *p) ? p : dflveditor);
	strcpy(checker, dflchecker);
	sprintf(linebuf, "%s/.sent", pw->pw_dir);
	strcpy(copyfile, linebuf);
	sprintf(drffile, "%s/%s", pw->pw_dir, drft_tmplt);
	strcpy(subargs, d_subargs);
	wflag = 0;
	cflag = 0;
	aflag = 0;
	rflag = 0;
	qflag = 0;
	dflag = 0;
	pflag = 0;

	strcpy(signature, from);

	/* Get info from the ".sendrc" file */

	sprintf(rcfilename, "%s/%s", pw->pw_dir, SENDRC);
	if ((fp = fopen(rcfilename, "r")) != NULL) {
		char *cp;

		/* Process info a line at a time */
		while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
			if (cp = index(linebuf, '\n'))
				*cp = 0;
			if (sstr2arg(linebuf, NARGS, av, " \t") < 0)
				continue;
			if ((i=picko(av[0])) < 0)
				continue; /* bad option */
			select_o(i, av);
		}	/* end of while on lines */
	}	/* end of if statement on file open */

	free(pw);
	return(TRUE);
}	/* end of getuinfo routine */

/* select an option */

picko(pp)
char *pp;
{
	struct RCOPTION *rcopt;

	rcopt = rcoptions;
	while (rcopt->name != END) {
		if (lexequ(rcopt->name, pp))
			return(rcopt->idnum);
		rcopt++;
	}

	return(-1); /* no match */
}

get_key(s, key, list)
char *s, **key, **list;
{
	char tmps[512];
	char *ptmps;
	int len_list;
/*
**  This routine takes the input string s and divides it into two parts,
**  the first of which consists of the first word in the original string.
**  It is returned in key. The second part is a list of the remaining names.
**  They are returned in the list. Malloc is used to allocate the necessary
**  storage for the sublists.
*/

	/* throw away initial white space */

	while (*s == ' ' || *s == '\t')
		*s++;

	/* get the first word. It may be terminated by spaces, tabs, commas, */
	/* newlines or end-of-file					     */

	ptmps = tmps;
	while (*s != ' ' && *s != '\t' && *s != ',' && *s != '\n' && 
			*s != (char)EOF) {
		*ptmps++ = *s++;
	}
	*ptmps = 0;
	len_list = strlen(tmps);
	if ((*key = malloc((unsigned)(len_list) + 1)) == 0) {
		perror(" SEND - GET_KEY -- MALLOC ERROR");
		return(-1);
	}

	strcpy(*key, tmps);

	/* and now the rest of the list */

	ptmps = s;
	while (*s != '\n' && *s != (char)EOF && *s != '\0')
		*s++;
	*s = 0;
	len_list = strlen(ptmps);
	if (len_list <= 0)
		return(-1);	/* no list - ERROR - ERROR */
	if ((*list = malloc((unsigned)(len_list) +1)) == 0) {
		perror(" SEND GET_KEY - MALLOC ERROR");
		return(-1);
	}

	strcpy(*list, ptmps);
	return(0);
}

select_o(key, pp)
int key;
char **pp;
{
	register char *p;
	char *akey, *list;
	char tmpbuf[512];

	switch (key) {
	case COPYFILE:
		/* Pick a file to store file copies of sent messages */
		strcpy(copyfile, *(++pp));
		if (copyfile[0] != '/') {
			strcpy(tmpline, copyfile);
			sprintf(copyfile, "%s/%s", pw->pw_dir, tmpline);
		}
		break;

	case DRAFTDIR:
		/* pickup the draft directory */
		if (*(++pp) != 0)
			sprintf(drffile, "%s/%s", *pp, drft_tmplt);
		else
			sprintf(drffile, "%s/%s", pw->pw_dir, drft_tmplt);
		break;

	case SIGNATURE:
		/* If he wants a signature block use it */
		if (*(++pp) == 0) {
#ifdef PWNAME
			/* use his login name alone */
			sprintf(signature, "%c%s@%s.%s",
				lowtoup(midp[0]), &(midp[1]), locname, locdomain);
#else
			strcpy(signature, from);	/* NO-OP ? */
#endif
		} else {
			linebuf[0] = '\0';
			while (*pp != 0) {
				strcat(linebuf, *pp);
				strcat(linebuf, " ");
				*pp++;
			}
			if (checksignature(linebuf) == 0) {
				sprintf(signature, "%s <%s@%s.%s>",
					linebuf, midp, locname, locdomain);
			} else {
				printf("Illegal signature, using default\n");
			}
		}
		break;

	case NOSIGNATURE:
#ifdef PWNAME
		/* use his login name alone */
		sprintf(signature, "%c%s@%s.%s",
			lowtoup(midp[0]), &(midp[1]), locname, locdomain);
#else
		strcpy(signature, from);
#endif
		break;

	case REPLYTO:
		rflag = 1;
		break;

	case ALIASES:
		aflag = 1;
		strcpy(aliasfilename, *(++pp));
		if (aliasfilename[0] != '/') {
			strcpy(tmpline, aliasfilename);
			sprintf(aliasfilename, "%s/%s",pw->pw_dir,tmpline);
		}

		/*
		**  Open the alias file and store the keys and lists
		**  in core
		*/

		afd = fopen(aliasfilename, "r");
		if (afd == NULL) {
			aflag = 0;
			perror(" Can't open alias file ");
			break;
		}
		als = &alias_ptr;
		old_als = 0;
		while (1) {
			if (fgets(tmpbuf, sizeof(tmpbuf), afd) != NULL) {
				get_key(tmpbuf, &akey, &list);
				if (old_als != 0) {
					if ((als = (struct ALIAS *) malloc(sizeof(alias_ptr))) == 0) {
						perror("SEND GETUINFO - MALLOC ERROR");
						break;
					}
					old_als->next_int = als;
				}
				als->key = akey;
				als->cntr = 0;
				als->names = list;
				als->next_int = 0;
				old_als = als;
			}
			else {
				old_als = NULL;
				break;
			}
		}
		break;

	case SUBARGS:
		strcpy(subargs, d_subargs);	/* Reset to default (required) */
		while (*(++pp) != NULL)
			strcat(subargs, *pp);
		wflag = 1;
		break;

	case EDITOR:
		/* Default editor for use with the "e" command */
		strcpy(editor, *(++pp));
		break;

	case VEDITOR:
		/* Default editor for use with the "v" command */
		strcpy(veditor, *(++pp));
		break;

	case CHECKER:
		/* Default spelling checker for use with the "c" command */
		strcpy(checker, *(++pp));
		break;

	case DIREDIT:
		dflag = 1;
		if ((*(++pp) != (char *) 0) && (**pp == 'e'))
			dflag = 2;
		break;

	case PAGING : 
		/* Turn on paging during message review */
		pflag = 1;
		break;

	case CFILE:
		cflag = 1;
		break;

	case QFILE:
		qflag = 1;
		/* DROP */
	case NOFILE:
		cflag = 0;
		break;

	case ADDHEADER:
		/* Add a header line to the message */
		++pp;
		if (isstr(*pp))
			addheader(pp[0], pp[1]);
		break;

	default:
		printf("GETUINFO - NO SUCH OPTION key = %d\n", key);

	} /* end of switch */
}

/* Prompts for a header with print as prompt and gathers it up */
gethead(print, buf, flag)
char	print[];
register char	*buf;
int	flag;			/* print contents, if any?		*/
{
	register int	c, n;
	register int	max = S_BSIZE;
	int	retval;		/* type of FLD modification		*/

	if (flag && *buf)		/* print what already is there	*/
		printf(shrtfmt, print, buf);

	flag = TRUE;		/* always & only look at first char	*/
	do {
		printf("%s", print);	/* prompt			*/
		if (*buf == '\\') {
			*buf++ = ',';
			*buf++ = '\n';
		}
		fflush(stdout);

		if (flag) {
			flag = FALSE;	/* ignore first char of next lines */
			switch (c = getchar()) {
			case '-':	/* delete single name		*/
				retval = ITMRMV;
				break;
			case '#':	/* delete all contents	*/
				*buf = 0;
				while (getchar() != '\n');
				return(FLDREMV);

			case '\n':	/* leave it as it is		*/
				return(FLDSAME);

			case '+':	/* append to end of field	*/
				retval = FLDADD;
				if (*buf != '\0') { /* if not already empty */
					/* point to end of buf */
					buf = &buf[strlen(buf)];
					/* assume it's an address field */
					*buf++ = ',';
					*buf++ = ' ';
					max -= 2;
				}
				break;

			default:	/* copy over it			*/
				retval = FLDOVER;
				*buf++ = c;
				max--;
				break;
			}
		}
		n = gather(buf, max);	/* get rest of header line	*/
		max -= n;
		buf = &buf[n - 1];
	} while (*buf == '\\');	/* user wants to add more?	*/

	return(retval);
}

prefix(buf, pre)
char *buf, *pre;
{
	while (*pre)
		if (*buf++ != *pre++)
			return(FALSE);
	return(TRUE);
}


getaddr(print, buf, flag, defhost)
char print[];
char *buf;
int flag;
char defhost[];
{
	char	addrin[S_BSIZE];	/* temporary store for user input */
	char	*dn, *gstrindex();

	if (flag && *buf)		/* print what already is there	*/
		printf(shrtfmt, print, buf);

	addrin[0] = '\0';		/* mark temp buffer as empty	*/
	switch (gethead(print, addrin, NO)) {
	case FLDREMV:		/* erase any existing contents	*/
		buf[0] = '\0';
		break;

	case ITMRMV:		/* delete single name		*/
		dn = gstrindex(addrin, buf);
		if (dn == 0) {
			printf(" no such name in the list; name = '%s'\n",
				addrin);
			break;
		}
		dlt_name(buf, dn);
		break;
	case FLDSAME:		/* leave as is			*/
		break;

	case FLDOVER:		/* overwrite existing contents	*/
		buf[0] = '\0';	/* erase existing, then add	*/
		addrcat(buf, addrin, defhost);
		break;

	case FLDADD:		/* append to existing		*/
		addrcat(buf, addrin, defhost);
		break;
	}
}

/* these varibles are made global for recursive reasons */

int linelen,
destlen,
addrlen;

addrcat(dest, src, thehost)
char *dest, *src, thehost[];
{
	char	addr[S_BSIZE], name[ADDRSIZE], address[ADDRSIZE],
		temphost[FILNSIZE], defhost[FILNSIZE], hostname[FILNSIZE];
	char	*sadrptr, *ptr;
	int	sub_flag;

	if (thehost != 0)		/* save official version of default */
		strcpy(defhost, thehost);

	for (ptr = dest, linelen = 0; *ptr != '\0'; ptr++, linelen++)
		if (*ptr == '\n')	/* get length of last line of header */
			linelen = -1;	/* make it zero for next character */
	destlen = ptr - dest;

	for (adrptr = src;;) {		/* adrptr is state variable, used in */
					/*   next_address		*/
		switch (next_address(addr)) {
		case -1:		/* all done			*/

			/* clean markers in aliases structure */
			als = &alias_ptr;
			while (als != NULL) {
				als->cntr = 0;
				als = als->next_int;
			}
			return;

		case 0:		/* empty address			*/
			continue;
		}

		name[0] = address[0] = temphost[0] = '\0';
		parsadr(addr, name, address, temphost);
		if (address[0] == '\0') {	/* change the default host */
			if (temphost[0] != '\0')
				strcpy(defhost, temphost);
			continue;
		}
		if (aflag && temphost[0]==0) {
			als = &alias_ptr;
			sub_flag = 0;
			while (als != NULL) {
				if (lexequ(address, als->key) && als->cntr == 0) {
					als->cntr++;
					sadrptr = adrptr;
					addrcat(dest, als->names, thehost);
					adrptr = sadrptr;
					sub_flag = 1;
					break;
				}
				else
					if (lexequ(address, als->key) && als->cntr != 0) {
						sub_flag = 1;
						break;
					}
				als = als->next_int;
			}
			if (sub_flag == 1)
				continue;
		}
		if (temphost[0] == '\0')
			strcpy(hostname, defhost);
		else
			strcpy(hostname, temphost);

		if (name[0] != '\0') {	/* put into canonical form	*/
					/* name <mailbox@host>		*/

			/* force hostname to lower		*/
			for (ptr = hostname; !isnull(*ptr); ptr++)
				*ptr = uptolow(*ptr);

			/* is quoting needed for specials?	*/
			for (ptr = name; ; ptr++) {
				switch (*ptr) {
				default:
					continue;

				case '\0':	/* nope			*/
					sprintf(addr, "%s <%s@%s>",
						name, address, hostname);
					goto doaddr;

				case '<':	/* quoting needed	*/
				case '>':
				case '@':
				case ',':
				case ';':
				case ':':
					sprintf(addr, "\"%s\" <%s@%s>",
						name, address, hostname);
					goto doaddr;
				}
			}
		}
		else {
			sprintf(addr, "%s@%s", address, hostname);
		}

doaddr:
		addrlen = strlen(addr);

		/* it will fit into dest buffer	*/
		if ((destlen + addrlen) < (S_BSIZE - 15)) {
			if (destlen == 0)
				strcpy(dest, addr);
			else {	/* monitor line length		*/
				if ((linelen + addrlen) < 67)
					sprintf(&dest[destlen], ", %s", addr);
				else {
					sprintf(&dest[destlen], ",\n%s", addr);
					linelen = 0;
				}
				destlen += 2;
				linelen += 2;
			}
			destlen += addrlen;
			linelen += addrlen;
		}

	}
}
/*
**	S P E C I A L  S T R I N D E X	F U N C T I O N
**
** Specially modified by George Hartwig to return a pointer to the beginning
** of the requested string instead of the offset.
*/

char *gstrindex(str, target)		/* return column str starts in target */
register char	*str,
*target;
{
	char *otarget;
	register short slen;

	for (otarget = target, slen = strlen(str); ; target++) {
		if (*target == '\0')
			return((char *) 0);

		if (equal(str, target, slen))
			return(target);
	}
}

/* buf is a pointer to a character array and pos is a pointer to a position  */
/* within that array. The purpose of this routine is delete the name field   */
/* pointed to by pos. In order to do this we will search backward to either  */
/* comma ',' or a colon ':' (if a comma, we will then delete that comma also */
/* and then delete forward until another comma or			     */
/* a end-of-string '\0' is found.					     */
dlt_name(buf, pos)
char *buf, *pos;
{
	char tmpbuffer[S_BSIZE];
	char *sd, *ed, *cp;
	int quote_flag;


	cp = pos;
	quote_flag = 0;

	while (cp >= buf) {
		if (*cp == '"')
			if (quote_flag == 0)
				quote_flag = 1;
			else
				quote_flag = 0;
		if (*cp == ',' && quote_flag == 0)
			break;
		else
			cp--;
	}
	sd = cp;

	cp = pos;

	while (*cp != '\0') {
		if (*cp == '"')
			if (quote_flag == 0)
				quote_flag = 1;
			else
				quote_flag = 0;
		if (*cp == ',' && quote_flag == 0)
			break;
		else
			cp++;
	}

	ed = cp;

	/* at this point *sd should point at the comma preceding the name
	** to be deleted or the beginning of the line and ed should be
	** pointing to the trailing comma or the end of the line. So it is
	** necessary at this point to determine just which of these cases
	** have occurred so that the string may be properly reassembled.
	*/

	if (*sd == ',' && *ed == ',') {
		sd++;
		*sd = '\0';
		ed++;
		sprintf(tmpbuffer, "%s%s", buf, ed);
	}

	else
		if (sd <= buf) {
			ed++;
			strcpy(tmpbuffer, ed);
		}
		else
			if (*ed == '\0') {
				*sd = '\0';
				strcpy(tmpbuffer, buf);
			}
	strcpy(buf, tmpbuffer);
}

/*
 *	checksignature()
 *
 *  Verifies that the signature string does not contain any special
 *  characters or is quoted.  Also checks for blank "name" portion.
 */
checksignature(sp)
register char *sp;
{
	char tptr[128], *savptr, *ptr;
	static char quote[] = "\"";

	savptr = sp;
	while (*sp && isspace(*sp))	/* eat leading whitespace */
		sp++;
	if (*sp == '\0') {
		printf("Signature is blank\n");
		return(-1);		/* must have some signature */
	}
	while (*sp) {
		switch (*sp) {
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '.':	/*  Arggggg...	*/
		case '[':
		case ']':
			/* surround bad signature with quotes */
			tptr[0] = '\0';
			strcat(tptr,quote);
			sp = savptr;
			strcat(tptr,sp);
			if (ptr = rindex(tptr, ' '))
				*ptr = '\0';
			strcat(tptr,quote);
			strcpy(sp,tptr);
			return(0);

		case '\\':
			sp++;
			if (*sp == '\0')
				return(-1);
			break;

		case '"':
			for (sp++; *sp != '"'; sp++) {
				if (*sp == '\\') {
					sp++;
					if (*sp == '\0')
						return(-1);
				} else if (*sp == '\0') {
					printf("Unmatched '\"' in signature\n");
					return(-1);
				}
			}
			break;

		default:
			break;
		}
		sp++;
	}

	return(0);
}

addheader(name, data)
char	*name;
char	*data;
{
	register struct header *nhp;
	register struct header *ohp;
	register char *cp;
	char	buf[LINESIZE];

	if (!isstr(name))
		return;
	for (cp = name; *cp; cp++)
		if (!isprint(*cp) && *cp != '\t') {
			printf("Illegal character in header name.\n");
			return;
		}
	data = (isstr(data) ? data : "");
	for (cp = data; *cp; cp++)
		if (!isprint(*cp) && *cp != '\t') {
			printf("Illegal character in header data.\n");
			return;
		}
	if (headers) {
		for (ohp = headers; !lexequ(name, ohp->hname); ohp = ohp->hnext)
			if (ohp->hnext == (struct header *)0)
				break;
		if (lexequ(name, ohp->hname)) {
			strncpy(ohp->hdata, data, sizeof(ohp->hdata)-1);
			return;
		}
	}
	nhp = (struct header *)malloc(sizeof(*nhp));
	sprintf(buf, "%s:  ", name);
	if (nhp == NULL || (nhp->hname = strdup(buf)) == NULL) {
		printf("No memory for header '%s'\n", buf);
		if (nhp)
			free(nhp);
		return;
	}
	strncpy(nhp->hdata, data, sizeof(nhp->hdata)-1);
	nhp->hnext = (struct header *)0;
	nhp->hdata[sizeof(nhp->hdata)-1] = '\0';
	if (headers == (struct header *)0) {
		headers = nhp;
		return;
	}
	ohp->hnext = nhp;
}
