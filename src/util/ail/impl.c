/*	@(#)impl.c	1.2	94/04/07 14:33:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"
# include <errno.h>
extern int errno;

    /*********************************************************************

	Dear hackers,

	To make a new code generator for ail, create a new function
	that will be called by Generate(), and make new entries in
	the genen[] array.

	It yields good karma to study dbg.c, bfile.c and myprintf.c
	if you're writing a generator.

	Some generators have different implementations for each language.
	These are the ones with the extra {}'s. They list the
	implementation in the order of the L_* macros: NULL at the zeroth
	position (to mark there actually is a per-language implementation),
	K&R C first, then ansi and lint, and finally Python.
	Actually, the #ifdef PYTHON means python support.

	Sincerely yours, Siebren.

    **********************************************************************/

struct patch genen[] = {

    "Act_deact",		IM_NOFP|IM_IDUNI,
    "Server calls (de)activate functions",
    { ActDeAct },

    "client_interface",		IM_NOFP|IM_IDUNI,
    "Definitions of a class contents",
#ifdef PYTHON
    { NULL, HeaderGen, HeaderGen, NULL, xPythonDocGen },	
#else
    { NULL, HeaderGen, HeaderGen, NULL, NULL },
#endif

    "client_stubs",		IM_NOFP|IM_IDUNI,
    "Generate client-stubs",
#ifdef PYTHON
    { NULL, CClientGen, CClientGen, LintGen, xPythonClientGen },	
#else
    { NULL, CClientGen, CClientGen, LintGen, NULL },	
#endif

    "command",			IM_NOFP|IM_IDUNI,
    "Generate program that does name_lookup/trans",
    { NULL, CommandGen, CommandGen, NULL, NULL },	

    "complete_interface",	IM_NOARG,
    "Like client_interface, for all inherited classes",
#ifdef PYTHON
    { NULL, AllHeaders, AllHeaders, NULL, AllPythonDocs },
#else
    { NULL, AllHeaders, AllHeaders, NULL, NULL },
#endif
    "complete_stubs",		IM_NOFP,
    "Like client_stubs, for all inherited classes",
#ifdef PYTHON
    { NULL, AllStubs, AllStubs, NULL, AllPythonStubs },
#else
    { NULL, AllStubs, AllStubs, NULL, NULL },
#endif
    "define",			IM_NOARG,
    "#Define the procedure numbers",
    { NULL, DefGen, DefGen, NULL, NULL },	

    "feature",			IM_NOFP|IM_IDUNI,
    "Generate several nifty C-variables",
    { Feature },

    "Get_mem",			IM_ONEARG|IM_NOFP,
    "How to allocate memory",
    { Dynamic },	

    "Idempotent",		0,
    "Declare a procedure as idempotent",
    { Idempotent },	

    "impl_dummy",		IM_NOARG,
    "Generate dummy implementations",
    { ImplGen },	

    "Impl_repl_buf",		IM_NOFP|IM_IDUNI,
    "Tell which implementations return a replybuffer",
    { ImplRepl },	

    "Language",			IM_ONEARG|IM_NOFP,
    "Select an output language",
    { Language },	

    "marsh_funcs",		IM_ONEARG|IM_NOFP,
    "Generate marshaling functions",
    { MarshPair },	

    "Monitor_client",		IM_NOFP|IM_IDUNI,
    "Monitor client-stubs",
    { MonClientGen },

    "output_files",		IM_ONEARG|IM_NOCLASS,
    "Tell which files were genereted",
    { ListFiles },	

    "Output_directory",		IM_ONEARG|IM_NOFP,
    "Set the output directory",
    { SetDir },	

    "Pass_acttype",		IM_NOFP|IM_IDUNI,
    "Pass activate type to these implementations",
    { PassAct },	

    "server",			IM_NOTYPE|IM_IDUNI,
    "server-mainloop",
#ifdef PYTHON
    { NULL, ServerGen, ServerGen, NULL, xPythonSvGen },	
#else
    { NULL, ServerGen, ServerGen, NULL, NULL },	
#endif

    /***	The ones I'm less sure about have names starting in '_' ***/

    "_Post_deact",		IM_NOARG,
    "Call deactivation function after putrep",
    { SetPostDeact },

    "_class_list",		IM_NOARG|IM_NOCLASS,
    "List all known classes on stdout",
    { ShowClassGen },

    "_contents",		IM_NOARG,
    "List contents of a class on stdout",
    { Verbose },	

    "_flat_interface",		IM_NOARG,
    "Definitions of a class + explicitely inherited ones",
    { FlatHeader },	

    "_ordinal",			IM_ONEARG|IM_NOFP,
    "Generate function to compute the ordinal of a string",
    { EnumOrd },	

    "_type_list",		IM_NOFP|IM_NOCLASS,
    "Display types on stdout",
    { ShowTypTab },	

};

