/*	@(#)unmsh.c	1.3	94/04/07 14:39:33 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
/*
 *	This file contains the functions that
 *	generate code that calls unmarshalers.
 *	It calls some code from msh.c
 */

static int vartmp;	/* Used to create unique scratch variables	*/
static Bool is_svr;	/* Working for the server?			*/
#define	resettmp()	vartmp = 0
#define	nexttmp()	++vartmp

/*
 *	Unmarshal an array, don't care about whether it's open or not
 *	Try using memcpy whenever possible.
 *	We don't have to start getting the alignment right, since
 *	our caller (unmsh()) took care of this yet.
 */
void
arrunmsh(fp, name, upb, type, mode, algp)
    handle fp;		/* The file to write on	*/
    char *name;		/* What we unmarshal	*/
    struct etree *upb;	/* upperbound		*/
    struct typdesc *type;
    int mode;
    int *algp;		/* Alignment		*/
{
    char newname[200], ind[20], max[20];
    int old_align;
    old_align = *algp;
    switch (type->td_kind) {
    case tk_char:
	MustDeclMemCpy();
	mypf(fp, "%n(void) memcpy(%s, _adv, ", name);
	ExprSpell(fp, upb, 100, MarshState.ms_scope, Yes);
	mypf(fp, "); _adv += ");
	ExprSpell(fp, upb, 100, MarshState.ms_scope, Yes);
	mypf(fp, ";");
	++*algp;
	break;
    case tk_int:
	if ((type->td_size == 1 || type->td_size == -1)
	&& (mode == INT_GOOD || mode == INT_CLIENT)) {
	    MustDeclMemCpy();
	    mypf(fp,"%n(void) memcpy((char *) %s, _adv, sizeof(%s) * (",
		name, (type->td_size == 1) ? "long" : "short");
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, Yes);
	    mypf(fp, ")); _adv += sizeof(%s) * ",
		(type->td_size == 1) ? "long" : "short");
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, Yes);
	    mypf(fp, ";");
	    *algp += type->td_size == 1 ? 4 : 2;
	    break;	/* Out of switch */
	}
	/* Else: FALLTHROUGH */
    default:
	/*
	 *	Can't use memcpy for some reason; loop
	 */
	sprintf(ind, "_ind%d", vartmp); sprintf(max, "_max%d", vartmp);
	nexttmp();
	sprintf(newname, "%s[%s]", name, ind);
	/* Open blocks	*/
	if (upb->et_const) {
	    mypf(fp, "%n{%> register int %s;", ind);			/*}*/
	    mypf(fp, "%nfor (%s = 0; %s < %d; ++%s) {%>", ind, ind,	/*}*/
		upb->et_val, ind);
	} else {
	    mypf(fp, "%n{%> register int %s, %s;", ind, max);		/*}*/
	    mypf(fp, "%n%s = ", max);
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, Yes);
	    mypf(fp, ";%nfor (%s = 0; %s < %s; ++%s) {%>", ind, ind,	/*}*/
		max, ind);
	}
	/* Do it	*/
	unmsh(fp, newname, type, mode, algp);
	/* Close the block {{	*/
	mypf(fp, "%<%n}%<%n}");
    }

    /* Adjust alignment:        */
    if (upb->et_const) {        /* We know it exactly   */
	*algp += (*algp - old_align) * upb->et_val;
    } else {
	*algp = type->td_align; /* Pretend we marshaled ONE     */
    }
} /* arrunmsh */

