/*	@(#)headergen.c	1.2	94/04/07 14:33:07 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"
#include "gen.h"

# define islower(ch)	('a' <= ch && ch <= 'z')
# define toupper(ch)	(ch + 'A' - 'a')

/*
 *	Generate #defines for the operators of a class.
 */
/*ARGSUSED*/
void
DefGen(args)
    struct gen *args;
{
    struct name *nm;
    char fname[1024];
    handle fp;
    sprintf(fname, "%s_def.h", ThisClass->cl_name);
    fp = OpenFile(fname);
    for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next) {
	if (nm->nm_kind == CALL) {
	    struct optr *op;
	    register char *cp;
	    mypf(fp, "#define ");
	    op = nm->nm_optr;
	    for (cp = op->op_name; *cp != '\0'; ++cp)
		mypf(fp, "%c", islower(*cp) ? toupper(*cp) : *cp);
	    mypf(fp, "\t%d\n", op->op_val);
	}
    }
    CloseFile(fp);
} /* DefGen */

/* static */ void
TestCompiler(h, tongue)
    handle h;
    int tongue;	/* Insert some code to inhibit the usage of a wrong compiler */
{
    switch (tongue) {
    case L_TRADITIONAL:
	mypf(h,
	"#if __STDC__\n#include \"traditional_c_required\"\n#endif\n");
	break;
    case L_ANSI:
	mypf(h,
	"#if !__STDC__\n#include \"ansi_c_required\"\n#endif\n");
	break;
    }
} /* TestCompiler */

/*
 *	Return (statically allocated) symbol
 *	used to prevent double inclusion.
 */
static char *
ProtectSymbol(cl)
    struct class *cl;
{
    static char ret[200];
    register char *chp, *rp;

    ret[0] = ret[1] = '_';
    rp = &ret[2];
    for (chp = cl->cl_name; *chp; ++chp)
	*rp++ = islower(*chp) ? toupper(*chp) : *chp;
    *rp++ = '_';
    *rp++ = 'H';
    *rp++ = '_';
    *rp++ = '_';
    *rp = '\0';
    return ret;
} /* ProtectSymbol */

/*
 *	Write the contents of a plain <class>.h file
 *	No includes for the inherited classes; only
 *	for the ones of the 'include' directive.
 */
static void
WriteHeader(fp, cl, baroque)
    handle fp;
    struct class *cl;
    Bool baroque;	/* Don't care about the size of the output?	*/
{
    struct name *nm;
    struct stind *incls;
    Bool ansi_problem;	/* Can't mix prototypes with old C always	*/
    Bool first_line;	/* Last line printed was first			*/

    /*
     * Include the files of the "include" directive
     */
    if ((incls = cl->cl_incl) != NULL) {
	mypf(fp, "\n/* Include directives: */\n");
	for (; incls != NULL; incls = incls->st_nxt) {
	    /*
	     * If the string starts with a '<' and ends with a '>', I don't
	     * need to quote it. Thus, include "<stdio.h>" in the sources
	     * will result in #include <stdio.h> in the target
	     */
	    if (incls->st_str[0] == '<' &&
	      incls->st_str[strlen(incls->st_str)-1] == '>') {
		mypf(fp, "#include %s\n", incls->st_str);
	    } else {
		mypf(fp, "#include \"%s\"\n", incls->st_str);
	    }
	}
    }

    /*
     *	#define the rights.
     */
    first_line = Yes;
    for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next) {
	if (nm->nm_kind == RIGHTS) {
	    if (first_line) {
		mypf(fp, "\n/* Rights: */\n");
		first_line = No;
	    }
	    mypf(fp, "#define %s 0x%X\n", nm->nm_name, nm->nm_ival);
	}
    }

    /*
     *	#define the constants.
     */
    first_line = Yes;
    for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next) {
	/* A constant with a type is an enumeration constant	*/
	assert(nm->nm_kind != INTCONST || nm->nm_type != NULL);
	if (nm->nm_kind == INTCONST && nm->nm_type->td_kind != tk_enum) {
	    if (first_line) {
		mypf(fp, "\n/* Constants: */\n");
		first_line = No;
	    }
	    mypf(fp, "#define %s %D\n", nm->nm_name, nm->nm_ival);
	} else if (nm->nm_kind == STRCONST) {
	    if (first_line) {
		mypf(fp, "\n/* Constants: */\n");
		first_line = No;
	    }
	    mypf(fp, "#define %s \"%s\"\n", nm->nm_name, nm->nm_sval);
	}
    }

    /*
     *	Declare the local types. There shouldn't be any RefTo's.
     */
    mypf(fp, "\n");
    ScopeType(fp, cl->cl_scope);

    /*
     *	Declare the operators as prototypes and traditional functions.
     *	On every line is one argument, preceded by a comment telling its type.
     */
    ansi_problem = No;
    first_line = Yes;
    switch (lang) {
    case L_ANSI:	/* Make prototypes:	*/
	for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next)
	    if (nm->nm_kind == CALL) {
		if (first_line) {
		    mypf(fp, "/* Operators: */\n");
		    first_line = No;
		}
		if (nm->nm_optr->op_flags & OP_INCOMPAT)
		    ansi_problem = Yes;
		OptrHead(fp, nm->nm_optr, Yes, No);
		mypf(fp, ";\n\n");
	    }
	break;
    case L_TRADITIONAL:
	/*
	 *	Define at least the return values of the clientstubs.
	 */
	for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next)
	    if (nm->nm_kind == CALL) {
		if (first_line) {
		    mypf(fp, "/* Operators: */\n");
		    first_line = No;
		}
		if (nm->nm_optr->op_flags & OP_INCOMPAT)
		    ansi_problem = Yes;
		if (baroque) OptrHead(fp, nm->nm_optr, Yes, Yes);
		else mypf(fp, "extern %s %s();\n",
			client_stub_return_type, nm->nm_name);
	    }
    	break;
    default:
	mypf(ERR_HANDLE,
	    "%r no headers for this language\n");
    }
    if (ansi_problem) TestCompiler(fp, lang);
    if (!first_line) mypf(fp, "\n");
} /* WriteHeader */

