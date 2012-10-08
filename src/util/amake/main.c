/*	@(#)main.c	1.4	96/02/27 13:06:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <ctype.h>
#include <stdio.h>
#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "dbug.h"
#include "getoptx.h"
#include "idf.h"
#include "scope.h"
#include "error.h"
#include "lexan.h"
#include "symbol2str.h"
#include "type.h"
#include "expr.h"
#include "eval.h"
#include "check.h"
#include "objects.h"
#include "builtin.h"
#include "execute.h"
#include "docmd.h"
#include "declare.h"
#include "assignment.h"
#include "tokenname.h"
#include "derive.h"
#include "cluster.h"
#include "caching.h"
#include "invoke.h"
#include "dump.h"
#include "slist.h"
#include "statefile.h"
#include "conversion.h"
#include "os.h"
#include "tools.h"
#include "generic.h"
#include "main.h"
#include "classes.h"

#if defined(unix) && !defined(sun)
/* Only use link() optimisation when we are pretty sure that every
 * `reasonable' command (read: any command needed to build Amoeba)
 * can be executed without exceeding system limits on the size of
 * exec() arguments.
 */
#include <sys/param.h>
#ifdef NCARGS
#define DEF_LINK_OPT	(NCARGS >= 0x40000)
#endif
#endif

PRIVATE char *Amake = "amake";	  /* name of the Amake program */
PUBLIC  char *Amakefile = NULL;	  /* Amake description file */

PUBLIC int			  /* when flag is TRUE: */
    F_no_exec = FALSE,		  /* don't execute commands */
    F_cleanup = FALSE,		  /* cleanup the .Amake dir and statefile */
    F_no_warnings = FALSE,	  /* don't give warnings on stderr */
    F_keep_intermediates = FALSE, /* don't remove intermediates */
    F_reading = FALSE,		  /* reading Amake input */
    F_verbose = FALSE,		  /* give more warnings */
    F_silent = FALSE,		  /* don't echo commands on stdout */
    F_one_version = FALSE,	  /* only keep one version */
    F_scheduling = TRUE,
#ifdef DEF_LINK_OPT
    F_link_opt = DEF_LINK_OPT,
#else
    F_link_opt = TRUE,		  /* avoid [un]link() calls when possible */
#endif
    F_globalize = FALSE,	  /* globalize local cid attributes */
    F_use_rsh = FALSE;

PUBLIC int
    Verbosity = 1;

#define USAGE0		"usage:"
#define USAGE1		"[-f file] [-L lib] [-D var[=expr]] [-p par] [-t file]"
#ifdef USE_RSH
#define USAGERSH 	"[-r]"
#endif
#define USAGEDBUG	"[-%d,name,..] [-%t]"
#define USAGE2		"[-v [level]] [-{cCknsvw}] [cluster ..]"

PRIVATE void
Usage()
{
   int n = strlen(USAGE0) + strlen(Amake) + 2;

   PrintError("%s %s %s", USAGE0, Amake, USAGE1);
#ifdef USE_RSH
   PrintError(" %s", USAGERSH);
#endif
   PrintError("\n");
   for (; n > 0; n--) {
       PrintError(" ");
   }
#ifdef DEBUG
   PrintError("%s ", USAGEDBUG);
#endif
   PrintError("%s\n", USAGE2);

   PrintError("-C\t\tcleanup: remove entire object pool and targets\n");
   PrintError("-D var[=expr]\tset `var' to `expr' (default %%true)\n");
   PrintError("-L lib\t\tput `lib' in front of the default searchpath\n");
   PrintError("-c\t\tcache only invocations used in current run\n");
   PrintError("-f file\t\tread from `file' instead of Amakefile\n");
   PrintError("-k\t\tkeep intermediate objects in this directory\n");
   PrintError("-n\t\tno command execution, only print them\n");
   PrintError("-p par\t\tparallelism (1..20), 0 forces blocking commands\n");
#ifdef USE_RSH
   PrintError("-r\t\trun commands remote (environment: RSH_POOL)\n");
#endif
   PrintError("-s\t\tsilent: don't print commands\n");
   PrintError("-t file\t\tconsider file touched\n");
   PrintError("-v [level]\tverbose, the higher `level' the more verbose\n");
   PrintError("-w\t\twarnings are not printed\n");
   PrintError("cluster\tmake `cluster' (target or cluster name) up-to-date\n");
}

#ifdef DEBUG
PRIVATE long struct_bytes;
PRIVATE long struct_num;

PRIVATE int
structs_left(p)
register char *p;
{
    int left = 0;

    for (; p != NULL; p = (char *) (((PALLOC_)p)->_A_next)) {
	left++;
    }
    return(left);
}

