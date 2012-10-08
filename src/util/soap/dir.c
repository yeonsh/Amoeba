/*	@(#)dir.c	1.7	96/02/27 13:14:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "sp_dir.h"
#include "module/tod.h"
#include "module/stdcmd.h"
#include "module/mutex.h"
#include "module/prv.h"
#include "module/path.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define LONG_TIME	(60*60*24*365/2)	/* half a year */


char *prog;
long seconds;
sp_result *out;
SP_DIR *dd;
int cflag, dflag, eflag, kflag, lflag, rflag, tflag, uflag, xflag;
int multiple, *order;

print_result(sr)
sp_result *sr;
{
	char *ctime();
	char *text;
	long time;
	int i, n, len;
	errstat err;
	interval old;
	char buf[32];
	capset *cs = &sr->sr_capset;

	if (xflag) {
		for (i = 0, n = 0; i < cs->cs_final; i++)
			if (cs->cs_suite[i].s_current)
				n++;
		printf(" %d,%d,%d", cs->cs_initial, n, cs->cs_final);
	}
	if (kflag) {
		for (i = 0; i < cs->cs_final; i++) {
			dumpcap(&cs->cs_suite[i].s_object);
		}
	}
	if (lflag) {
		if (sr->sr_status != STD_OK)
			printf("  (%s)", err_why((errstat) sr->sr_status));
		else {
			if ((time = sr->sr_time) == 0 ||
						(text = ctime(&time)) == 0)
				printf("%*s", 14, "");
			else
				printf("  %7.7s%5.5s", text + 4,
					text + (time > seconds ? 11 : 19));
			old = timeout((interval) 2000);
			/* If the capset is empty we better initialise err */
			err = STD_NOTFOUND;
			for (i = 0; i < cs->cs_final; i++) {
				err = std_info(&cs->cs_suite[i].s_object,
					       buf, (int) sizeof(buf), &len);
				if (err == STD_OVERFLOW) { /* truncate */
					len = sizeof(buf) - 1;
					buf[len] = '\0';
					err = STD_OK;
				}
				if (err == STD_OK) {
					break;
				}
			}
			(void) timeout(old);
			if (err == STD_OK)
				printf("  %.*s", len, buf);
			else
				printf("  (%s)", err_why(err));
		}
	}
	printf("\n");
}

dumpcap(cap)
	capability *cap;
{
	long num;
	long rights;
	
	printf(" ");
	dumport(&cap->cap_port);
	num = prv_number(&cap->cap_priv) & 0xffffff;
	rights = cap->cap_priv.prv_rights & PRV_ALL_RIGHTS;
	printf(" %6lx %2lx ", num, rights);
	dumport(&cap->cap_priv.prv_random);
}

dumport(p)
	port *p;
{
	if (NULLPORT(p))
		printf("<NULL>");
	else {
		char *cp = (char *)p;
		int n = PORTSIZE;
		while (--n >= 0) {
			char c = *cp++;
			if (c < ' ' || c > '~')
				c = '?';
			putchar(c);
		}
	}
}

#ifdef __STDC__
compar(const void *i, const void *j)
#else
compar(ip, jp)
int *ip, *jp;
#endif
{
#ifdef __STDC__
	register const int *ip = i, *jp = j;
#endif
	register sign = rflag ? -1 : 1;

	if (uflag)
		return (*ip - *jp) * sign;
	if (tflag)
		return (out[*jp].sr_time - out[*ip].sr_time) * sign;
	return sign * strcmp(dd->dd_rows[*ip].d_name, dd->dd_rows[*jp].d_name);
}

char *splitup(name)
char **name;
{
	char *last = 0, *p;

	/* Handle the case of name's ending in "." or "..": */
	static char name_buff[256];
	char *q = *name + strlen(*name);  /* Point at end of name */
	extern char *getenv();

	while (q > *name && *--q == '/') ;  /* Ignore trailing /'s */
	if (*q == '.' && (q == *name || *--q == '/' ||
			  *q == '.' && (q == *name || *--q == '/'))) {
		/* Path ends in "." or ".."; normalize it: */
		if (path_norm(getenv("_WORK"),
				 *name, name_buff, sizeof name_buff) >= 0)
			*name = name_buff;
	}

	for (p = *name; *p != 0; p++)
		if (*p == '/')
			last = p;
	if (last != 0) {
		*last++ = 0;
		if (**name == '\0') /* then we had '/string' */
		    *name = "/";
	} else {
		last = *name;
		*name = "";
	}
	return last;
}

clean_up(in, nrows, nodir)
sp_entry *in;
{
	register i;

	if (in != 0)
		free((char *) in);
	if (out != 0) {
		for (i = 0; i < nrows; i++)
			cs_free(&out[i].sr_capset);
		free((char *) out);
	}
	if (order != 0)
		free((char *) order);
	sp_closedir(dd);
	if (!nodir && multiple)
		printf("\n");
}

