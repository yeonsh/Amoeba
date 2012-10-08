/*	@(#)client.c	1.3	96/02/27 12:43:05 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

/*
 *	This file attempts to provide functions
 *	that generate the client stubs.
 *	The stubs will have the same name as the
 *	operator definition they were generated
 *	from.
 *	In general, a client stub contains:
 *	- #includes, definitions of the marshalers
 *	- The header of the stub.
 *	- Declaration of internal variables. Most
 *	  notably are the buffer and its pointer.
 *	  This section might involve calling malloc().
 *	- Code to marshal the input parameters.
 *	- Code to call trans().
 *	- Code to unmarshal the output parameters.
 */

/*
 *	This variable tells how we want our buffer today.
 *	Its possible values are the B_* macro's.
 */
static AllocWay;		/* How to get a buffer	*/

/*
 *	Some expressions:
 */
static struct etree *ReqAlloc, *ReplAlloc;	/* Sizes to buf-alloc	*/

/*
 *	Buffernames. They might be takes from the parameterlist
 */
/* Names to use, might be borrowed	*/
static char *ReqName, *ReplName;
/* Need '&' operator on this buffer?	*/
static Bool ReqAddr, ReplAddr;
/* The buffername I might generate	*/
static char *BufName = "_buf";

/*
 *	Arrange space for a buffer,
 *	using either malloc or alloca.
 */
/* static */ void
VarBuf(fp, way, scope)
    handle fp;
    char *way;			/* "malloc" or "alloca"			*/
    struct itlist *scope;
{
    int ReqCon, ReplCon;	/* -1: not constant, >= 0: constant size */
    assert(way != NULL);
    assert(ReqAlloc != NULL);
    assert(ReplAlloc != NULL);
    if (ReqAlloc->et_const) ReqCon = ReqAlloc->et_val;
    else ReqCon = -1;
    if (ReplAlloc->et_const) ReplCon = ReplAlloc->et_val;
    else ReplCon = -1;
    assert(!(ReqCon>0 && ReplCon>0));	/* At least 1 dynamic	*/

    /* Open a new block: */
    mypf(fp, "%n{%>");
    if (ReqCon<0) mypf(fp, "%nregister int in; /* Size for request */");
    if (ReplCon<0) mypf(fp, "%nregister int out; /* Size for reply */");
#if 0
    if (ReqCon < 0 && ReplCon < 0) mypf(fp, "%nint size; /* Maximum */");
#endif
    mypf(fp, "%nextern char *%s();", way);

    if (ReqCon<0) {				/* Compute req	*/
	mypf(fp, "%nin = ");
	ExprSpell(fp, ReqAlloc, 100, scope, No);
	mypf(fp, ";");
    }
    if (ReplCon<0) {				/* Compute repl	*/
	mypf(fp, "%nout = ");
	ExprSpell(fp, ReplAlloc, 100, scope, No);
	mypf(fp, ";");
    }

    mypf(fp, "%n_buf = %s(", way);		/* _buf = <way>(*/
    if (ReqCon>0) mypf(fp, "%d", ReqCon);	/* <in>		*/
    else mypf(fp, "in");
    mypf(fp, " > ");				/* >		*/
    if (ReplCon>0) mypf(fp, "%d", ReplCon);	/* <out>	*/
    else mypf(fp, "out");
    mypf(fp, " ? ");				/* ?		*/
    if (ReqCon>0) mypf(fp, "%d", ReqCon);	/* <in>		*/
    else mypf(fp, "in");
    mypf(fp, " : ");				/* :		*/
    if (ReplCon>0) mypf(fp, "%d", ReplCon);	/* <out>	*/
    else mypf(fp, "out");
    mypf(fp, ");%nif (_buf == 0) return AIL_CLN_MEM_BAD;%<%n}");
} /* VarBuf */

/*
 *	The client may want to pass one parameter directly to
 *	trans to avoid copying. Here we set the corresponding
 *	bit, and find out whether to use '&' operator or not.
 */