PUBLIC void
StructReport(name, size, number, free_list)
char *name;
int size, number;
#ifdef __STDC__
void *free_list;
#else
struct slist *free_list;
#endif
{
    int bytes = size * number;

    struct_bytes += bytes;
    struct_num += number;
    Verbose(3, "%12s: %6d * %2d = %7d (%5d left)",
	    name, number, size, bytes, structs_left((char *)free_list));
}


PRIVATE void
AllocationReport()
{
    extern int
	cnt_variable, cnt_function, cnt_cached, cnt_binding, cnt_cluster,
	cnt_derivation,	cnt_process, cnt_Generic, cnt_invocation, cnt_object,
	cnt_attr_descr,	cnt_param, cnt_scope, cnt_cons, cnt_slist, cnt_tool,
	cnt_type, cnt_expr;
    extern char *free_expr;

    Verbose(3, "Allocation report:");
    struct_bytes = struct_num = 0;
    JobStructReport();
    AttrStructReport();
    DerivationStructReport();
    StructReport("variable", sizeof(struct variable), cnt_variable,
		 h_variable);
    StructReport("function", sizeof(struct function), cnt_function,
		 h_function);
    StructReport("cached", sizeof(struct cached), cnt_cached, h_cached);
    StructReport("binding", sizeof(struct binding), cnt_binding, h_binding);
    StructReport("cluster", sizeof(struct cluster), cnt_cluster, h_cluster);
    StructReport("derivation", sizeof(struct derivation), cnt_derivation,
		 h_derivation);
    StructReport("process", sizeof(struct process), cnt_process, h_process);
    StructReport("Generic", sizeof(struct Generic), cnt_Generic, h_Generic);
    StructReport("invocation", sizeof(struct invocation), cnt_invocation,
		 h_invocation);
    StructReport("object", sizeof(struct object), cnt_object, h_object);
    StructReport("attr_descr", sizeof(struct attr_descr), cnt_attr_descr,
		 h_attr_descr);
    StructReport("param", sizeof(struct param), cnt_param, h_param);
    StructReport("scope", sizeof(struct scope), cnt_scope, h_scope);
    StructReport("cons", sizeof(struct cons), cnt_cons, h_cons);
    StructReport("slist", sizeof(struct slist), cnt_slist, h_slist);
    StructReport("tool", sizeof(struct tool), cnt_tool, h_tool);
    StructReport("type", sizeof(struct type), cnt_type, h_type);
    StructReport("expr", sizeof(struct expr), cnt_expr, free_expr);
    Verbose(3,"memory used for %ld structures: %ld", struct_num, struct_bytes);
}
#endif

/*
 * A dummy class job is introduced to make it possible to initialise the
 * datastructure for the new algorithm, allowing to make derivations (which,
 * of course, are jobs so may have to be suspended upon
 */
PRIVATE int ClassDummy;

struct dummy {
    char *d_descr;
    void (*d_func)();
};

PRIVATE struct job *
EnterDummy(descr, func)
char *descr;
void (*func)();
{
    struct dummy *d = (struct dummy *)Malloc(sizeof(struct dummy));

    d->d_descr = descr;
    d->d_func = func;
    return(CreateJob(ClassDummy, (generic)d));
}

PRIVATE void
RmDummy(d)
struct dummy *d;
{
    free((char *)d);
}

PRIVATE void
RunDummy(dp)
struct dummy **dp;
{
    DBUG_PRINT("dummy", ("dummy `%s' is being run", (*dp)->d_descr));
    (*dp)->d_func();
}

PRIVATE char *
PrDummy(d)
struct dummy *d;
{
    (void)sprintf(ContextString, "dummy `%s'", d->d_descr);
    return(ContextString);
}

PUBLIC void
InitDummies()
{
    ClassDummy = NewJobClass("dummy", (void_func) RunDummy,
			     (void_func) RmDummy, (string_func) PrDummy);
}

PRIVATE char AmakeLibDir[] = AMAKELIBDIR;

