/*	@(#)server.c	1.5	96/02/27 12:44:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

/*
 *	This file contains the routines that generate the mainloop.
 *	These routines depend on the fact that only ONE serverloop
 *	is generated at a time.
 */

static handle	/* Handles are a replacement for filepointers	*/
	whole,	/* Handle for the servermainloop file		*/
	b_decl,	/* Handle for local declarations (the buffer)	*/
	top;	/* Backpatch for declarations above the server	*/

/*
 *	BufSize indicates how to obtain a buffer, or how large if >= 0.
 */
static int BufSize;
static long BiggestOp;	/* Size of biggest operation, -1 if unknown	*/

/*
 *	Generate the signature of a server loop
 */
/*ARGSUSED*/
static void
ServerHead(h, loop)
    handle h;
{
    mypf(h, "%s%nml_%s", mainloop_return_type, ThisClass->cl_name);
    switch (BufSize) {
    case BS_ARG:	/* It's an argument		*/
	switch (lang) {
	case L_TRADITIONAL:
	    mypf(h, "(_prt, _buf, _buf_sz)%>%n");
	    mypf(h, "port *_prt;\nchar _buf[];%n");
	    mypf(h, "unsigned int _buf_sz; /* Size of _buf[] */%<%n");
	    break;
	case L_ANSI:
	    mypf(h, "(port *_prt, char _buf[], unsigned int _buf_sz)%n");
	    break;
	default:
	    fatal("Unknown language!?");
	}
	break;
    case BS_AIL:	/* Ail has to figure it out	*/
    default:		/* The programmer told ail	*/
	mypf(h, lang == L_ANSI ?
	    "(port *_prt)%n" :
	    "(_prt)%>%nport *_prt;%<%n");
	break;
    }
} /* ServerHead */

/*
 *	Emit declarations for activate functions, if any
 *	If we use this, we need a variable _err as well.
 *	BUG: should take the Language into account.
 *	This won't bite as long as ail only knows some C flavours.
 */
/* static */ void
ActivateDecl(fp)
    handle fp;
{
    switch (Activate) {	/* Declare (de)activate stuff */
    case AC_GEN:
	mypf(fp, "%s _obj;%nerrstat _err, ", ActType);
	mypf(fp, "%s( /* (private *), (%s *) */ );%n",
				ActFunc, ActType);
	mypf(fp, "extern void %s( /* %s */ );%n", DeactFunc, ActType);
	break;
    }
} /* ActivateDecl */

/*
 *	Perhaps call the activate function:
 *
 *	if ((_err = <act_fun>(&_hdr.h_priv, &_obj)) != STD_OK) {
 *		_hdr.h_status = err;
 *		goto _fout;
 *	}
 */
/* static */ void
MaybeActivate(fp)
    handle fp;
{
    char *id;

    switch (Activate) {
    case AC_GEN:
	id = "_obj";
	break;
    default:
	return;
    }
    MarshState.ms_errlab = Yes;
    mypf(fp, "%nif ((_err = %s(&_hdr.h_priv, &%s)) != STD_OK) {%>",
	ActFunc, id);
    mypf(fp, "%nMON_EVENT(\"activate: bad cap\");");
    mypf(fp, "%n_hdr.h_status = _err;%ngoto _fout;%<%n}");
} /* MaybeActivate */

/*
 *	Perhaps call the deactivate function
 */
/* static */ void
MaybeDeactivate(fp)
    handle fp;
{
    if (Activate != AC_NO)
	mypf(fp, "%nif (_err == STD_OK) %s(_obj);", DeactFunc);
} /* MaybeDeactivate */

/*
 *	Emit an aligned buffer declaration on stack
 */