/*
 *	Since this loop was made at several places by my own code,
 *	I decided that it might be a good idea to write a function
 *	that does it, so that code-generator hackers won't have to
 *	figure out trivial stuff like this.
 *
 *	This function is intended to be called by a code-generator.
 *	FindOptrs attempts to find the operators within the current
 *	class by name.  If search_deep is set, it also tries to find
 *	the function in the inherited classes.
 *	Then it calls the function with both a pointer to the gen
 *	and the operator, which is NULL if the operator could not
 *	be found.
 */
void
FindOptrs(p, search_deep, func)
    struct gen *p;
    int search_deep;
    void (*func)();
{
    struct name *nm;
    /*
     *	Loop over the identifiers in p.
     */
    for (;p != NULL; p = p->gn_next) {
	/* To suppress cascades of error messages:	*/
	if (ErrCnt) return;
	/* Find the operator:	*/
	for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next)
	    if (p->gn_name == nm->nm_name) break;
	if (search_deep && nm == NULL) {
	    struct clist *clp;
	    for (clp=ClassList; nm==NULL && clp!=NULL; clp=clp->cl_next) {
		for (nm=clp->cl_this->cl_scope; nm!=NULL; nm=nm->nm_next)
		    if (p->gn_name == nm->nm_name)
			/*
			 *	This breaks two loops, since nm
			 *	is NULL at the end of the list
			 */
			break;
	    }
	}
	if (nm != NULL && nm->nm_kind != CALL) {
	    nm = NULL;
	}
	(*func)(p->gn_name, (nm == NULL) ? (struct optr *) NULL : nm->nm_optr);
    }
} /* FindOptrs */

void
SetDir(ptr)
    struct gen *ptr;
{
    /*
     *	The test against "." is needed on SunOS:
     *	mkdir thinks "." is an invalid argument...
     */
    if (options & OPT_NOOUTPUT) return;
    errno = 0;
    outdir = ptr->gn_name;
    if ((outdir[0] != '.' || outdir[1] != '\0') &&
      mkdir(outdir, 0777) != 0 && errno != EEXIST) {
	perror(outdir);
	fatal("That's the outputdirectory!");
	/*NOTREACHED*/
    }
} /* SetDir */

/*ARGSUSED*/
void
SetPostDeact(ptr)
    struct gen *ptr;
{
    PostDeact = Yes;
} /* SetPostDeact */

/*
 *	All strings in ail are unique, so they can be
 *	compared with == instead of strcmp().
 *	This function takes care that the "names" of the
 *	code generators are such shared strings.
 */
void
ImplInit()
{
    register int i;
    for (i = 0; i < sizeof(genen)/sizeof(genen[0]); ++i)
	genen[i].impl_name = StrFind(genen[i].impl_name);
} /* ImplInit */

/*
 *	Generate: call a code generator.
 *	Some checks are done here, so that generators need
 *	not worry about them: previous type-errors, multiple
 *	declared identifiers.
 *	The arguments to the generator are reversed by the
 *	parser. Generate reverses them back.
 *	Generate also takes care that ThisClass points to
 *	ImplClass by the time a generator is called, and when
 *	the parser is active. The latter is important since
 *	expressions are evaluated in ThisClass.
 */
void
Generate(name, args)
    char *name;
    struct gen *args;
{
    struct gen *p;
    int flags, indx;

    if (options & OPT_DEBUG) ShowTypTab((struct gen *) NULL);
    ThisClass = ImplClass;
    /* Find the corresponding gen:	*/
    for (indx = 0; indx < sizeof(genen)/sizeof(genen[0]); ++indx)
	if (genen[indx].impl_name == name) break;
    if (indx == sizeof(genen)/sizeof(genen[0])) {
	mypf(ERR_HANDLE, "%r Unknown generator %s\n", name);
	return;
    }
    genname = name;	/* So that error messages are more informative	*/
    /* Do some checks	*/
    flags = genen[indx].impl_bits;
    if (ImplClass == &GlobClass && !(flags & IM_NOCLASS)) {
	mypf(ERR_HANDLE, "%r should not be called without class\n");
	goto stop;
    }
    if (args != NULL && (flags & IM_NOARG)) {
	mypf(ERR_HANDLE, "%r no arguments allowed\n");
	goto stop;
    }
    if ((flags & IM_ONEARG) && (args == NULL || args->gn_next != NULL)) {
	mypf(ERR_HANDLE, "%r exactly one argument required\n");
	goto stop;
    }