main(argc, argv, env)
int argc;
char *argv[];
char *env[];
{
    int ch;
    struct slist *other_args = NULL;
    struct slist *lib_dirs = NULL; /* library search path */
#ifdef DEBUG
    char *opt_string = "f:t:p:L:D:v*wnckrsCONGS%:?";
#else
    char *opt_string = "f:t:p:L:D:v*wnckrsCONGS?";
#endif
    DBUG_ENTER("main");
    DBUG_PROCESS("amake");

#ifndef AtariSt
    Amake = argv[0];
#endif

    DBUG_PUSH("d,exec"); /* always show command startup */

    /* init the next 2 first, they are use in path->object conversion */
    init_symbol_table();
    object_init();
    ConversionInit();
    InitOS();

    while ((ch = getopt(argc, argv, opt_string)) != EOF) {
	switch (ch) {
	case 'f': if (Amakefile == NULL) {
	    	      Amakefile = optarg;
		  } else {
		      Warning("only one -f option may be specified (ignored)");
		  }
	          break;
	case 'L': Append(&lib_dirs,
			 ObjectExpr(SystemNameToObject(optarg))); break;
	case 'p': {
	    		/* first check that optarg represents a number */
	    		register char *p;

			for (p = optarg; *p != '\0'; p++) {
			    if (!isdigit(*p)) {
				Usage();
				exit(EXIT_FAILURE);
			    }
			}
			if (!SetParallelism(atoi(optarg))) {
			    exit(EXIT_FAILURE);
			}
		  }
	          break;
#ifdef USE_RSH
	case 'r': F_use_rsh = TRUE; break;
#endif
	case 'D': CmdLineDefine(optarg); break;
	case 'n': F_no_exec = TRUE; break;
	case 'c': F_one_version = TRUE; break;
	case 'C': F_cleanup = TRUE; break;
	case 'w': F_no_warnings = TRUE; break;
	case 'v': F_verbose = TRUE;
	    	  if (optarg != NULL && '0' <= *optarg && *optarg <= '9') {
		      Verbosity = atoi(optarg);
		  }
	          break;
	case 'k': F_keep_intermediates = TRUE; break;
	case 's': F_silent = TRUE; break;
	case 't': TouchFile(optarg); break;

	/* temporary options: */
	case 'G': F_globalize = TRUE; break;
	case 'O': F_link_opt = TRUE; break;
	case 'S': F_scheduling = FALSE; break;
#ifdef DEBUG
	case '%':
	    DBUG_PUSH(optarg);
	    break;
#endif
	default:
	    Usage();
	    exit(EXIT_FAILURE);
	}
    }

    /* (un)link optimisation conflicts with the '-k' option: */
    if (F_keep_intermediates) {
	F_link_opt = FALSE;
    }

    if (Amakefile == NULL) { /* take default one */
	Amakefile = AMAKEFILE;
    }

#ifdef DEBUG
    Write = DefaultWriter;
#endif

    InitScope();
    InitCommand();
    InitDeclare();
    InitInclude();
    InitDefault();
    InitAssignments();
    InitDerive();
    InitInvokations();
    InitDummies();
    InitEval();

    type_init();
    expr_init();
    eval_init();

    Append(&lib_dirs, ObjectExpr(SystemNameToObject(AmakeLibDir)));

    builtin_init(lib_dirs);
    check_init();
    init_distr();


    /* The reset of the arguments should be cluster or object names
     * For the moment parse them as objects. We may find out later that
     * they are really cluster names.
     */
    for (ch = optind; ch < argc; ch++) {
	Append(&other_args, ObjectExpr(SystemNameToObject(argv[ch])));
    }

    InitClusters(other_args);

    init_lex();
    InitCaching();
    InitUpdate();

    CheckStatefile();

    ReadEnvironment(env);

    F_reading = TRUE;

    ExecuteCmdLineDefs();

    /* Statefile reading should be done before first, because otherwise tool
     * invocations present as in assignments aren't always cached. However,
     * currently the definition of a tool has to precede its use, so this
     * doesn't work.
     */

    read_from(Amakefile);
    LLparse();

    if (!ErrorsOccurred) {
	struct job *init_job;

	ReadStateFile();
	F_reading = FALSE;

	/* Initialisation of the classes may require doing derivations,
	 * which are jobs. Therefore the class-initialisation must itself
	 * be a (dummy) job.
	 */
	init_job = EnterDummy("init-classes", InitClasses);
	PutInReadyQ(init_job);
	StartExecution(init_job);

	AllClustersKnown();

#ifdef DEBUG
	AllocationReport();	/* allocation including statefile */
#endif
	StartExecution((struct job *)NULL); /* until no more jobs available */
	UpdateStateFile();
    }

    if (!F_keep_intermediates) {
	RemoveIntermediates();
    }

    CommandReport();
    ExecutionReport();

#ifdef DEBUG
    AllocationReport();
#endif

#ifdef EXPR_DEBUG
    ReportExprs();
#endif

    if (F_cleanup) {
	system("rm -rf .Amake");
	/* A more generic approach would be to use SystemName(AmakeDir)
	 * but in the (unlikely) event that an amake bug causes this to
	 * deliver something odd (e.g., "/") this might have  drastic.
	 * consequences. Better be safe than sorry.
	 */
    }

    exit(GetExitStatus());
    /*NOTREACHED*/
}