static void
AllocTransBuf(fp, n, comment)
    handle fp;
    int n;	/* Size of the buffer */
    char *comment;
{
    extern int Do_malloc_reqbuf;

    if (Do_malloc_reqbuf) {
	/* malloced data is already suitably aligned */
	mypf(fp, "%nstruct _mybuf_t {%>%nchar _buf_raw[%d]; /* %s */",
	     n, comment);
	mypf(fp, "%<%n} *_mybuf;\n");
	mypf(fp, "%n_mybuf = (struct _mybuf_t*) malloc(sizeof(struct _mybuf_t));");
	mypf(fp, "%nif (_mybuf == NULL) return STD_NOMEM;");
	mypf(fp, "\n#define _buf _mybuf->_buf_raw\n");
    } else {
	/* declare aligned buffer of given size on the stack */
	char *alg_tp;

	if (ThisClass->cl_align <= 1) {
	    mypf(fp, "%nchar _buf[%d]; /* %s */\n", n, comment);
	    return;
	}
	switch (ThisClass->cl_align) {
	case 2:
	    alg_tp = "int16";
	    break;
	case 4:
	    alg_tp = "int32";
	    break;
	case 8:
	    alg_tp = "double";
	    break;
	default:
	    assert(0);
	}
	mypf(fp, "%nunion {%>%nchar _buf_raw[%d]; /* %s */", n, comment);
	mypf(fp, "%n%s _buf_algn;%<%n} _buf_un;", alg_tp);
	mypf(fp, "\n#define _buf _buf_un._buf_raw\n");
    }
} /* AllocTransBuf */

/*
 *	Open the file, print first part of ml_<class>().
 *	The parameters to ml_*() depend highly on the
 *	way the code generator was invoked.
 */
void
MainLoop(mon, buf_size, loop)
    Bool mon;		/* Include monitor.h?	*/
    int buf_size;	/* If > 0, use this buffersize	*/
    Bool loop;		/* Produce a loop?	*/
{
    char cname[40];
    Minit("_buf");
    sprintf(cname, "ml_%s.c", ThisClass->cl_name);
    whole = OpenFile(cname);
    mypf(whole, "/*\n *\tServer-loop for %s by Ail.\n */\n",
	ThisClass->cl_name);
    mypf(whole,
	"#include <ailamoeba.h>\n#include <stdlib.h>\n#include \"%s.h\"\n\n",
	ThisClass->cl_name);
    mypf(whole, mon ?
	"#include <monitor.h>\n" :
	"/* Don't #include <monitor.h> */\n");
    mypf(whole, "#ifndef MON_EVENT\n#define MON_EVENT(Off)\n#endif\n\n");
    BufSize = buf_size;		/* Save buffer-discipline	*/
    BiggestOp = 0;		/* Biggest buffer required	*/
    top = BackPatch(whole);
    ServerHead(whole, loop);
    mypf(whole, "{%>%nheader _hdr;%n");	/*}*/
    ActivateDecl(whole);

# ifdef DIRTY_COMPAT_HACK
    mypf(whole, "short _ret;\t/*COMPAT*/");
# else
    mypf(whole, "long _ret;");
# endif /* DIRTY_COMPAT_HACK */
    mypf(whole, "%nregister char *_adv;");
    b_decl = BackPatch(whole);	/* Here you can insert other local decls */

    /*
     *	Create a buffer somehow:
     */
    if (BufSize != BS_AIL && BufSize > 0) {
	AllocTransBuf(whole, BufSize, "As you asked for");
    }

    /*
     *	First few statements:
     *	Get a request, check against buffer overflow.
     *	The assignment to _hdr.h_port should be done
     *	outside the loop, since Guido shouldn't mess
     *	with the header. Maybe an option to server?
     */
    if (loop) mypf(whole, "%nfor (;;) {%>");	/*}*/
    if (GetAnyFlag() & FL_IMPLBUF) {
	mypf(whole, "%nchar *_rep; /* some impl_*s provide reply buffer */");
	mypf(whole, "%n_rep = _buf;");
    }
    mypf(whole, "%n_hdr.h_port = *_prt;\t/* Port to listen to */");
    mypf(whole, "%ndo {%>%n");
    switch (BufSize) {
    case BS_AIL:
	/*
	 *	The third argument is a macro, and
	 *	the second too if we happen not to
	 *	need a buffer
	 */
	mypf(whole, "_ret = getreq(&_hdr, _buf, sizeof(_buf));");
	break;
    case BS_ARG:
	mypf(whole, "_ret = getreq(&_hdr, _buf, _buf_sz);");
	break;
    case 0:	/* No buffer	*/
	mypf(whole, "_ret = getreq(&_hdr, (char *) 0, 0);");
	break;
    default:
	assert(BufSize > 0);
	mypf(whole, "_ret = getreq(&_hdr, _buf, sizeof(_buf));");
    }
    mypf(whole, "%<%n} while (_ret == RPC_ABORTED);%nif (_ret < 0) {%>");
    mypf(whole, "%nMON_EVENT(\"Negative getreq\");");
#if 0
    if (loop)
	mypf(whole, "%nreturn; /* Should never happen */%<%n}");
    else
	mypf(whole, "%nreturn _ret; /* Should never happen */%<%n}");
#else
    /* Guido wants it this way; see ServerHead()	*/
    mypf(whole, "%nreturn _ret; /* Should never happen */%<%n}");
#endif
    mypf(whole, "%n_adv = _buf;\n");
    MaybeActivate(whole);
    mypf(whole, "%nswitch (_hdr.h_command) {"); /* } */
} /* MainLoop */

