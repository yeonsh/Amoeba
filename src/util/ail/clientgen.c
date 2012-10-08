/*	@(#)clientgen.c	1.2	94/04/07 14:32:14 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"

/*
 *	Function to set "dynamic".
 */
void
Dynamic(arg)
    struct gen *arg;
{
    assert(arg);
    assert(arg->gn_next == NULL);	/* Guaranteed by Generate()	*/
    assert(arg->gn_type == NULL);	/*	"	*/
    assert(arg->gn_value == NULL);	/*	"	*/
    if (strcmp(arg->gn_name, "alloca") == 0) {
	dynamic = B_ALLOCA;
    } else if (strcmp(arg->gn_name, "malloc") == 0) {
	dynamic = B_MALLOC;
    } else if (strcmp(arg->gn_name, "off") == 0) {
	dynamic = B_ERROR;
    } else {
	mypf(ERR_HANDLE, "%r unknown allocator %s\n", arg->gn_name);
    }
} /* Dynamic */

static void
SetIdem(name, optr)
    char *name;
    struct optr *optr;
{
    if (optr == NULL) {
	mypf(ERR_HANDLE, "%r operator %s unknown\n", name);
    } else {
	SetFlag(optr->op_val, FL_IDEMPOTENT);
    }
} /* SetIdem */

/*
 *	Flags procedures as idempotent, perhaps set the RetryCount.
 */
void
Idempotent(arg)
    struct gen *arg;
{
    if (RetryCount != 0) {
	mypf(ERR_HANDLE, "%r generator cannot be called twice\n");
	return;
    }
    if (arg != NULL && strcmp(arg->gn_name, "retry") == 0
    && arg->gn_value != NULL && arg->gn_value->et_const) {
	RetryCount = arg->gn_value->et_val;
	if (RetryCount <= 0) {
	    mypf(ERR_HANDLE, "%r negative retry count %d\n", RetryCount);
	    return;
	}
	arg = arg->gn_next;
    } else {
	RetryCount = 5;	/* Default retry count	*/
    }
    if (arg == NULL) {
	mypf(ERR_HANDLE, "%r no operators specified\n");
	return;
    }
    FindOptrs(arg, 1, SetIdem);
} /* Idempotent */

void
ClGen(name, op)
    char *name;
    struct optr *op;
{
    char fname[256];
    handle fp;
    if (op == NULL) {	/* Means that FindOptrs couldn't find it	*/
	mypf(ERR_HANDLE, "%r Unknown operator %s\n", name);
    } else if (op->op_flags & OP_STARRED) {	/* '*' as first arg	*/
	/* Open an output file:	*/
	sprintf(fname, "%s.c", op->op_name);
	fp = OpenFile(fname);
	ClientOptr(fp, op);
	CloseFile(fp);
    } else {		/* Probably an operator that wraps another function */
	sprintf(fname, "%s.c.dummy", op->op_name);
	fp = OpenFile(fname);
	mypf(fp, "/* No '*' parameter defined; edit this function */\n");
	mypf(fp, "#include \"%s.h\"\n", ThisClass->cl_name);
	OptrHead(fp, op, Yes, No);
	mypf(fp, "\n{\n\t-- Your code --\n} /* %s */\n", op->op_name);
	CloseFile(fp);
    }
} /* ClGen */

/*
 *	CClientGen() : the code generator of the
 *	C-client stubs for the Amoeba transation layer.
 */
/*ARGSUSED*/
void
CClientGen(args)
    struct gen *args;
{
    struct name *nm;
#ifdef PYTHON
    if (lang == L_PYTHON_SC) {
	xPythonClientGen(args);
	return;
    }
#endif
    if (args != NULL)
	FindOptrs(args, 0, ClGen);
    /* If not, generate all of the clientstubs defined within the class	*/
    else for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next) {
	if (nm->nm_kind == CALL)
	    ClGen(nm->nm_name, nm->nm_optr);
	if (ErrCnt) break;	/* Something went wrong	*/
    }
} /* CClientGen */

static void
ClMon(name, optr)
    char *name;
    struct optr *optr;
{
    if (optr == NULL) {
	mypf(ERR_HANDLE, "%r procedure %s undefined\n", name);
    } else {
	SetFlag(optr->op_val, FL_CLIENTMON);
    }
} /* ClMon */

/*
 *	MonClientGen(): tell ail that the
 *	client stubs need monitor calls.
 */
/*ARGSUSED*/
void
MonClientGen(args)
    struct gen *args;
{
    /* If no arguments, monitor all stubs by setting a global flag */
    if (args == NULL) MonitorAll = Yes;
    else FindOptrs(args, 1, ClMon);
} /* MonClientGen */

/*
 *	Generate all clientstubs for the inherited classes
 */
void
AllStubs(args)
    struct gen *args;
{
    struct clist *clp;
    for (clp = ThisClass->cl_effinh; clp != NULL; clp = clp->cl_next) {
	ThisClass = clp->cl_this;
	CClientGen(args);
	if (ErrCnt) break;
    }
} /* AllStubs */

/*
 *	Put the headers of the clientstubs on a file
 *	Assumes nobody wants ANSI lintlibraries.
 */
void
LintClass(fp, cl)
    handle fp;
    struct class *cl;
{
    struct name *nm;
    int old_lang;
    old_lang = lang;
    lang = L_TRADITIONAL;
    mypf(fp, "\n\n/*\n *\tClass %s:\n */", cl->cl_name);
    for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next)
	if (nm->nm_kind == CALL) {
	    mypf(fp, "\n/*ARGSUSED*/\n");
	    OptrHead(fp, nm->nm_optr, Yes, No);
	    mypf(fp, "\n{ return -1; }\n");
    }
    lang = old_lang;
} /* LintClass */

/*
 *	Wrapper for LintClass().
 *	Open & write a lintlib.
 */
/*ARGSUSED*/
void
LintGen(args)
    struct gen *args;
{
    struct clist *cl;
    handle fp;
    char fname[256];
    sprintf(fname, "llib-l%s", ThisClass->cl_name);
    fp = OpenFile(fname);
    mypf(fp, "#include <%s>\n#include \"%s.h\"\n",
	general_header_file, ThisClass->cl_name);
    mypf(fp, "/*LINTLIBRARY*/");
    LintClass(fp, ThisClass);
    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next)
	LintClass(fp, cl->cl_this);
    CloseFile(fp);
} /* LintGen */