Bool
NoMarsh(list, name)
    struct itlist *list;
    char *name;
{
    struct typdesc *type;
    assert(list != NULL);
    while (list->il_name != name) {
	list = list->il_next;
    }
    list->il_bits |= AT_NOMARSH;
    for (type = list->il_type; type->td_kind == tk_tdef; type = type->td_prim)
	;
    switch (type->td_kind) {
    case tk_ref:
    case tk_arr:
    /* case tk_string: */
	return No;
    default:
	if (options & OPT_DEBUG) mypf(ERR_HANDLE, "NoMarsh: &: yes\n");
	return Yes;
    }
    /*NOTREACHED*/
} /* NoMarsh */

/*
 *	Return proper type to align the buffer for an operation
 */
char *
AlignType(op)
    struct optr *op;
{
    int tpin, tpout;
    tpin = op->op_in->td_align;
    tpout = op->op_out->td_align;
    if (tpin == 0 && tpout == 0) return "char";
    switch (tpout > tpin ? tpout : tpin) {
    case 1: return "char";
    case 2: return "int16";
    case 4: return "int32";
    case 8: return "double";
    default: assert(0);
    }
    /*NOTREACHED*/
} /* AlignType */

/*
 *	Declare - and possibly allocate - a buffer
 *	and some other variables for managing it.
 */
/* static */ void
DeclBuf(fp, op)
    handle fp;
    struct optr *op;
{
    struct itlist *args;
    int scratch;

    args = op->op_client;
/*	This must still be re-implemented	*/
    if (op->op_in->td_esize == NULL) {
mypf(ERR_HANDLE, "%r Actual in size for %s unknown; too bad\n", op->op_name);
	return;
    }
    if (op->op_out->td_esize == NULL) {
mypf(ERR_HANDLE, "%r Actual out size for %s unknown; too bad\n", op->op_name);
	return;
    }

#ifdef NOTDEF
    if (op->op_in->td_msize == NULL) {
mypf(ERR_HANDLE, "%r Max in size for %s unknown; too bad\n", op->op_name);
	return;
    }
    if (op->op_out->td_msize == NULL) {
mypf(ERR_HANDLE, "%r Max out size for %s unknown; too bad\n", op->op_name);
	return;
    }
#endif

    ReplAddr = ReqAddr = No;	/* Default: no & needed */
    /*
     *	For the reply message:
     */
    if (op->op_ob == 0) {	/* No out-parameters in the buffer	*/
	ReplName = NULL;
	ReplAlloc = Cleaf((long) 0);
    } else if (op->op_out->td_meml->il_next == NULL
	    && CharsOnly(op->op_out->td_meml->il_type)) {
	/* One out-parameter in the buffer; and it needs no marshaling	*/
	ReplName = op->op_out->td_meml->il_name;
	ReplAddr = NoMarsh(op->op_client, ReplName);
	ReplAlloc = Cleaf((long) 0);
    } else {
	/* Too bad, no obvious optimisations here	*/
	ReplName = BufName;		/* Use the declared buffer	*/
	/* Always use max for out, if known	*/
	ReplAlloc = op->op_out->td_msize;
	if (ReplAlloc == NULL) ReplAlloc = op->op_out->td_esize;
    }

    /*
     *	For the request message:
     */
    if (op->op_ib == 0) {	/* No in-parameters in the buffer	*/
	ReqName = NULL;
	ReqAlloc = Cleaf((long) 0);
    } else if (op->op_in->td_meml->il_next == NULL
	    && CharsOnly(op->op_in->td_meml->il_type)) {
	/* One in-parameter in the buffer; and it needs no marshaling	*/
	ReqName = op->op_in->td_meml->il_name;
	ReqAddr = NoMarsh(op->op_client, ReqName);
	ReqAlloc = Cleaf((long) 0);
    } else {
	/* Too bad, no optimisations possible	*/
	ReqName = BufName;		/* Use the declared buffer	*/
	if (ReplAlloc->et_const) {
	    /*
	     *	Replysize is constant; try to use a
	     *	constant requestsize as well
	     */
	    if (op->op_in->td_msize != NULL
	    && op->op_in->td_msize->et_const)	/* Max is constant	*/
		ReqAlloc = op->op_in->td_msize;
	    else
		ReqAlloc = op->op_in->td_esize;	/* Use the actual size for in */
	} else {
	    /* Replysize is dynamic; can as well use actual size for alloc */
	    ReqAlloc = op->op_in->td_esize;
	}
    }