/*
 *	Return sum of maximum size for parameters that travel ONE direction
 */
static long
StealCnt(from, to, dir)
    struct itlist *from, *to;
    int dir;
{
    int otherdir;
    long count = 0;	/* How many byte of storage we'd steal */
    switch (dir) {
    case AT_REQUEST:
	otherdir = AT_REPLY;
	break;
    case AT_REPLY:
	otherdir = AT_REQUEST;
	break;
    default:
	assert(No);
    }
    for (;from != to; from = from->il_next) {
	assert(from != NULL);
	if (from->il_attr->at_hmem == NULL && (from->il_bits & dir) &&
	    !(from->il_bits & otherdir) && CharsOnly(from->il_type)) {
		/* We can steal this one	*/
		if (from->il_type->td_msize != NULL
		&& from->il_type->td_msize->et_const) {
		    count += from->il_type->td_msize->et_val;
		} else {	/* Don't know maximum size; Guess it's HUGE */
		    count += 1000;
		}
	}
    }
    return count;
} /* StealCnt */

/*
 *	Steal the parameters that travel ONE direction
 */
static void
StealDir(from, to, dir)
    struct itlist *from, *to;
    int dir;
{
    int otherdir;
    switch (dir) {
    case AT_REQUEST:
	otherdir = AT_REPLY;
	break;
    case AT_REPLY:
	otherdir = AT_REQUEST;
	break;
    default:
	assert(No);
    }
    for (;from != to; from = from->il_next) {
	assert(from != NULL);
	if (from->il_attr->at_hmem == NULL && (from->il_bits & dir) &&
	    !(from->il_bits & otherdir) && CharsOnly(from->il_type)) {
		/* Steal this one	*/
		from->il_bits |= AT_NODECL|AT_ADVADV|AT_NOMARSH;
	}
    }
} /* StealDir */

/*
 *	Massage the argumentlist of an operator
 *	to reflect the server's view on the optr.
 */
void
ServerAnalysis(op)
    struct optr *op;
{
    struct itlist *past_io, *past_fix, *arg;
    int best;	/* Best direction to steal from	*/

    assert(op->op_flags & OP_STARRED);
    if (op->op_flags & OP_SERVER) return;
    op->op_flags |= OP_SERVER;
    if (op->op_flags & OP_NORPC) {
	mypf(ERR_HANDLE, "%r %s not generatable by ail\n", op->op_name);
	return;
    }
    op->op_server = CopyItList(op->op_args);

    /*
     *	Try to avoid as much declarations as you can,
     *	since any declaration means an assignment or copy.
     *	This cannot be done as efficient as in the client-
     *	stub: the implementation routine should not be
     *	able to overwrite one of its in-parameters just
     *	because it has the same address as some out-par.
     *	This complicates the code a lot.
     *
     *	Note that not declaring it DOESN'T necessarily
     *	mean no marshaling should be done; it might be
     *	done inplace.
     *	However, since there isn't any inplace marshaling
     *	yet, we require that no marshaling is necessary,
     *	so effectively it is the same.
     *	The reply (thus marshaling, for the server) also
     *	involves advancing the _adv-pointer, since _adv
     *	is used in the third parameter of putrep.
     *	If this is the only thing to do, AT_ADVADV is set.
     *
     *	The strategy used is suboptimal. It uses knowledge
     *	on the buffer layout done by DefOptr:
     *
     *	Req:	[Fixed InOut's][Fixed In's][Dynamic InOut's][Dynamic In's]
     *	Reply:	[Fixed InOut's][Fixed Out's][Dynamic InOut's][Dynamic Out's]
     *			       ^	   ^
     *	past_io ---------------|	   |
     *	past_fix --------------------------|
     */

