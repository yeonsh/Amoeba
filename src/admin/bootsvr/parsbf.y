/*	@(#)parsbf.y	1.8	96/02/27 10:11:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Parser for the bootfile
 */

%{
/*
 * We cannot include stdlib.h, because SunOs 4.1 yacc generates
 * declarations of malloc() and realloc() that conflict with the
 * ones in our ANSI-compatible stdlib.h!
 * Fortunately, it seems we don't need it for anything special here.
 */
#define __STDLIB_H__

#include "ailamoeba.h"
#include "monitor.h"
#include "string.h"

#include "stdio.h"

#include "svr.h"
#define yyerror(s)     MyError(yystate, yychar, s)

extern char *mymalloc();

boot_data curr_conf;	/* Configuration we're working on */

#ifndef MAX_BOOTCONF
#define MAX_BOOTCONF MAX_OBJ
#endif

#ifndef DEFAULT_STACKSIZE
/* By setting the default stacksize to 0, the process will be started
 * with a default stack size (16 K, currently) unless the binary itself
 * specifies a different one.
 */
#define DEFAULT_STACKSIZE 	0
#endif /* DEFAULT_STACKSIZE */

char parsstate[200] = "initialising\n";

boot_data confs[MAX_BOOTCONF];	/* Config's we got */
int Nconf, n_errors;	/* How many */
static int got_argv, got_capv, got_environ;
extern int LineNo;

extern char *strdup();

static char *args[MAX_ARG];
static int n_args;
static boot_caps capv[MAX_CAPENV];
static int ncaps;

#ifndef MAX_VARS
#define MAX_VARS 50
#endif

static struct {	/* Table for cap FOO=<cap> definitions */
    char *name;
    boot_ref ref;
} NamedCaps[MAX_VARS];
static int NnamedCaps = 0;

%}

%token CAPABILITY IDENT NUMBER STRING DOWN
%token MACHINE BOOTRATE POLLRATE CAP
%token ENVIRON CAPV ARGV STACK
%token PROGRAM POLLCAP PROCSVR
%token AFTER

%union {
    capability t_cap;
    boot_ref t_ref;
    boot_cmd t_cmd;
    long t_num;
    char *t_str;
    char **t_ptrp;
}

%type <t_cap>	CAPABILITY
%type <t_str>	IDENT STRING optstring
%type <t_num>	NUMBER
%type <t_ref>	reference startcap
%type <t_cmd>	cmd envopts
%type <t_ptrp>	strenv argv strings

%%


	/************************************************\
	 *						*
	 *	Gross structure of the bootfile:	*
	 *						*
	\************************************************/

bootfile
    : /* Empty */	
	{
	    LineNo = 1;
	    /* Free old table */
	    for ( ; NnamedCaps > 0; free(NamedCaps[--NnamedCaps].name))
		;
	}
    | bootfile statement ';'
    ;

statement
    : CAP IDENT '=' reference
	{
	    if (NnamedCaps >= MAX_VARS) {
		MyError(-1, -1, "too many capability names");
		free($2);
		return 1;
	    } else if (FindCapName($2) >= 0) {
		MyError(-1, -1, "variable assigned to more than once");
		free($2);
		return 1;
	    } else {
		NamedCaps[NnamedCaps].name = $2;
		NamedCaps[NnamedCaps++].ref = $4;
		/* Recognize one special name */
		if (strcmp($2, "CORE_DIR") == 0)
		    SetCoreDir($4);
	    }
	}
    | service
    | strenv
	{
	    if (debugging || verbose)
		prf("%nusing new environment\n");
	    SetEnv($1);
	}
    ;