/*
 *	Generate an including <class>.h file
 */
/*ARGSUSED*/
void
HeaderGen(args)
    struct gen *args;
{
    struct clist *cl;
    handle fp;
    char fname[200];
    char *sym;
    Bool baroque;

    if (args == NULL) baroque = 0;
    else {
	if (strcmp(args->gn_name, "baroque") == 0) baroque = 1;
	else mypf(ERR_HANDLE, "%r unknown option '%s'\n", args->gn_name);
    }

    /*
     *	Construct the output filename & open
     */
    sprintf(fname, "%s.h", ThisClass->cl_name);
    fp = OpenFile(fname);

    /*
     *	Prevent double inclusion:
     */
    sym = ProtectSymbol(ThisClass);
    mypf(fp, "#ifndef %s\n", sym);
    mypf(fp, "#define %s\n", sym);

    /*
     *	Include the inherited class-file definitions
     */
    if ((cl = ThisClass->cl_definh) != NULL) {
	mypf(fp, "\n/* Inherited classes: */\n");
	for (; cl != NULL; cl = cl->cl_next)
	    mypf(fp, "#include \"%s.h\"\n", cl->cl_this->cl_name);
    }
    WriteHeader(fp, ThisClass, baroque);
    mypf(fp, "#endif /* %s */\n", sym);
    CloseFile(fp);
} /* HeaderGen */

/*
 *	Call HeaderGen for each effectively inherited class.
 */
/*ARGSUSED*/
void
AllHeaders(args)
    struct gen *args;
{
    struct clist *cl;
    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next) {
	ThisClass = cl->cl_this;
	HeaderGen(args);	/* I have to pass something...	*/
    }
    /* Let Generate() restore ThisClass	*/
} /* AllHeaders */

/*
 *	Create a flat headerfile for the by-definition-inherited classes,
 *	with #includes for the classes that are only effectively inherited.
 */
/*ARGSUSED*/
void
FlatHeader(args)
    struct gen *args;
{
    struct clist *def, *eff;
    handle fp;
    char fname[200], *sym;
    Bool did_inc;	/* Did we generate an include statement?	*/
    /*
     *	Open the file:
     */
    sprintf(fname, "%s.h", ThisClass->cl_name);
    fp = OpenFile(fname);
    sym = ProtectSymbol(ThisClass);
    mypf(fp, "#ifndef %s\n", sym);
    mypf(fp, "#define %s\n\n", sym);
    mypf(fp, "/*\n *\tFlat interface for\n");
    mypf(fp, " *\tclass %s by Ail.\n */\n", ThisClass->cl_name);

    /*
     *	Include the effectively inherited classes
     *	that are not included by definition.
     */
    did_inc = No;
    for (eff = ThisClass->cl_effinh; eff != NULL; eff = eff->cl_next) {
	/*
	 *	Skip if inherited by definition as well
	 */
	for (def = ThisClass->cl_definh;
	    def != NULL && def->cl_this != eff->cl_this;
	    def = def->cl_next)
		;
	if (def == NULL) {	/* Not in cl_definh => include	*/
	    if (!did_inc) {
		did_inc = Yes;
		mypf(fp, "\n/* Implicitely inherited classes: */\n");
	    }
	    mypf(fp, "#include \"%s.h\"\n", eff->cl_this->cl_name);
	}
    }

    /*
     *	Write out the definitions in the classes of cl_definh,
     *	followed by the class itself.
     */
    for (def = ThisClass->cl_definh; def != NULL; def = def->cl_next)
	WriteHeader(fp, def->cl_this, 0);
    WriteHeader(fp, ThisClass, 0);

    mypf(fp, "#endif /* %s */\n", sym);
    CloseFile(fp);
} /* FlatHeader */