void
unmsh(fp, name, type, mode, algp)
    handle fp;		/* The file to write on		*/
    char *name;		/* ASCII			*/
    struct typdesc *type;
    int mode;		/* INT_GOOD, INT_BAD, INT_CLIENT	*/
    int *algp;
{
    char newname[200];	/* BUG: not checked against overflow	*/
    char *u_func;
    /*
     *	Find either an unmarshaler-definition or a non-typedef
     */
    assert(name != NULL);
    assert(type != NULL);
    type = SameType(type);
    TouchType(type);		/* Resolve standard types	*/
    if (MarshState.ms_bufnm != NULL) {
	/*
	 *	First time to touch the buffer; set adv.
	 *	The server does this itself.
	 */
	mypf(fp, "%n_adv = %s;", MarshState.ms_bufnm);
	MarshState.ms_bufnm = NULL;	/* Don't do this again	*/
    }
    /*
     *	Handle alignment
     */
    if (*algp < 0) {		/* Unknown alignment	*/
	Align(fp, type->td_align);
	*algp = type->td_align;
    } else if (type->td_align <= 0) {
	mypf(ERR_HANDLE, "%r Don't know how to align \"");
	TypeSpell(ERR_HANDLE, type->td_styp, No);
	SoftSpell(ERR_HANDLE, type, name);
	mypf(ERR_HANDLE, "\"\n");
    } else if (*algp % type->td_align != 0) {
#if 0
        int diff;
        
        diff = type->td_align - (*algp % type->td_align);
        *algp += diff;
	mypf(fp, "%n_adv += %d; /* Align */",
	    diff);
#endif
	Align(fp, type->td_align);
	*algp = type->td_align;
    }

    /*
     *	Do we know how to unmarshal this type?
     */
    if (type->td_marsh != NULL) {
	u_func = (mode == INT_CLIENT) ?
	    type->td_marsh->m_clu : type->td_marsh->m_svu;
    } else u_func = NULL;

    if (u_func != NULL) {
	MustDecl(type->td_marsh);
	mypf(fp, "%n_adv = %s(%s, ",
	    u_func, (MarshState.ms_bufnm == NULL) ? "_adv" : MarshState.ms_bufnm);
	MarshState.ms_bufnm = NULL;	/* Make shure is *never* used again	*/
	dbg2("Prefixing %s\n", name);
	if (type->td_kind != tk_ref && type->td_kind != tk_arr)
	    mypf(fp, "&");	/* Dereference	*/
	mypf(fp, "%s", name);
	/* Pass integer info?	*/
	if ((type->td_bits & TD_CONTINT) && mode != INT_CLIENT)
	    mypf(fp, mode == INT_GOOD ? ", 1" : ", 0");
	mypf(fp, ");");
	if (mode == INT_CLIENT)
	    mypf(fp, "%nif (_adv == 0) return AIL_CLN_OUT_BAD;");
	else
mypf(fp, "%nif (_adv == 0) {%>%n_ret = AIL_SVR_IN_BAD;%nbreak;%<%n}");
/*	*algp = -1;	Don't trust everything?	*/
    } else switch (type->td_kind) {

    case tk_char: {
	++*algp;	/* This takes one byte	*/
	mypf(fp, "%n%s = *_adv++;", name);
	break;
    }

    case tk_int: {
	if (type->td_size < 0) {	/* Short		*/
	    *algp += 2;			/* A short is two bytes	*/
	    switch (mode) {
	    case INT_GOOD:
	    case INT_CLIENT:
		mypf(fp, "%n%s = *(short *)_adv; _adv += 2;", name);
		break;
	    case INT_BAD:
		mypf(fp, "%n%s = (*(short *)_adv << 8) + ", name);
		mypf(fp, "((*(short *) _adv >> 8) & 0xff); _adv += 2;");
		break;
	    }
	} else {			/* Long or int		*/
	    *algp += 4;			/* int==long is 4 bytes	*/
	    switch (mode) {
	    case INT_GOOD:
	    case INT_CLIENT:
		mypf(fp, "%n%s = *(long *)_adv; _adv += 4;", name);
		break;
	    case INT_BAD:
		mypf(fp, "%n{%> register long _tmp;");
		mypf(fp, "%n_tmp = *(long *)_adv; _adv += 4;");
		mypf(fp, "%n%s = (_tmp>>24)&0xff | (_tmp>>8)&0xff00 |", name);
		mypf(fp, "%>%n(_tmp<<8)&0xff0000 | (_tmp<<24)&0xff000000;%<");
		mypf(fp, "%<%n}");
		break;
	    default:
		assert(0);
	    }
	}
	break;
    }

    case tk_arr: {
	assert(type->td_act != NULL);	/* Actual size	*/
	if (type->td_bound->et_const && !type->td_act->et_const) {
	    /* Verify actual upperbound	*/
	    mypf(fp, "%nif (");
	    ExprSpell(fp, Diadic('>', type->td_act, type->td_bound),
		100, MarshState.ms_scope, Yes);
	    mypf(fp, ") {%>");
	    Fail(fp, is_svr, is_svr ? AT_REQUEST : AT_REPLY);
	    mypf(fp, "%<%n}");
	}
	arrunmsh(fp, name, type->td_act, type->td_prim, mode, algp);
	break;
    }

    case tk_struct: {
	struct itlist *mp;	/* Member-pointer	*/
	int old_align;
	old_align = *algp;
	for (mp = type->td_meml; mp != NULL; mp = mp->il_next) {
	    sprintf(newname, "%s.%s", name, mp->il_name);
	    unmsh(fp, newname, mp->il_type, mode, algp);
	}
	/* Now at least at right alignment for this kinda struct: */
	if (type->td_esize->et_const)
	    *algp = old_align + type->td_esize->et_val;
	else *algp = type->td_align;
	break;
    }

    case tk_ref: {
	struct typdesc *prim;
	/* Forget irrelevant typedefs	*/
	prim = SameType(type->td_prim);
	/*
	 *	This if-statement isn't really necessary.
	 *	The code that would be generated by the
	 *	else-clause only looks a bit wierd:
	 *		_adv = u_func(_adv, (*struct_ptr).member);
	 *	instead of
	 *		_adv = u_func(_adv, struct_ptr->member);
	 */
	if (prim->td_kind == tk_struct) {
	    struct itlist *mp;	/* Member-pointer	*/
	    for (mp = prim->td_meml; mp != NULL; mp = mp->il_next) {
		sprintf(newname, "%s->%s", name, mp->il_name);
		unmsh(fp, newname, mp->il_type, mode, algp);
	    }
	} else {
	    /* BUG: tdef's referring to arrays too	*/
	    if (prim->td_kind == tk_arr) {
		unmsh(fp, name, prim, mode, algp);
	    } else {
		sprintf(newname, "(*%s)", name);
		unmsh(fp, newname, prim, mode, algp);
	    }
	}
	break;
    }

    case tk_copy:
	/* Copies are marshaled just like whatever they're a copy of. */
	unmsh(fp, name, type->td_prim, mode, algp);
	break;

    case tk_tdef: {
	if (u_func == NULL) {
	    unmsh(fp, name, type->td_prim, mode, algp);
	    break;
	} /* Else: fall through to report a proper error	*/
    }

    case tk_enum: {
	char newname[200];
	sprintf(newname, "(enum %s) %s", type->td_name, name);
	unmsh(fp, newname, ModifTyp(tk_int, 1, 0, 1), mode, algp);
	break;
    }

    default:
	mypf(ERR_HANDLE, "%r Don't know how to unmarshal \"");
	TypeSpell(ERR_HANDLE, type->td_styp, No);
	SoftSpell(ERR_HANDLE, type, name);
	if (ThisOptr != NULL)
	    mypf(ERR_HANDLE, "\" of %s\n", ThisOptr->op_name);
	else mypf(ERR_HANDLE, "\"\n");
    }
} /* unmsh */