    /*
     *	Make past_io point to the first parameter beyond the Fixed InOut's
     */
    for (past_io = op->op_server; past_io != NULL; past_io = past_io->il_next) {
	if ((past_io->il_bits & (AT_REQUEST|AT_REPLY)) != (AT_REQUEST|AT_REPLY)
	    || past_io->il_type->td_esize == NULL
	    || !past_io->il_type->td_esize->et_const)
		break;
    }

    /*
     *	Make past_fix point to the first parameter beyond the Fixed ones
     */
    for (past_fix = past_io; past_fix != NULL; past_fix = past_fix->il_next) {
	if (past_fix->il_type->td_esize == NULL
	|| !past_fix->il_type->td_esize->et_const)
	    break;
    }

    /*
     *	First of all, steal as many fixed inout's as you can (never overlap)
     *	Don't steal headerfields. (This could happen to eg capabilities)
     */
    for (arg = op->op_server; arg != past_io; arg = arg->il_next) {
	if (arg->il_attr->at_hmem == NULL && CharsOnly(arg->il_type))
	    arg->il_bits |= AT_NODECL|AT_ADVADV|AT_NOMARSH;
	/* Check whether a cast is needed	*/
	if (!(arg->il_bits & AT_CHARBUF)) arg->il_bits |= AT_CAST;
    }

    /*
     *	See whether we're better of stealing in- or out
     *	parameters, then steal the fixed ones.
     *	I prefer to steal in parameters, because out's can be
     *	Impl_repl_buf'd (not really important)
     */
    if (StealCnt(op->op_server, (struct itlist *) NULL, AT_REPLY)
    < StealCnt(op->op_server, (struct itlist *) NULL, AT_REQUEST))
	best = AT_REQUEST;
    else
	best = AT_REPLY;
    StealDir(op->op_server, past_fix, best);

    /*
     *	And steal the first dynamic one that travels the same direction,
     *	if possible
     */
    for (arg = past_fix; arg != NULL; arg = arg->il_next) {
	if (arg->il_bits & best) {
	    if (CharsOrIntsOnly(arg->il_type)) {
	        arg->il_bits |= AT_NODECL|AT_ADVADV|AT_NOMARSH;
	        break;
	    }
	}
    }

    /*
     *	Should steal nonconflicting parameters
     *	that travel the other way here
     */

    /*
     *	Loop once more over the parameters to patch types
     */
    for (arg = op->op_server; arg != NULL; arg = arg->il_next) {
	if (arg->il_attr->at_hmem != NULL) {	/* Not in the buffer	*/
#if 0
	    if (!(arg->il_bits & AT_REDECL) &&
	    	CharsOrIntsOnly(arg->il_type)) {/* Not exact the same type */
#else
	    if (!(arg->il_bits & AT_REDECL)) {/* Not exact the same type */
#endif
		arg->il_bits |= AT_NODECL;
	    }
# if 0
	    continue;
# endif
	}
	if (arg->il_type->td_kind == tk_ref) {
	    /*
	     *	A server doesn't have the thing itself anywhere
	     *	in its address space, so the thing has to be
	     *	declared in the serverstub.
	     *	So a <thing> has to be declared, but to impl_*()
	     *	we pass a <ref-to thing>.
	     *	This is a neccesary braindamage of the type-thing.
	     *	Be warned!
	     */
	    arg->il_type = CopyOf(arg->il_type);
	}
    }
} /* ServerAnalysis */

/*
 *	This routine tests that the request buffer has the right size
 *	if this is a constant, and sets MarshState.ms_check accordingly.
 *	Whenever the test fails, it sets _hdr.h_status and breaks.
 *	The latter assumes that the check is made at a point where
 *	the closest surrounding loop or switch (I'm talking C here)
 *	is the switch on the original h_command.
 */
/* static */ void
TestBuf(fp, op)
    handle fp;
    struct optr *op;
{
    long size;