    /*
     *	At this point ReplAlloc and ReqAlloc are zero (!= NULL!!) if
     *	the buffer can be "borrowed" from the caller, or there
     *	are no buffer parameters that travel the associated way.
     *	This is (and that's no coincidence) exactly the information
     *	that VarBuf() expects.
     *	Now it's time to determine the way we allocate a buffer.
     */
    AllocWay = B_NOBUF;			/* Be optimistic		*/
    if (!IsZero(ReqAlloc)) {		/* No buffer for request	*/
	if (ReqAlloc->et_const) AllocWay = B_STACK;
	else AllocWay = dynamic;	/* Either malloc or alloca	*/
    }
    if (!IsZero(ReplAlloc) && AllocWay != dynamic) {
	if (ReplAlloc->et_const) AllocWay = B_STACK;
	else AllocWay = dynamic;
    }

    /* Declare the most obvious: */
    mypf(fp, "%nheader _hdr;");
    if (lang == L_TRADITIONAL)	/* pcc & derivatives' signextension bug... */
	mypf(fp, "%nshort _ret;\t/* PCC signextension bug */");
    else
	mypf(fp, "%nint _ret;");

    /* Arrange a buffer, declare _adv	*/
    switch (AllocWay) {
    case B_NOBUF:
	/* No other declarations need to be made. */
	assert(IsZero(ReqAlloc));
	assert(IsZero(ReplAlloc));
	break;
    case B_STACK:
	/* Compute max(request, reply)	*/
	if (!IsZero(ReqAlloc)) {
	    assert(ReqAlloc->et_const);
	    scratch = ReqAlloc->et_val;
	} else {
	    scratch = 0;
	}
	if (!IsZero(ReplAlloc)) {
	    assert(ReplAlloc->et_const);
	    if (ReplAlloc->et_val > scratch) scratch = ReplAlloc->et_val;
	}
	mypf(fp, "%nunion { char _ail_buf[%d]; %s _ail_align; } _ail_buf;",
						scratch, AlignType(op));
	mypf(fp, "\n#define _buf _ail_buf._ail_buf");
	mypf(fp, "%nregister char *_adv;");
	break;
    case B_MALLOC:
	VarBuf(fp, "malloc", args);
	break;
    case B_ALLOCA:
	VarBuf(fp, "alloca", args);
	break;
    case B_ERROR:
	mypf(ERR_HANDLE, "%r no memory allocator defined\n");
	break;
    default:
	assert(0);
    }
    assert(ReplAlloc != NULL);
    assert(ReqAlloc != NULL);
} /* DeclBuf */

/*
 *	Assign op->op_client + additional analysis. Used to be HUGE!
 */
void
ClientAnalysis(op)
    struct optr *op;
{
    struct itlist *arg;
#if 0
    struct itlist *onlyrepl, *onlyreq;		/* Not used yet	*/
    onlyrepl = onlyreq = NULL;	/* Redundant -- for asserts	*/
#endif
    assert(op != NULL);
    if (op->op_flags & OP_CLIENT) return;	/* Done yet	*/
    op->op_flags |= OP_CLIENT;			/* Not again	*/
    op->op_client = CopyItList(op->op_args);
    if (op->op_flags & OP_NORPC) {		/* Easy		*/
	return;
    }
    /*
     *	Make a client-oriented version of the argument-list:
     */
    for (arg = op->op_client; arg != NULL; arg = arg->il_next) {
	struct typdesc *type;
	arg->il_bits &= ~AT_REDECL;	/* This bit is for the server	*/
	type = arg->il_type;
    }
} /* ClientAnalysis */

/*
 *	Generate the header of a procedure.
 *	This includes the declarations of the arguments.
 *	The order we use is the users', not the one calculated by DefOptr().
 *	Several styles are handled: prototypes versus traditional headers,
 *	and impl_ headers versus clientstubs. ServerStubs have such wierd
 *	headers that I wrote a separate routine for them (it's not a function).
 *	The ANSI-C header generator calls this to declare operators as well.
 *	No braces here, only parentheses.
 */