/*
 *	"Unmarshal" the values that travel in the header
 *	No byteorder problems, but we may need to cast stuff.
 *	BUG: for ANSI, the latter is done using knowledge
 *	on the types of headerfields that isn't tested for.
 *	I.e. work to to when we move to Amoeba 4...
 */
void
HeadUnmsh(fp, args, on, off)
    handle fp;
    struct itlist *args;
    int on, off;
{
    for (; args != NULL; args = args->il_next) {
	if (args->il_attr->at_hmem != NULL && /* In the header	*/
	  BitsSetNotset(args->il_bits, on, off)) {
	    /* Real type and what it is used for:	*/
	    struct typdesc *rt, *ut;
	    /*
	     *	Exception for capabilities:
	     */
	    if (args->il_attr->at_hmem == pseudofield) {
		assert(on & AT_REPLY);	/* Because the header's type */
		mypf(fp, "%n%s->cap_port = _hdr.h_port;", args->il_name);
		mypf(fp, "%s->cap_priv = _hdr.h_priv;", args->il_name);
		continue;
	    }

	    /*
	     *	Some fields in the header are unsigned shorts.
	     *	We got a problem when a signed int gets put in
	     *	one of these.
	     *	With PCC, this triggers a signextension bug, which
	     *	can only be worked around by FIRST assigning to a
	     *	signed short, and then to whatever you want your value.
	     *	Since this compiler is to be found all over the world,
	     *	I 'd better program around it.
	     *	When compiling for ANSI, a cast to short does the trick.
	     *
	     *	Enumerated types can be in the header as well and need a cast.
	     */

	    for (rt = AmType(args->il_attr->at_hmem);
		rt->td_kind == tk_copy ||
		rt->td_kind == tk_ref ||
		rt->td_kind == tk_tdef;
		rt = SameType(rt->td_styp));
		    ;

	    for (ut = SameType(args->il_type);
		ut->td_kind == tk_copy ||
		ut->td_kind == tk_ref ||
		ut->td_kind == tk_tdef;
		ut = SameType(ut->td_styp));
		    ;

	    if (on & AT_REPLY) {
		/* We're client, so we must dereference	*/
		if (rt == ut) {	/* No type problems	*/
		    mypf(fp, "%n*%s = _hdr.%s;",
			args->il_name, args->il_attr->at_hmem);
		} else if (rt->td_kind == tk_enum) {
		    assert(args->il_type->td_name != NULL);
		    mypf(fp, "%n*%s = (enum %s) _hdr.%s;", args->il_name,
			args->il_type->td_name, args->il_attr->at_hmem);
		} else if (rt->td_kind == tk_int) {
		/* Was: } else if (ut->td_kind == tk_int) { */
#if 0	/* Do NOT delete - this code will come back */
		    if (lang == L_TRADITIONAL) {
			/* S.E. bug in PCC */
			MarshState.ms_shcast = Yes; /* Must declare _pcc */
			mypf(fp, "%n_pcc = _hdr.%s;", args->il_attr->at_hmem);
			mypf(fp, "%n*%s = _pcc;", args->il_name);
		    } else {
			/* Casting alone is enough	*/
			mypf(fp, "%n*%s = (short) _hdr.%s;",
			    args->il_name, args->il_attr->at_hmem);
		    }
#endif
		    int did_cast = 0;

		    mypf(fp, "%n*%s = ", args->il_name);
		    /* Cast to an int of the right sign and the wrong size */
		    if (rt->td_sign != ut->td_sign && ut->td_sign > 0) {
			mypf(fp, "(unsigned ");
			did_cast = 1;
		    }
		    if (rt->td_size != ut->td_size) {
			if (!did_cast) mypf(fp, "(");
			mypf(fp, rt->td_size < 0 ? "short" : "long");
			did_cast = 1;
		    }
		    if (did_cast) mypf(fp, ")");
		    mypf(fp, " _hdr.%s;", args->il_attr->at_hmem);
		} else { /* Not an int, not an enum, not equal !? */
		    mypf(ERR_HANDLE, "%r (internal) client-header-unmarsh\n");
		    mypf(ERR_HANDLE, "field=%s, arg=%s, type %d\n",
			args->il_attr->at_hmem, args->il_name, rt->td_kind);
		    abort();
		}
	    } else {
		assert(on & AT_REQUEST);
		/* The server cannot have to dereference	*/
		if (ut == rt) { /* No type problems	*/
		    mypf(fp, "%n%s = _hdr.%s;",
			args->il_name, args->il_attr->at_hmem);
		} else if (ut->td_kind == tk_enum) {
		    assert(args->il_type->td_name != NULL);
		    mypf(fp, "%n%s = (enum %s) _hdr.%s;", args->il_name,
			args->il_type->td_name, args->il_attr->at_hmem);
		} else if (ut->td_kind == tk_int) {
		    int did_cast = 0;

		    mypf(fp, "%n%s = ", args->il_name);
		    /* Cast to an int of the right sign and the wrong size */
		    if (rt->td_sign != ut->td_sign && ut->td_sign > 0) {
			mypf(fp, "(unsigned ");
			did_cast = 1;
		    }
		    if (rt->td_size != ut->td_size) {
			if (!did_cast) mypf(fp, "(");
			mypf(fp, rt->td_size < 0 ? "short" : "long");
			did_cast = 1;
		    }
		    if (did_cast) mypf(fp, ")");
		    mypf(fp, " _hdr.%s;", args->il_attr->at_hmem);
		} else {
		    fprintf(stderr, "ut->td_kind = %d\n", rt->td_kind);
		    abort();
		}
	    }
	}
    }
} /* HeadUnmsh	*/