    if (op->op_in->td_esize != NULL && op->op_in->td_esize->et_const) {
	MarshState.ms_check = NULL;	/* No unchecked members	*/
	size = op->op_in->td_esize->et_val;
    } else {
	MarshState.ms_check = op->op_server;	/* The first one	*/
	return;
    }
    /* Lots of procedures only exploit the header:	*/
    mypf(fp, "%nif (_ret != %d) {%>", size);
    mypf(fp, "%n_hdr.h_status = AIL_SVR_IN_BAD;");
    mypf(fp, "%nMON_EVENT(\"Bad requestsize for %s\");", op->op_name);
    mypf(fp, "%nbreak;%<%n}");
} /* TestBuf */

/*
 *	Generate the case numbers of a server operator:
 */
static void
ServerCases(fp, op)
    handle fp;
    struct optr *op;
{
    if (op->op_alt != NULL) {
	struct alt *ap;
	mypf(fp, "%n/* Alternative opcodes: */");
	for (ap = op->op_alt; ap != NULL; ap = ap->alt_next) {
	    mypf(fp, "%ncase %d:", ap->alt_int);
	}
    }
    mypf(fp, "%ncase %d: MON_EVENT(\"%s request\"); {%>",
						op->op_val, op->op_name);
} /* ServerCases */

/*
 *	Declare the implementation of an operator on a handle.
 *	TO DO: make this work for any language?
 */
void
DeclImpl(fp, op)
    handle fp;
    struct optr *op;
{
    if (lang == L_TRADITIONAL) {
	mypf(fp, "%nerrstat impl_%s();", op->op_name);
    }
}

/*
 *	Lookup offset of field in structure. Used when calling the
 *	implementation. We must pass two types: one for the request,
 *	one for the reply. The strategy of marking arguments as
 *	passable-straight-from-the-buffer ensures that the answer
 *	would be the same; whichever type we would use.
 */
static int
FieldOffs(t1, t2, id)
    struct typdesc *t1, *t2;
    char *id;
{
    struct itlist *il = NULL;
    assert(t1 == NULL || t1->td_kind == tk_struct);
    assert(t2 == NULL || t2->td_kind == tk_struct);
    assert(t1 != NULL || t2 != NULL);

    if (t1 != NULL)
	for (il= t1->td_meml; il != NULL && il->il_name != id; il= il->il_next)
	    ;
    if (il == NULL)
	for (il= t2->td_meml; il != NULL && il->il_name != id; il= il->il_next)
	    ;
    assert(il != NULL);
    return il->il_offs;
} /* FieldOffs */

/*
 *	Call a server implementation.
 *	The tricky part of this is, that the serverimplementor
 *	probably wants to have his arguments in the order that
 *	they were defined, instead of the order DefOptr() made.
 *	Order is set in the right order, arg is the server-way
 *	to think about it.
 *	Another exception when ownbuf: there should be exactly
 *	one argument in the buffer, which is passed different.
 */
static void
ServerCallImpl(fp, op, ownbuf)
    handle fp;
    struct optr *op;
    struct itlist *ownbuf;
{
    extern int Do_pass_arraysize;
    struct itlist *walk;
    struct typdesc *type;
    mypf(fp, "%n_hdr.h_status = impl_%s(&_hdr", op->op_name); /*)*/

    /* Pass activate argument? */
    if (GetFlags(op->op_val) & FL_PASSACT) switch (Activate) {
	case AC_GEN:
	    mypf(fp, ", _obj");
	    break;
    }

