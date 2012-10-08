/*	@(#)servergen.c	1.2	94/04/07 14:38:57 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"

/*
 *	Return parameter that can be used for Impl_repl_buf,
 *	as a pointer to an argument in the op_server list.
 *	(it is only relevant to servers).
 */
struct itlist *
LonelyPar(name, op)
    char *name;
    struct optr *op;
{
    struct itlist *arg;
    char *lonely;	/* Name of the lonely parameter	*/

    if (op == NULL) {
	mypf(ERR_HANDLE, "%r unknown operator %s\n", name);
	return NULL;
    }
    ServerAnalysis(op);
    /*
     *	This is only possible if there's 1 arg in the replybuffer
     */
    if (op->op_out->td_meml->il_next != NULL) {
	mypf(ERR_HANDLE, "%r %s: not exactly one out parameter\n", name);
	return NULL;
    }
    assert(op->op_out != NULL);
    assert(op->op_out->td_meml != NULL);
    lonely = op->op_out->td_meml->il_name;
    for (arg = op->op_server; arg != NULL; arg = arg->il_next) {
	if (arg->il_name == lonely)
	    break;
    }
    assert(arg != NULL);
    /*
     *	This restriction is technically not necessary, but
     *	it is not clear how the prototype of the corresponding
     *	impl_ routine should be if it has more than 1 in-buffer parameter
     */
    if (op->op_ib > 1 && arg->il_bits & AT_REQUEST) {
	mypf(ERR_HANDLE, "%r %s of %s is an in-parameter as well\n",
	    lonely, op->op_name);
	return NULL;
    }
    return arg;
} /* LonelyPar */

/*
 *	Remember that the implementation has its own
 *	reply buffer by putting it in own_repl[].
 */
/* static */ void
SetOwnRepl(name, op)
    char *name;
    struct optr *op;
{
    if (op == NULL) {
	mypf(ERR_HANDLE, "%r unknown operator %s\n", name);
	return;
    }
    if (LonelyPar(name, op) != NULL) SetFlag(op->op_val, FL_IMPLBUF);
} /* SetOwnRepl */

/*
 *	Does the implementation of this operator provide
 *	its own reply buffer? If so, point to that single arg
 */
struct itlist *
DoesOwnRepl(op)
    struct optr *op;
{
    return (GetFlags(op->op_val) & FL_IMPLBUF) ? LonelyPar("", op) : NULL;
} /* DoesOwnRepl */

/* static */ void
SetPassAct(name, op)
    char *name;
    struct optr *op;
{
    if (op == NULL) mypf(ERR_HANDLE, "%r unknown operator %s\n", name);
    else SetFlag(op->op_val, FL_PASSACT);
} /* SetPassAct */

/*
 *	Remember that we want to pass the activate-function return value
 *	to the implementation for this procedure (server). Default: all.
 */
void
PassAct(args)
    struct gen *args;
{
    if (args == NULL) {	/* Flag all the operators	*/
	struct clist *cl;
	struct name *nm;
	/* The ones defined in this class: */
	for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next)
	    if (nm->nm_kind == CALL) SetPassAct(nm->nm_name, nm->nm_optr);
	/* The other visible ones: */
	for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next)
	    for (nm = cl->cl_this->cl_scope; nm != NULL; nm = nm->nm_next)
		if (nm->nm_kind == CALL) SetPassAct(nm->nm_name, nm->nm_optr);
    } else FindOptrs(args, Yes, SetPassAct);
} /* PassAct */

/*
 *	Remember (de)activate functions and type for server 
 */
void
ActDeAct(args)
    struct gen *args;
{
    if (Activate != AC_NO) {
	mypf(ERR_HANDLE, "%r cannot call this generator twice\n");
	return;
    }
    Activate = AC_GEN;
    if (args == NULL) goto argcount;
    ActType = args->gn_name; args = args->gn_next;
    if (args == NULL) goto argcount;
    ActFunc = args->gn_name; args = args->gn_next;
    if (args == NULL) goto argcount;
    DeactFunc = args->gn_name; args = args->gn_next;
    if (args == NULL) return;
argcount:
    mypf(ERR_HANDLE, "%r wrong argcount, should be 3\n");
} /* ActDeAct */

/*
 *	Generate a server-mainloop.
 */
void
ServerGen(args)
    struct gen *args;
{
    struct name *nm;
    Bool mon, loop, rights_check; /* Some switches	*/
    int buf_size;	/* Requested size of the buffer	*/