service
    : IDENT
	{ int i;
	    /* First one by this name? */
	    for (i = 0; i < Nconf; ++i) {
		if (strcmp($1, confs[i].name) == 0) {
		    MyError(-1, -1, "config defined twice");
		    return 1;	/* Exit the parser */
	        }
	    }
	    memset((char *) &curr_conf, 0, sizeof(curr_conf));
	    /* Defaults */
	    curr_conf.bootrate = DEFAULT_BOOTRATE;
	    curr_conf.pollrate = DEFAULT_POLLRATE;
	    (void) strcpy(curr_conf.name, $1);
	    free($1);
	}
      '{' options '}'
	{
	    /* All essential info available? */
	    if (NULLPORT(&curr_conf.bootcmd.executable.cap.cap_port)) {
		MyError(-1, -1, "missing program");
		MON_EVENT("on reinit: Missing program");
		return 1;
	    }
	    if (NULLPORT(&curr_conf.machine.cap.cap_port)) {
		MyError(-1, -1, "missing machine or procsvr");
		MON_EVENT("on reinit: Missing machine");
		return 1;
	    }
	    if (Nconf < MAX_BOOTCONF - 1) confs[Nconf++] = curr_conf;
	    else MyError(-1, -1, "Too many services");
	}
    ;

options
    : option
    | options ';' option
    ;

option
    : /* Empty */
    | PROCSVR reference
	{
	    if (NULLPORT(&curr_conf.machine.cap.cap_port)) {
		curr_conf.flags |= BOOT_ONPROCSVR;
		curr_conf.machine = $2;
	    } else {
		MON_EVENT("on reinit: procsvr specified twice");
		MyError(-1, -1, "procsvr specified twice");
		return 1;
	    }
	}
    | MACHINE reference
	{
	    if (NULLPORT(&curr_conf.machine.cap.cap_port)) {
		curr_conf.machine = $2;
	    } else {
		MON_EVENT("on reinit: machine specified twice");
		MyError(-1, -1, "machine specified twice");
		return 1;
	    }
	}
    | DOWN
	{ curr_conf.flags |= BOOT_INACTIVE; }
    | POLLCAP reference
	{ curr_conf.pollcap = $2; curr_conf.flags |= BOOT_POLLREF; }
    | BOOTRATE NUMBER
	{ curr_conf.bootrate = $2; }
    | POLLRATE NUMBER
	{ curr_conf.pollrate = $2; }
    | PROGRAM cmd
	{ curr_conf.bootcmd = $2; }
    | AFTER IDENT
	{ int i;

	  if (curr_conf.depname[0]) {
		MON_EVENT("on reinit: after specified twice");
		MyError(-1, -1, "after specified twice");
	  } else {
		for (i = 0; i < Nconf; ++i)
		    if (strcmp($2, confs[i].name) == 0)
			break;
		if (i < Nconf) {
		    strcpy(curr_conf.depname, $2);
		} else {
		    MyError(-1, -1, "parameter for after unknown");
		}
	  }
	  free($2);
	}

    ;

reference
    : startcap optstring
	{
	    $$.cap = $1.cap;
	    if (!pathcat($1.path, $2, $$.path)) return 1;
	    if ($2)
		free($2);
	}
    ;

startcap
    : CAPABILITY		{ $$.cap = $1; $$.path[0] = '\0'; }
    | IDENT
	{
	    long i;
	    i = FindCapName($1);
	    free($1);
	    if (i < 0) {
		MyError(-1, -1, "undefined capname used");
		return 1;
	    }
	    $$ = NamedCaps[i].ref;
	}
    | '`' IDENT '`'
	{
	    capability *capp;
	    capp = getcap($2);
	    if (capp != NULL) $$.cap = *capp;
	    else {
		prf("%ncapability '%s' not in my environment", $2);
		memset((char *) & $$.cap, 0, sizeof(capability));
	    }
	    if (debugging)
		prf("%nenv-cap %s: %s\n", $2, ar_cap(capp));
	    $$.path[0] = '\0';
	    free($2);
	}
    ;

optstring
    : /* Empty */		{ $$ = NULL; }
    | STRING			{ $$ = $1; }
    ;

cap
    :	IDENT
	{
	    if (ncaps >= MAX_CAPENV) {
		MyError(-1, -1, "too many caps in env");
		return 1;
	    }
	    if ((capv[ncaps].cap_name = $1) == NULL) return 1;
	}
	cap_ref
    ;
cap_ref
    : /* Empty */		{ capv[ncaps++].from_svr = 1; }
    | '=' reference
	{
	    capv[ncaps].from_svr = 0;
	    capv[ncaps].cap_ref = $2;
	    ++ncaps;
	}
    ;