    /* Pass other arguments */
    for (walk = op->op_order; walk != NULL; walk = walk->il_next) {
	struct itlist *arg;
	mypf(fp, ", ");
	for (arg = op->op_server;
	    arg->il_name != walk->il_name;
	    arg = arg->il_next)
		;
	/*
	 *	Exception if ownbuf:
	 */
	if (arg == ownbuf) {
	    mypf(fp, "&_rep, &_rep_sz");
	    continue;
	}

	type = arg->il_type;
	/* Those fancy types are for marshaling	*/
	if (type->td_kind == tk_copy) type = type->td_prim;
	if (type->td_kind == tk_ref) type = type->td_prim;
	assert(type->td_kind != tk_ref && type->td_kind != tk_copy);
	while (type->td_kind == tk_tdef) type = type->td_prim;
	assert(type->td_kind != tk_ref && type->td_kind != tk_copy);

	/* Now where is the parameter?	*/
	if (arg->il_attr->at_hmem != NULL && !(arg->il_bits & AT_REDECL)) {
	    /* In the header and typesafe	*/
	    if (arg->il_attr->at_bits & AT_VAR && type->td_kind != tk_arr &&
		!(type->td_bits & TD_CPREF))
		    mypf(fp, "&");
	    mypf(fp, "_hdr.%s", arg->il_attr->at_hmem);
	} else if (arg->il_bits & AT_NODECL) {
	    /* In the buffer	*/
	    int offs;
	    if (arg->il_bits & AT_CAST) {
		/* Might be, say, an integer array; cast	*/
		if (arg->il_bits & AT_VAR) type = PointerTo(type);
		Cast(fp, type);
	    }
	    assert(arg->il_attr->at_hmem == NULL || (arg->il_bits & AT_REDECL));
	    offs = FieldOffs(op->op_in, op->op_out, arg->il_name);
	    if (offs == 0) mypf(fp, "_buf");
	    else mypf(fp, "_buf + %d", offs);
	} else {
	    /*
	     *	Maybe in the header, maybe in the buffer,
	     *	but unmarshaled to a local variable.
	     */
	    if (arg->il_attr->at_bits & AT_VAR && type->td_kind != tk_arr &&
		!(type->td_bits & TD_CPREF))
		    mypf(fp, "&");
	    mypf(fp, "%s", arg->il_name);
	}
	if (type->td_kind == tk_arr && Do_pass_arraysize) {
	    /* generate array bound */
	    if (type->td_bound != NULL) {
		if (type->td_bound->et_const) {
		    mypf(fp, ", /* bound: */ ");
		    ExprSpell(fp, type->td_bound, 100,
			      op->op_server, No);
		} else {
		    mypf(fp, ", /* non-const bound: */ ");
		    ExprSpell(fp, type->td_bound, 100,
			      op->op_server, No);
		}
	    } else {
		mypf(fp, ", /* no bound: */ -1");
	    }
	}
    }
    mypf(fp, ");");
} /* ServerCallImpl */

/*
 *	Marshal the output variables of a server stub
 *
 *	output:
 *		if (_hdr.h_status == 0) {
 *			marshal header;
 *			marshal buffer;
 *		} else {
 *			MON_EVENT(failure);
 *		}
 */
static void
ServerMarshalOut(fp, op, ownbuf)
    handle fp;
    struct optr *op;
    struct itlist *ownbuf;
{
    mypf(fp, "%nif (_hdr.h_status == 0) {%>");
    /*
     *	Marshal output-variables
     *	The non-header ones need not be marshaled if ownbuf.
     */
    if (op->op_oh > 0) {
	HeadMsh(fp, op->op_server, AT_REPLY, AT_NODECL);
    }
    Mreinit(ownbuf ? "_rep":"_buf");
    if (op->op_ob > 0) {
	if (ownbuf) {	/* impl_ gives me the buffer and the size	*/
	    mypf(fp, "%n_adv = _rep + _rep_sz;");
	} else {
	    if (op->op_flags & OP_INTSENS) {
		mypf(fp, "%nif (_eok) {%>");
		Marshal(fp, AT_REPLY, op, INT_GOOD);
		mypf(fp, "%<%n} else {%>");
		Mreinit(ownbuf ? "_rep":"_buf");
		Marshal(fp, AT_REPLY, op, INT_BAD);
		mypf(fp, "%<%n} /* _eok */");
	    } else {
		Marshal(fp, AT_REPLY, op, INT_GOOD);
	    }
	}
    }
    /* Terminate the if-impl-worked	*/
    mypf(fp, "%<%n} else {%>");
    mypf(fp, "%nMON_EVENT(\"%s failure\");%<%n}", op->op_name);
} /* ServerMarshalOut */

/*
 *	Generate a serverstub.
 */
void
ServerOptr(op, rights_check)
    struct optr *op;
    Bool rights_check;	/* Perform rights checking?	*/
{
    struct itlist *ownbuf;	/* Impl routine provides reply buffer	*/

    if (ErrCnt != 0) return;	/* Don't do anything if errors exist	*/
    ThisOptr = op;		/* For error messages			*/
    /* Useles operator def?	*/
    if (!(op->op_flags & OP_STARRED)) {
	mypf(whole, "%n/* Operator %s has no capability */", op->op_name);
	return;
    }
    ServerAnalysis(op);
    if (ErrCnt) return;

    /*
     *	Needed for the calculation of the server buffer-size
     */
    if (!(op->op_in->td_msize != NULL && op->op_in->td_msize->et_const)
    || !(op->op_out->td_msize != NULL && op->op_out->td_msize->et_const)) {
	/* No constant size; forget it */
	BiggestOp = -1;
    } else if (BiggestOp >= 0) {
	/* Maybe this operator is bigger than we ever saw before?	*/
	if (op->op_in->td_msize->et_val > BiggestOp)
	    BiggestOp = op->op_in->td_msize->et_val;
	if (op->op_out->td_msize->et_val > BiggestOp)
	    BiggestOp = op->op_out->td_msize->et_val;
    }
    ownbuf = DoesOwnRepl(op);
    if (ownbuf == NULL && BufSize == 0 && (op->op_ib || op->op_ob)) {
	/* The buffersize can be demanded by the user	*/
	mypf(ERR_HANDLE, "%r operator %s requires a buffer\n", op->op_name);
	return;
    }
#if 0
    /* BUG:
     * I should test that the buffer
     * isn't dimensioned too small,
     * but this code won't do that
     */
    if (BufSize >= 0 && op->op_out->td_msize != NULL
    && op->op_out->td_msize->et_const && BufSize < op->op_out->td_msize->et_val)
	mypf(ERR_HANDLE, "%w requested buffer too small for %s\n", op->op_name);
#endif

    ServerCases(whole, op);	/* Case statements	*/
    if (ownbuf != NULL) {
	mypf(whole, "%n/* impl_%s has out-%s in some buffer */",
	    op->op_name, ownbuf->il_name);
    }
    DeclImpl(whole, op);

    /*
     *	Generate the declarations:
     */
    /* Make sure DeclList doesn't declare too much:	*/
    if (ownbuf != NULL && !(ownbuf->il_bits & AT_REQUEST) &&
				CharsOrIntsOnly(ownbuf->il_type))
	ownbuf->il_bits |= AT_NODECL;
    DeclList(whole, op->op_server);
    if (ownbuf != NULL && !(ownbuf->il_bits & AT_REQUEST) &&
				CharsOrIntsOnly(ownbuf->il_type))
	ownbuf->il_bits &= ~AT_NODECL;

    /* Miscellaneous declarations: */
    if (ownbuf) {
	mypf(whole, "%nlong _rep_sz;");
    }
    if (op->op_ailword != NULL) {
	if (op->op_flags & OP_INTSENS)
	    mypf(whole, "%nint _eok;");
	if (op->op_flags & OP_FLTSENS)
	    mypf(whole, "%nint _floatrep is not implemented;");
	/* Got the right declarations; now set them:	*/
	if (op->op_flags & OP_INTSENS) {
	    mypf(whole, "%n_eok = ((_ailword & AIL_INTMASK) == ");
	    mypf(whole, "(_hdr.%s & AIL_INTMASK));", op->op_ailword);
	}
	if (op->op_flags & OP_FLTSENS) {
	    mypf(whole, "%nEven the kernel doesn't support floats!");
	    mypf(whole, "%nWhat would you expect?");
	}
    }

    /*
     *	Check validity of the request
     */
    TestBuf(whole, op);
    if (rights_check) {
	if (op->op_rights != 0) {
	    mypf(whole, "%nif ((_hdr.h_priv.prv_rights & 0x%x) != 0x%x) {%>",
		op->op_rights, op->op_rights);
	    mypf(whole, "%nMON_EVENT(\"insufficient rights for %s request\");",
		op->op_name);
	    mypf(whole, "%n_hdr.h_status = STD_DENIED;");
	    mypf(whole, "%nbreak;%<%n}");
	} else mypf(whole, "%n/* No rights required */");
    }

    /*
     *	Unmarshal the input-variables
     *	Ideally, all non-SENS stuff should be done outside the
     *	if statement.
     */
    if (op->op_ih) {
	HeadUnmsh(whole, op->op_server, AT_REQUEST, AT_NODECL);
    }
    Mreinit("_buf");
    MarshState.ms_scope = op->op_server;
    if (op->op_ib > 0) {		/* Buffer parameters	*/
	if (op->op_flags & OP_INTSENS) { /* Need to do it two ways:	*/
	    mypf(whole, "%nif (_eok) {%>");
	    Unmarshal(whole, AT_REQUEST, op, INT_GOOD);
	    mypf(whole, "%<%n} else {%>");
	    Mreinit("_buf");
	    Unmarshal(whole, AT_REQUEST, op, INT_BAD);
	    mypf(whole, "%<%n}");
	} else {
	    Unmarshal(whole, AT_REQUEST, op, INT_GOOD);
	}
    }

    ServerCallImpl(whole, op, ownbuf);

    if (op->op_oh + op->op_ob > 0)
	ServerMarshalOut(whole, op, ownbuf);
    else mypf(whole,
	"%nif (_hdr.h_status) MON_EVENT(\"%s failure\");", op->op_name);

    /*{*/
    mypf(whole, "%nbreak;%<%n}\n");
    ThisOptr = NULL;
} /* ServerOptr */

/*
 *	Generate the tail of the server-mainloop,
 *	and fill the b_decl-hole with a declaration
 *	of a buffer, and top with the missing
 *	marshaling macro's.
 *
 *	All inherited operators are generated at
 *	this point too (by calling ServerOptr).
 */
void
GenServer(loop, rights_check)
    Bool loop, rights_check;
{
    struct name *nm;
    struct clist *clp;	/* Inherited classes	*/
    struct class *Yuck;	/* I'm not proud...	*/
    /* Some for vi: {{{ */

    Yuck = ThisClass;
    for (clp = ThisClass->cl_effinh; clp != NULL; clp = clp->cl_next) {
	ThisClass = clp->cl_this;
	mypf(whole, "\n\n\t/* Inherited from class %s: */",
	    clp->cl_this->cl_name);
	for (nm = clp->cl_this->cl_scope; nm != NULL; nm = nm->nm_next) {
	    if (nm->nm_kind == CALL) ServerOptr(nm->nm_optr, rights_check);
	    /* Size analysis should be done here	*/
	}
    }
    ThisClass = Yuck;
    mypf(whole, "%ndefault:%>%n_hdr.h_status = AIL_H_COM_BAD;");
    mypf(whole, "%nMON_EVENT(\"Unrecognized h_command\");%<%n} /* switch */");

    if (MarshState.ms_errlab)	/* Label to jump to when eg marshaling fails */
	mypf(whole, "\n_fout:");

    /*
     *	Call putrep() and (perhaps) the deactivator in the rights order:
     */
    if (!PostDeact) MaybeDeactivate(whole);
    if (GetAnyFlag() & FL_IMPLBUF)
    {
	/* Some impl_ returns its own buffer.
	 * BUG: currently it does the same as in the "else" case.
	 * Surely that cannot be correct!
	 */
	mypf(whole,
	"%nputrep(&_hdr, _buf, (bufsize)(_hdr.h_status ? 0 : _adv - _rep));");
    }
    else
	mypf(whole,
	"%nputrep(&_hdr, _buf, (bufsize)(_hdr.h_status ? 0 : _adv - _buf));");
    if (PostDeact) MaybeDeactivate(whole);

    /*
     *	Terminate (perhaps) the loop and the function
     */
    if (loop) mypf(whole, "%<%n} /* for (;;) */");
    mypf(whole, loop ? "%n/*NOTREACHED*/" : "%nreturn 0;");
    mypf(whole, "%<%n} /* ml_%s */\n", ThisClass->cl_name);

    /*
     *	Generate declarations of used marshalers,
     *	and allocate the transaction buffer if needed.
     *	These are written at the top of the function.
     */
    if (MarshState.ms_shcast) {
	MarshState.ms_shcast = No;
	mypf(b_decl, "%nshort _pcc; /* Pcc signextension bug */");
    }

    if (BufSize == BS_AIL) {
	/* Allocate the buffer (on the stack or using malloc) */
	if (BiggestOp < 0) {
	    mypf(ERR_HANDLE, "%w Can't compute buffersize; using 30000\n");
	    AllocTransBuf(b_decl, 30000, "Not computed");
	} else {
	    /* Easy way out if no buffer is required at all	*/
	    if (BiggestOp == 0)
		AllocTransBuf(b_decl, 1, "Redundant");
	    else
		AllocTransBuf(b_decl, (int) BiggestOp, "Computed");
	}
    }

    DoDecl(top, Yes);
    mypf(top, "\n");
    CloseFile(whole);
} /* GenServer */