    mon = No;		/* No monitor code by default	*/
    rights_check = No; /* No rights checking by default(!?) */
    loop = Yes;		/* Loop by default		*/
    buf_size = BS_AIL;	/* Compute buffersize self by default	*/
    dbg2("Generating a server for class %s\n", ThisClass->cl_name);
    /*
     *	Parse the argument list, setting flags as you go.
     */
    while (args != NULL) {
	if (strcmp(args->gn_name, "rights_check") == 0) {
	    if (args->gn_value != NULL) {
		mypf(ERR_HANDLE, 
		    "%r server argument \"rights_check\"can't have a value\n");
		return;
	    }
	    rights_check = Yes;
	} else if (strcmp(args->gn_name, "monitor") == 0) {
	    if (args->gn_value != NULL) {
		mypf(ERR_HANDLE, 
		    "%r server argument \"monitor\"can't have a value\n");
		return;
	    }
	    mon = Yes;
	} else if (strcmp(args->gn_name , "buf_size") == 0) {
	    if (buf_size != BS_AIL) {
		mypf(ERR_HANDLE, "%r buf_size and my_buf don't mix\n");
		return;
	    }
	    if (args->gn_value == NULL) {
		mypf(ERR_HANDLE, "%r missing value of buf_size\n");
		return;
	    } else {
		/* The buffersize is constant	*/
		if (!(args->gn_value->et_const)) {
		    mypf(ERR_HANDLE, "%r buf_size must be constant\n");
		    return;
		}
		buf_size = args->gn_value->et_val;
		if (buf_size < 0) {
		    mypf(ERR_HANDLE,
			"%r buf_size can't be negative.\n");
		    return;
		}
	    }
	} else if (strcmp(args->gn_name , "no_loop") == 0) {
	    loop = No;	/* Don't loop	*/
	} else if (strcmp(args->gn_name, "my_buf") == 0) {
		if (args->gn_value != NULL || args->gn_type != NULL) {
		    mypf(ERR_HANDLE,
			"%r my_buf shouldn't have a type or value\n");
		    return;
		}
		if (buf_size != BS_AIL) {
		    mypf(ERR_HANDLE, "%r buf_size and my_buf don't mix\n");
		    return;
		}
		buf_size = BS_ARG;
	} else {					/* typo		*/
	    mypf(ERR_HANDLE,
		"%r unrecognized argument %s\n", args->gn_name);
	    return;
	}
	args = args->gn_next;
    }
    MainLoop(mon, buf_size, loop);
    for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next)
	if (nm->nm_kind == CALL) ServerOptr(nm->nm_optr, rights_check);
    GenServer(loop, rights_check); /* Also generates inherited operators */
} /* ServerGen */

void
ImplRepl(p)
    struct gen *p;
{
    FindOptrs(p, Yes, SetOwnRepl);
} /* ImplRepl */

/*
 *	Put dummy implementation functions of a class on a file
 */
/*ARGSUSED*/
static void
ImplClassGen(h, cl)
    handle h;
    struct class *cl;
{
    struct name *nm;
    mypf(h, "\n/* Class %s: */\n\n", cl->cl_name);
    for (nm = cl->cl_scope; nm != NULL; nm = nm->nm_next) {
	if (nm->nm_kind == CALL) {
	    mypf(h, "/*ARGSUSED*/\n");
	    OptrHead(h, nm->nm_optr, No, No);
	    /* Make a dummy body: */
	    mypf(h, "\n{%>%nreturn STD_ARGBAD;%<%n} /* impl_%s */\n\n",
		nm->nm_name);
	}
    }
} /* ImplClassGen */

/*
 *	Generate dummy impl_ routines.
 */
/*ARGSUSED*/
void
ImplGen(args)
    struct gen *args;
{
    struct clist *cl;
    char fname[200];
    handle h;
    sprintf(fname, "%s.dummy", ThisClass->cl_name);
    h = OpenFile(fname);
    mypf(h, "/*\n *\tDummy implementation routines\n *\tfor %s by Ail\n */\n",
	ThisClass->cl_name);
    mypf(h, "#include <ailamoeba.h>\n");
    mypf(h, "#include \"%s.h\"\n", ThisClass->cl_name);
    ImplClassGen(h, ThisClass);
    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next)
	ImplClassGen(h, cl->cl_this);
    CloseFile(h);
} /* ImplGen */
