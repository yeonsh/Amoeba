/*	@(#)std_params.c	1.3	94/04/06 12:00:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Get and/or set server parameters
 */

#include <amtools.h>
#include <ampolicy.h>
#include <class/stdparams.h>
#include <stdlib.h>
#include <string.h>
#include <module/buffers.h>

char *progname = "std_params";

int verbose = 0;
int setting = 0;

struct param {
    char *par_name;
    char *par_type;
    char *par_descr;
    char *par_value;
};


void showparams();
void add_setting();
int  do_settings();

static void
usage()
{
    fprintf(stderr, "usage: %s [-v] [-s param=value] svrname\n", progname);
    fprintf(stderr, "-s: set parameter <param> to <value>\n");
    fprintf(stderr, "-v: also show parameter description and type\n");
    exit(2);
}

main(argc, argv)
int argc;
char *argv[];
{
    char       *svrname;
    capability  svrcap;
    int		exitval;
    errstat	err;
    int		opt;
	
    progname = argv[0];
	
    /* Scan command line options */
    while ((opt = getopt(argc, argv, "s:v")) != EOF) {
	switch (opt) {
	case 's':
	    setting++;
	    add_setting(optarg);
	    break;
	case 'v':
	    verbose++;
	    break;
	default:
	    usage();
	    break;
	}
    }

    if (argc - optind != 1) {
	usage();
    } else {
	svrname = argv[optind];
    }
	
    if ((err = name_lookup(svrname, &svrcap)) != STD_OK) {
	fprintf(stderr, "%s: cannot find run server '%s' (%s)\n",
		progname, svrname, err_why(err));
	exit(1);
    }
	
    if (setting) {
	exitval = do_settings(&svrcap);
    } else {
	exitval = 0;
    }

    if (!setting || verbose) {
	showparams(svrname, &svrcap);
    }

    exit(exitval);
}



/*
 * Get server parameters (name, type, description, value).
 */

static char *
fetch_string(buf, end, str)
char *buf, *end;
char **str;
{
    char *bufp, *s;

    bufp = buf_get_string(buf, end, &s);
    if (bufp != NULL) {
	*str = (char *)malloc((size_t)(strlen(s) + 1));
	if (*str == NULL) {
	    fprintf(stderr, "could not allocate string\n");
	    exit(1);
	}
	(void)strcpy(*str, s);
    }
    return bufp;
}

static errstat
do_get_params(cap, ret_params, ret_nparams)
capability *cap;
struct param **ret_params;
int *ret_nparams;
{
    errstat err;
    int i, paramlen, numparams;
    struct param *params;
    char *parambuf;
    char *bufp, *end;

    parambuf = (char *)malloc((size_t)MAX_PARAM_LEN);
    if (parambuf == NULL) {
	return STD_NOSPACE;
    }

    err = std_getparams(cap, parambuf, MAX_PARAM_LEN, &paramlen, &numparams);
    if (err != STD_OK) {
	return err;
    }

    params = (struct param *)
	malloc((size_t)(numparams * sizeof(struct param)));
    if (params == NULL) {
	free(parambuf);
	return STD_NOSPACE;
    }

    bufp = parambuf;
    end = &parambuf[paramlen];

    for (i = 0; i < numparams && bufp != NULL; i++) {
	char *s;

	bufp = fetch_string(bufp, end, &params[i].par_name);
	bufp = fetch_string(bufp, end, &params[i].par_type);
	bufp = fetch_string(bufp, end, &params[i].par_descr);
	bufp = fetch_string(bufp, end, &params[i].par_value);
    }

    free(parambuf);

    if (i < numparams) {
	free(params);
	/* also freeing the strings here isn't worth the trouble */
	return STD_NOSPACE;
    }

    *ret_params = params;
    *ret_nparams = numparams;
    return STD_OK;
}


/*
 * Show current server parameters.
 */

static void
show(par, npar, showdescr, showtype)
struct param *par;
int npar;
int showdescr;
int showtype;
/*
 * Show the parameter list `par' with length `npar' on standard output.
 * If showdescr is nonzero, also show the description of each parameter.
 * If showtype is nonzero, also show the type of each parameter.
 */
{
    int i;

    for (i = 0; i < npar; i++) {
	struct param *p = &par[i];

	printf("%-12s ", p->par_name);

	if (showtype) {
	    /* use a filler to make output pretty */
	    static char filler[] = "             ";

	    printf("[%s]", p->par_type);
	    if (strlen(p->par_type) + 2 < sizeof filler) {
		printf("%s", &filler[strlen(p->par_type) + 2]);
	    }
	}

	printf(" %6s", p->par_value);

	if (showdescr) {
	    printf(" (%s)", p->par_descr);
	}
	printf("\n");
    }
}

void
showparams(svrname, svrcap)
char *svrname;
capability *svrcap;
{
    errstat err;
    struct param *params;
    int nparams, par_index;
    interval old;
	
    old = timeout(2000L);
    if ((err = do_get_params(svrcap, &params, &nparams)) != STD_OK) {
	fprintf(stderr, "%s: cannot get parameters from `%s' (%s)\n",
		progname, svrname, err_why(err));
	exit(1);
    }
    (void) timeout(old);

    show(params, nparams, verbose, verbose);
}

/*
 * Parameter setting
 */

#define NPARAMS		10

static int nparams = 0;
static char *params[NPARAMS];
static char *values[NPARAMS];

void
add_setting(string)
char *string;
/*
 * Add a parameter setting to the list.
 * The string should be of the form "param=value"
 */
{
    char *equals;

    if (nparams >= NPARAMS) {
	fprintf(stderr, "too many parameter settings (max %d)\n", NPARAMS);
	exit(1);
    }

    /* find '=' in string, and check */
    equals = strchr(string, '=');
    if (equals == NULL) {
	fprintf(stderr, "missing '=' in parameter setting `%s'\n", string);
	exit(1);
    }

    /* The equal sign could be at the beginning or end of the setting,
     * but whether this is correct or not, we leave to the server to decide.
     */
    *equals = '\0';
    params[nparams] = string;
    values[nparams] = equals + 1;
    nparams++;
}

static errstat
do_std_setparam(cap, param, value)
capability *cap;
char *param;
char *value;
{
    errstat err;
    size_t length;
    char *buf;
    char *bufp, *end;

    length = strlen(param) + strlen(value) + 2;
    buf = (char *)malloc(length);
    if (buf == NULL) {
	return STD_NOSPACE;
    }

    bufp = buf;
    end = &buf[length];

    bufp = buf_put_string(bufp, end, param);
    bufp = buf_put_string(bufp, end, value);
    
    if (bufp == NULL) {
	err = STD_NOSPACE;
    } else {
	err = std_setparams(cap, buf, (int)length, 1);
    }
    free(buf);

    return err;
}

int
do_settings(svrcap)
capability *svrcap;
{
    int failed = 0;
    int i;

    /* Set the parameters accumulated in params[].
     * We do them one by one, so that we can see which ones are unacceptable.
     */
    for (i = 0; i < nparams; i++) {
	errstat err = do_std_setparam(svrcap, params[i], values[i]);

	if (err != STD_OK) {
	    fprintf(stderr, "%s: could not set parameter %s (%s)\n",
		    progname, params[i], err_why(err));
	    failed++;
	}
    }
    return failed;
}