int
list(name)
char *name;
{
	short i, j, nrows, nodir;
	errstat n;
	rights_bits rights;
	capset cs;
	sp_entry *in;
	struct sp_direct *d;
	char *last;
	struct sp_direct *rows;
	char orig_buff[256], *orig_name = name;

	if ((nodir = dflag) != 0) {
		orig_name = orig_buff;
		if (strlen(name) < sizeof orig_buff)
			(void) strcpy(orig_name, name);
		else {
			(void) strncpy(orig_name, name, (sizeof orig_buff)-1);
			orig_name[(sizeof orig_buff)-1] = '\0';
		}

		last = splitup(&name);
	}
	if ((n = sp_lookup(SP_DEFAULT, name, &cs)) != STD_OK) {
		fprintf(stderr, "%s: can't find '%s' (%s)\n",
			prog, name, err_why(n));
		return 1;
	}
	dd = 0;
	if ((n = sp_list(&cs, &dd)) != STD_OK) {
		if (!dflag) {
			orig_name = orig_buff;
			if (strlen(name) < sizeof orig_buff)
				(void) strcpy(orig_name, name);
			else {
				(void) strncpy(orig_name, name,
							(sizeof orig_buff)-1);
				orig_name[(sizeof orig_buff)-1] = '\0';
			}

			last = splitup(&name);
			dd = sp_opendir(name);
			nodir = 1;
		}
		if (dd == 0) {
			fprintf(stderr, "%s: can't open directory '%s'\n",
							prog, name);
			return 1;
		}
	}
	if (!nodir && multiple)
		printf("'%s':\n", name);
	if (cflag) {
		rights = dd->dd_capset.cs_suite->s_object.cap_priv.prv_rights;
		if ((rights & (SP_DELRGT | SP_MODRGT)) ==
						(SP_DELRGT | SP_MODRGT))
			printf("%-18.18s", "{delete|modify}");
		else if (rights & SP_DELRGT)
			printf("%-18.18s", "{delete}");
		else if (rights & SP_MODRGT)
			printf("%-18.18s", "{modify}");
		else
			printf("%-18.18s", "");
		for (i = 0; i < dd->dd_ncols; i++)
			printf("%8.8s%c ", dd->dd_colnames[i],
						rights & (1 << i) ? '*' : ' ');
		printf("\n");
	}
	rows = dd->dd_rows;
	nrows = dd->dd_nrows;
	if (nodir) {
		while ((d = sp_readdir(dd)) != 0)
			if (strcmp(d->d_name, last) == 0) {
				rows = d;
				nrows = 1;
				break;
			}
		if (d == 0) {
			fprintf(stderr, "%s: '%s' not found\n", prog, last);
			sp_closedir(dd);
			return 1;
		}
	}
	out = 0;
	order = 0;
	if (nrows == 0) {
		clean_up((sp_entry *) 0, 0, nodir);
		return 0;
	}
	if ((in = (sp_entry *) calloc(sizeof(sp_entry), (unsigned) nrows)) == 0
	    || (out = (sp_result *) calloc(sizeof(sp_result), (unsigned) nrows)) == 0
	    || (order = (int *) calloc(sizeof(int), (unsigned) nrows)) == 0) {
		fprintf(stderr, "%s: not enough memory\n", prog);
		clean_up(in, nrows, nodir);
		return 1;
	}
	for (i = 0; i < nrows; i++) {
		in[i].se_capset = dd->dd_capset;
		in[i].se_name = rows[i].d_name;
	}
	if ((n = sp_setlookup(nrows, in, out)) != STD_OK) {
		fprintf(stderr, "can't setlookup (%s)\n", err_why(n));
		clean_up(in, nrows, nodir);
		return 1;
	}
	for (i = 0; i < nrows; i++)
		order[i] = i;
	qsort((char *)order, nrows, sizeof(int), compar);
	for (i = 0; i < nrows; i++) {
		d = &rows[order[i]];
		if (cflag || kflag || lflag || xflag)
			printf("%-20s ", nodir ? orig_name : d->d_name);
		else
			printf("%s", nodir ? orig_name : d->d_name);
		if (cflag)
			for (j = 0; j < dd->dd_ncols; j++)
				printf((j == 0) ? "%5lx": "%10lx",
					d->d_columns[j]);
		print_result(&out[order[i]]);
	}
	clean_up(in, nrows, nodir);
	return 0;
}

/*ARGSUSED*/
main(argc, argv)
char **argv;
{
	int millisecs, tz, dst;
	int exitstat = 0;

	/* Timeout longer than 10 seconds is infinite, as far as the impatient
	 * user is concerned.
	 */
	(void)timeout((interval)5000);
	prog = *argv;
	while (*++argv != 0 && !eflag && **argv == '-')
		while (*++*argv != 0)
			switch (**argv) {
			case 'c': cflag = 1; break;
			case 'd': dflag = 1; break;
			case 'e': /* compatibility: fall through */
			case '-': eflag = 1; break;
			case 'k': kflag = 1; break;
			case 'l': lflag = 1; break;
			case 'r': rflag = 1; break;
			case 't': tflag = 1; break;
			case 'u': uflag = 1; break;
			case 'x': xflag = 1; break;
			default:
				fprintf(stderr,
					"usage: %s [-cdklrtux] name ...\n",
					prog);
				fprintf(stderr, "-c: column rights\n");
				fprintf(stderr, "-d: directory status\n");
				fprintf(stderr, "-k: capability dump\n");
				fprintf(stderr, "-l: long listing\n");
				fprintf(stderr, "-r: reverse sort\n");
				fprintf(stderr, "-t: time sort\n");
				fprintf(stderr, "-u: unsorted\n");
				fprintf(stderr, "-x: replication info\n");
				exit(2);
			}
	if (lflag && tod_gettime(&seconds, &millisecs, &tz, &dst) == STD_OK)
		seconds -= LONG_TIME;
	else
		seconds = 0;
	if (*argv == 0) {
		if (list("")) exitstat = 1;
	} else {
		multiple = argv[1] != 0;
		while (*argv != 0)
			if (list(*argv++)) exitstat = 1;
	}
	exit(exitstat);
}