    /*
     *	Check and reverse arguments
     */
    p = args;
    args = NULL;
    while (p != NULL) {
	if (p->gn_type == &ErrDesc) goto stop;	/* Previous type-error	*/
	if ((flags & IM_NOVAL) && p->gn_value != NULL) {
	    mypf(ERR_HANDLE, "%r no value allowed\n");
	    goto stop;
	}
	if ((flags & IM_NOTYPE) && p->gn_type != NULL) {
	    mypf(ERR_HANDLE, "%r no types allowed\n");
	    goto stop;
	}
	if (flags & IM_IDUNI) {
	    register struct gen *q;
	    /* Check the rest of the identifiers:	*/
	    for (q = p->gn_next; q != NULL; q = q->gn_next) {
		if (p->gn_name == q->gn_name) {
		    mypf(ERR_HANDLE, "%r arg %s mentioned twice\n", p->gn_name);
		    goto stop;
		}
	    }
	}

	{   register struct gen *tmp;
	    /* Move to args	*/
	    tmp = p->gn_next;
	    p->gn_next = args;
	    args = p;
	    p = tmp;
	    assert(!((long)p & 0x01L));
	    assert(!((long)args & 0x01L));
	}
    }
    if (ErrCnt != 0) return;
    /* Find implementation and call it:	*/
    if (genen[indx].impl_func[0] != NULL) (*genen[indx].impl_func[0])(args);
    else {	/* Language specific; find implementation */
	if (genen[indx].impl_func[lang] == NULL)
	    mypf(ERR_HANDLE, "%r no %s implementation for %s\n",
		languages[lang], name);
	else (*genen[indx].impl_func[lang])(args);
    }
    ThisClass = ImplClass;
stop:
    genname = NULL;
} /* Generate */

/*
 *	Called when "generate foo {" has been parsed.
 *	Sets ImplClass (foo) and ThisClass so that
 *	expressions are evaluated correct.
 *	Also reset some other internal switches.
 */
void
StartGenerate(name)
    char *name;
{
    dynamic = B_ERROR;	/* Detest using dynamic amounts of storage	*/
    UnFlagOp();		/* Forget implementation details		*/
    lang = dfltoutlang;	/* Select default output language		*/
    outdir= dfltoutdir;	/* Using the default output directory		*/
    ThisClass = ImplClass = name == NULL ? &GlobClass : FindClass(name);
    ActFunc = DeactFunc = ActType = (char *) NULL;
    RetryCount = 0;
    MonitorAll = No;
    PostDeact = No;
    NoKeywords = Yes;	/* The scanner recognizes eg 'int' as an identifier */
    Activate = AC_NO;

    /*
     *	Make sure there is an output-directory
     *	The test against "." is needed on Suns.
     */
    errno = 0;
    if ((outdir[0] != '.' || outdir[1] != '\0') &&
      mkdir(outdir, 0777) != 0 && errno != EEXIST) {
	perror(outdir);
	fatal("That's the outputdirectory!");
	/*NOTREACHED*/
    }
} /* StartGenerate */

/*
 *	Called when the generate clause is closed (at '};').
 */
void
StopGenerate()
{
/* ShowOptrs(ThisClass->cl_scope); */
    ThisClass = &GlobClass;
    ImplClass = NULL;
    NoKeywords = No;	/* Scanner understands the keywords again	*/
} /* StopGenerate */

char *languages[] = {	/* _MUST_ be in the same order as the L_ macros */
    "Any language",
    "traditional",
    "ansi",
    "lint",
    "python"
};

int
GetLang(s)
    char *s;
{
    int l;
    for (l = 1; l <= N_LANGUAGES; ++l)
	if (strcmp(s, languages[l]) == 0) return l;
    mypf(ERR_HANDLE, "%r unknown language %s\n", s);
    return dfltoutlang;
} /* GetLang */

/*
 *	Tell in which language several
 *	outputs should be written
 */
/*ARGSUSED*/
void
Language(args)
    struct gen *args;
{
    lang = GetLang(args->gn_name);
} /* Language */

/*
 *	For the -g option.
 */
void
ListGenerators(c)
    int c;
{
    register int i;
    mypf(OUT_HANDLE, c == 'G' ?
	"The obscure generators are:\n" :
	"This version of ail knows these generators:\n");
    for (i = 0; i < sizeof(genen)/sizeof(genen[0]); ++i) {
	if ((genen[i].impl_name[0] != '_') != (c == 'G')) {
	    mypf(OUT_HANDLE, "%S-  ", genen[i].impl_name);
	    mypf(OUT_HANDLE, "%s\n", genen[i].impl_remark);
	}
    }
} /* ListGenerators */

#if defined(SYSV) || defined(GEMDOS)
mkdir(dir, mode)
    char *dir;
    int mode;
{
    char *buf;

    buf = MyAlloc(strlen(dir)+20);
    if (buf == 0) {
	errno = ENOMEM;
	return -1;
    }
    sprintf(buf, "mkdir %s", dir);
    if (system(buf) != 0 ) {
	free(buf);
	errno = EEXIST;
	return -1;
    }
    free(buf);
    return chmod(dir, mode);
}
#endif