void
OptrHead(fp, op, client, comment_argdef)
    handle fp;
    struct optr *op;
    int client;
    /* For traditional headers - comment out the argument names */
    Bool comment_argdef;
{
    extern int Do_pass_arraysize;
    struct itlist *ap;
    char *acttname;	/* May point to typename for activate return value */
    int need_comma;
    struct itlist *ownbuf;

    if (comment_argdef) assert(lang == L_TRADITIONAL);
    assert(op);
    assert(op->op_name);
    ClientAnalysis(op);

    /*
     * On the server side, the arguments can be quite
     * different from the definition because of calling generators
     */
    if (!client) {
	ownbuf = DoesOwnRepl(op);
	if (GetFlags(op->op_val) & FL_PASSACT) {
	    if (ActType == NULL) {
		mypf(fp, "%r activate type unknown\n");
		return;
	    }
	    acttname = ActType;
	} else acttname = NULL;
    } else {
	ownbuf = NULL;
	acttname = NULL;
    }

    /* Find which argument is the lonely reply, if any	*/
    ownbuf = client ? NULL : DoesOwnRepl(op);

    /* Print header + first, special argument	*/
    mypf(fp, "%s%n%s%s(", client_stub_return_type,
	client ? "" : "impl_", op->op_name);
    if (comment_argdef) mypf(fp, " /* ");
    if (op->op_flags & OP_STARRED) {
	if (lang == L_ANSI) mypf(fp, client ? "capability *" : "header *");
	mypf(fp, client ? "_cap" : "_hdr");
	need_comma = Yes;
    } else {
	need_comma = No;
    }

    /* For servers, maybe the activate value is passed */
    if (acttname != NULL) {
	if (need_comma) mypf(fp, ", ");
	switch (Activate) {
	case AC_GEN:
	    switch (lang) {
	    case L_ANSI:
		mypf(fp, "%s _obj", acttname);
		break;
	    case L_TRADITIONAL:
		mypf(fp, "_obj");
		break;
	    default: abort();
	    }
	    break;
	default:
	    abort();
	}
	need_comma = Yes;
    }

    /* The other args, in the right order	*/
    if (op->op_order == NULL) {
	/*
	 *	BUG: this test may not find all cases
	 *	in which there are no parameters
	 */
	if (!need_comma && lang == L_ANSI)
	    mypf(fp, "void");		/* Really... */
    } else for (ap = op->op_order; ap != NULL; ap = ap->il_next) {
	register struct itlist *ra;
	if (need_comma) mypf(fp, ", ");
	else need_comma = Yes;
	/*
	 *	Is this the special argument?
	 *	We have to test the namefield, since
	 *	DoesOwnRepl searches the op_server list.
	 */
	if (ownbuf != NULL && ownbuf->il_name == ap->il_name) {
	    assert(!client);	/* ownbuf should be NULL then	*/
	    if (lang == L_ANSI)
		mypf(fp, "char **%s, long *_rep_sz", ap->il_name);
	    else mypf(fp, "%s, _rep_sz", ap->il_name);
	    continue;
	}
	for (ra = op->op_client; ra->il_name != ap->il_name; ra = ra->il_next)
	    ;
	if (lang == L_ANSI) {
	    mypf(fp, "\n    /* %s */\t", IoName(ra->il_bits));
	    TypeSpell(fp, ra->il_type->td_styp, No);
	    SoftSpell(fp, ra->il_type, ra->il_name);
	} else {
	    mypf(fp, "%s", ap->il_name);
	}
	if (!client && Do_pass_arraysize) {
	    /* If it is an array parameter, the size is passed to the server
	     * implementation routine as well.
	     */
	    struct typdesc *type;

	    /* First we have to get the real type using same code as
	     * in ServerCallImpl.
	     */
	    type = ra->il_type;
	    if (type->td_kind == tk_copy) type = type->td_prim;
	    if (type->td_kind == tk_ref) type = type->td_prim;
	    assert(type->td_kind != tk_ref && type->td_kind != tk_copy);
	    while (type->td_kind == tk_tdef) type = type->td_prim;
	    assert(type->td_kind != tk_ref && type->td_kind != tk_copy);

	    if (type->td_kind == tk_arr) {
		if (lang == L_ANSI) {
		    mypf(fp, ", int %s_maxsiz", ra->il_name);
		} else {
		    mypf(fp, ", %s_maxsiz", ra->il_name);
		    /* type is int by default */
		}
	    }
	}
    }
    if (comment_argdef) mypf(fp, " */ ");
    mypf(fp, ")%>");

    /* Declare the arguments if not done yet:	*/
    if (lang == L_TRADITIONAL) {
	struct itlist *patch;
	if (comment_argdef) {
	    mypf(fp, "; /* More baroque in later Ail releases */\n");
	} else {
	    if (op->op_flags & OP_STARRED)
		mypf(fp, client ? "%ncapability *_cap;" : "%nheader *_hdr;");

	    if (acttname != NULL) switch (Activate) {
	    case AC_GEN:
		mypf(fp, "%n%s _obj; /* %s return value */", acttname, ActFunc);
		break;
	    }

	    if (ownbuf != NULL) {
		/* Redeclare the changed argument: */
		mypf(fp, "%nchar **%s; long *_rep_sz; /* Impl_repl_buf */%n",
		    ownbuf->il_name);
		/* Patch a bit to suppress the declaration of ownbuf	*/
		for (patch = op->op_client; patch->il_name != ownbuf->il_name; ) {
		    patch = patch->il_next;
		    assert(patch != NULL);
		}
		assert((patch->il_bits & AT_NODECL) == 0);
		patch->il_bits |= AT_NODECL;
	    }
	    /* Declare arguments (don't care about the order)	*/
	    DeclList(fp, op->op_client);
	    if (ownbuf) {
		assert(patch != NULL);
		patch->il_bits &= ~AT_NODECL;	/* Reset NODECL-bit	*/
	    }
	}
    }
    mypf(fp, "%<");
} /* OptrHead */