/*
 *	This function writes the code to
 *	unmarshal parameters in stubs/unmarshalers.
 */
void
Unmarshal(fp, direction, op, mode)
    handle fp;		/* The file to write	*/
    int direction;
    struct optr *op;
    int mode;		/* How are the integers today?	*/
{
    struct itlist *arg;
    struct itlist *scope;
    int align = 0;	/* Assume initial alignment	*/
    MarshState.ms_scope = scope =
	(direction == AT_REQUEST) ? op->op_server : op->op_client;
    is_svr = (direction == AT_REQUEST);
    resettmp();
    for (arg = scope; arg != NULL; arg = arg->il_next) {
	assert(arg->il_attr != NULL);
	assert(arg->il_type);
	/*
	 * Sometimes arguments need not be unmarshaled.
	 */
	if ((arg->il_bits & AT_NOMARSH) || !(arg->il_bits & direction))
	    continue;
	/*
	 * The server thinks of copying a h_<any-field> value
	 * to a redecl'd variable as unmarshaling.
	 */
	if (arg->il_bits & AT_REDECL) {
	    assert(arg->il_attr->at_hmem != NULL);
	    assert(direction == AT_REQUEST);
	    continue;
	}

	if (arg->il_attr->at_hmem == NULL) {
	    if ((options & OPT_DEBUG) && MarshState.ms_offset >= 0)
		mypf(fp, "%n/* Offset now %d */", MarshState.ms_offset);
	    unmsh(fp, arg->il_name, arg->il_type, mode, &align);
	    Moffset(arg->il_type->td_esize);
	}
    }
} /* Unmarshal */