strlist
    : STRING	{ args[0] = $1; n_args = 1; }
    | strlist ',' STRING
	{
	    if (n_args >= MAX_ARG) {
		MyError(-1, -1, "Too many strings in list");
		return 1;	/** Exit the parser */
	    }
	    if ((args[n_args++] = $3) == NULL) {
		while (n_args--) if (args[n_args] != NULL) free(args[n_args]);
		return 1;
	    }
	}
    ;
strings
    : '{' strlist '}'
	{
	    char **pp;
	    int i;
	    pp = (char **) mymalloc((int) (sizeof(char *) * (n_args + 1)));
	    if (pp == NULL) {
		for (i = n_args; i--; ) free(args[i]);
		return 1;	/* Exit the parser */
	    }
	    for (i = 0; i < n_args; ++i) pp[i] = args[i];
	    pp[n_args] = NULL;
	    $$ = pp;
	}
    ;

caplist
    : cap		
    | caplist ',' cap
    ;

strenv
    : ENVIRON
	{ if (got_argv) { MyError(-1, -1, "extra environ"); return 1; } }
      strings	
	{ $$ = $3; }
    ;
argv
    : ARGV
	{ if (got_argv) { MyError(-1, -1, "extra argv"); return 1; } }
      strings
	{ $$ = $3; }
    ;
capenv
    : CAPV
	{
	    if (got_capv) { MyError(-1, -1, "extra capv"); return 1; }
	    ncaps = 0;
	}
      '{' caplist '}'
    ;

envopts
    : reference		
	{
	    $$.executable = $1;
	    $$.stack = DEFAULT_STACKSIZE;
	}
    | envopts ';'		{ $$ = $1; }
    | envopts ';' STACK NUMBER
	{	/* To do: convince someone that exec_file should round up */
	    long tmp;
	    tmp = $4;
	    /* Required by Sun memory management: */
	    tmp = (tmp + 8191) & ~0x1fff;
	    $$.stack = tmp;
	}
    | envopts ';' argv		{ $$ = $1; $$.argv = $3; }
    | envopts ';' strenv	{ $$ = $1; $$.envp = $3; }
    | envopts ';' capenv
	{
	    $$ = $1;
	    $$.ncaps = ncaps;
	    if (ncaps > 0) {
		$$.capv = (boot_caps *) mymalloc((int)sizeof(boot_caps)*ncaps);
		if ($$.capv == NULL) return 1;
		(void) memmove((char *) $$.capv, (char *) capv,
						sizeof(boot_caps) * ncaps);
	    } else $$.capv = NULL;
	}
    ;

cmd
    : '{' 
	{
	    got_argv = got_capv = got_environ = 0;
	}
      envopts
      '}'
	{
	    $$ = $3;
	}
    ;

%%

MyError(state, token, err)
    int state, token;
    char *err;
{
    ++n_errors;
    sprintf(parsstate, "Line %d: %s", LineNo, err);
    if (state >= 0)
	sprintf(parsstate + strlen(parsstate), ", state %d", state);
    if (token >= 0)
	sprintf(
	    parsstate + strlen(parsstate),
	    token > 127 ? ", token %d" : ", token '%c'",
	    token
	);
    (void) strcat(parsstate, "\n");
    prf("%n");
    prf(parsstate);
} /* MyError */

/*
 *	Concatenate two paths, check for overflow
 */
static bool
pathcat(first, last, to)
    char *first, *last, *to;
{
    if (first == NULL || *first == '\0') {
	if (last == NULL) to[0] = '\0';
	else (void) strcpy(to, last);
	return 1;
    } else if (last == NULL || *last == '\0') {
	(void) strcpy(to, first);
	return 1;
    }
    if (strlen(first) + 1 + strlen(last) >= MAX_PATH) {
	MyError(-1, -1, "resulting path too long");
	return 0;
    }
    while ((*to++ = *first++) != '\0')
	;
    --to;
    if (to[-1] != '/' && last[0] != '/') *to++ = '/';
    while ((*to++ = *last++) != '\0')
	;
    return 1;
} /* pathcat */

static long
FindCapName(s)
    char *s;
{
    long i;
    for (i = NnamedCaps; i-- > 0 && strcmp(NamedCaps[i].name, s); )
	;
    return i;
} /* FindCapName */