static void
ClientBody(fp, op, flags)
    handle fp;
    struct optr *op;
    int flags;
{
    assert(op->op_flags & OP_CLIENT);
    assert(op->op_flags & OP_STARRED);
    /*
     *	Marshal the input-parameters
     */
    mypf(fp, "%n/* Marshaling code: */");
    Minit(ReqName);
    if (op->op_ih > 0)
	HeadMsh(fp, op->op_client, AT_REQUEST, 0);
    if (op->op_ib > 0) {
	Marshal(fp, AT_REQUEST, op, INT_CLIENT);
    }

    /*
     *	Do the trans:
     */
    mypf(fp, "%n/* Code to call trans */");
    mypf(fp, "%n_hdr.h_port = _cap->cap_port;");
    mypf(fp, "%n_hdr.h_priv = _cap->cap_priv;");
    mypf(fp, "%n_hdr.h_command = %d;", op->op_val);

    if (flags & FL_IDEMPOTENT)	/* Perhaps a loop around it	*/
	mypf(fp, "%nfor (_retry = %d; _retry-- && (", RetryCount + 1);
    else mypf(fp, "%n");

    mypf(fp, "_ret = %strans(&_hdr, %s%s, (bufsize) ",
	(lang == L_ANSI) ? "(short)" : "", /* ANSI C is value-preserving! */
	ReqAddr ? "(bufptr) &" : "",
	(ReqName == NULL) ? "(bufptr) 0" : ReqName);
    if (!ErrCnt)	/* td_esize might be NULL	*/
	ExprSpell(fp, op->op_in->td_esize, 100, op->op_client, No);

    /*
     *	Maybe I'd better pass a buffer if I got one anyway
     *	even if no reply is expected; this adds redundancy.
     */
    mypf(fp, ", &_hdr, %s%s, (bufsize) ",
	ReplAddr ? "(bufptr) &" : "",
	(ReplName == NULL) ? "(bufptr) 0" : ReplName);
    if (!ErrCnt)	/* td_msize might be NULL	*/
	ExprSpell(fp, op->op_out->td_msize, 100, op->op_client, No);

    if (flags & FL_IDEMPOTENT) mypf(fp, ")) == RPC_FAILURE; )%>%n;%<");
    else mypf(fp, ");");

    mypf(fp, "%nif (_ret < 0) {%>");
    mypf(fp, "%nMON_EVENT(\"%s RPC transaction failure\");", op->op_name);
    mypf(fp, "%ngoto _fout;%<%n}");

    /*
     *	Unmarshal output-parameters
     */
    Mreinit(ReplName);	/* Reset internal marshal state */
    if (op->op_oh > 0 || op->op_ob > 0) {	/* If there are out-parms */
	mypf(fp, "%nif (_hdr.h_status == 0) {%>");

	/*
	 *	Check output size. TO DO: make it bulletproof.
	 *	I wish trans() would cooperate, but it simply
	 *	truncates the reply-size if the buffer we pass
	 *	is too small. So, we only check if it's reasonable
	 *	to. I could also pass the buffer if I got one
	 *	anyway, but then I'd have to check that I didn't
	 *	borrow my buffer from the caller.
	 */
	if (op->op_out->td_esize->et_const) {
	    mypf(fp, "%nif (_ret != (");
	    ExprSpell(fp, op->op_out->td_esize, 0, op->op_client, Yes);
	    mypf(fp, ")) {%>");
	    mypf(fp, "%nMON_EVENT(\"%s RPC bad replysize\");", op->op_name);
	    mypf(fp, "%n_ret = AIL_CLN_OUT_BAD;");
	    mypf(fp, "%ngoto _fout;%<%n}");
	}

	if (op->op_oh > 0) HeadUnmsh(fp, op->op_client, AT_REPLY, 0);
	if (op->op_ob > 0) Unmarshal(fp, AT_REPLY, op, INT_CLIENT);

	mypf(fp, "%<%n} else MON_EVENT(\"%s RPC remote failure\");",op->op_name);
    }

    /*
     *	Program around signextension bug in PCC:
     */
    if (lang == L_TRADITIONAL) {
	mypf(fp, "%n_ret = _hdr.h_status; /* PCC bug... */");
    } else {
	mypf(fp, "%n_ret = (short) _hdr.h_status; /* Sign extend */");
    }

    /* Cleanup:	*/
    mypf(fp, "\n_fout:");
    if (AllocWay == B_MALLOC) {
	mypf(fp, "%nfree(_buf);");
    }
    mypf(fp, "%nreturn _ret;");
} /* ClientBody */

/*
 *	Generate client stub.
 */
void
ClientOptr(fp, op)
    handle fp;
    struct optr *op;
{
    handle top, decls;
    int flags;

    if (ErrCnt != 0) return;
    ThisOptr = op;	/* Errormessages find the name here	*/
    /* The flags might ask for additional code	*/
    flags = GetFlags(op->op_val);
    if (MonitorAll) flags |= FL_CLIENTMON;

    mypf(fp, "#include <%s>\n", general_header_file);
    if (flags & FL_CLIENTMON) mypf(fp, "#include <monitor.h>\n");
    else mypf(fp, "#define MON_EVENT(Off)\n");
    mypf(fp, "#include \"%s.h\"\n\n", ThisClass->cl_name);
    mypf(fp, "/*\n *\tClient stub for %s by ail\n */\n\n", op->op_name);

    top = BackPatch(fp);
    OptrHead(fp, op, Yes, No);
    mypf(fp, "%n{%>");
    decls = BackPatch(fp);
    if (flags & FL_IDEMPOTENT) mypf(fp, "%nint _retry;");
    DeclBuf(fp, op);		/* All local declarations - except _pcc	*/
    if (flags & FL_CLIENTMON) mypf(fp, "%nMON_EVENT(\"%s RPC\");", op->op_name);
    if (AllocWay == B_ERROR) return;
    if (op->op_ailword != NULL) {
	mypf(fp, "%n_hdr.%s = _ailword;", op->op_ailword);
    }
    ClientBody(fp, op, flags);
    if (MarshState.ms_shcast) {
	MarshState.ms_shcast = No;
	mypf(decls, "%nshort _pcc;");
    }
    DoDecl(top, No);
    mypf(fp, "%<%n} /* %s */\n", op->op_name);
    ThisOptr = NULL;
} /* ClientOptr */
