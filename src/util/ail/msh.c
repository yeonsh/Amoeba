/*	@(#)msh.c	1.2	94/04/07 14:34:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
/*
 *	This file contains the functions that
 *	generate code that calls marshalers,
 *	plus some code that is shared between
 *	unmsh.c and msh.c
 */

/*
 *	Called just before the name of a parameter is emitted
 *	when calling marshalers. Emits a '*' if needed.
 */
void
Prefix(fp, type)
    handle fp;
    struct typdesc *type;
{
    int gptr;	/* Got a pointer?	*/
    /* Find type, don't forget to follow typedef-primitives. */
    if ((gptr = type->td_kind == tk_ref) != 0) type = type->td_prim;
    assert(type->td_kind != tk_ref);
    while (type->td_kind == tk_tdef) type = type->td_prim;
    /*
     * Because of C's funny rules for arrays, no prefix is ever needed.
     */
    if (type->td_kind == tk_arr) return;
    gptr |= (type->td_bits & TD_CPREF);	/* Got pointer because cc thinks so */
    dbg2("Arg: got %spointer\n", gptr ? "" : "no ");
    if (gptr) mypf(fp, "*");		/* Got pointer; dereference	*/
} /* Prefix */

/*
 *	Return most restrictive alignment of two
 *	This is the best one to remember as current alignment
 *	Note that some information is lost here
 */
int
bestalign(a1, a2)
    int a1, a2;
{
    int i;
    for (i = MAX_ALIGN; i > 0; i /= 2) { /* Try all possible alignments */
	if (a1 % i == 0 || a2 % i == 0) return i;
    }
    assert(0);
    /*NOTREACHED*/
} /* bestalign */

static int vartmp;	/* Used to create unique scratch variables	*/
#define	resettmp()	vartmp = 0
#define	nexttmp()	++vartmp

/*
 *	Marshal an array of type, no matter whether it's open or not
 *	Try to use memcpy wherever possible; update *algp.
 *	We don't have to start getting the alignment right, since
 *	our caller (msh()) took care of this yet.
 */
void
arrmsh(fp, name, upb, type, mode, algp)
    handle fp;			/* The file to write on	*/
    char *name;			/* What we marshal	*/
    struct etree *upb;		/* upperbound		*/
    struct typdesc *type;
    int mode;
    int *algp;
{
    char newname[200], ind[20], max[20];
    int old_align;

    old_align = *algp;
    switch (type->td_kind) {
    case tk_char:
	MustDeclMemCpy();
	mypf(fp, "%n(void) memcpy(_adv, %s, ", name);
	ExprSpell(fp, upb, 100, MarshState.ms_scope, No);
	mypf(fp, "); _adv += ");
	ExprSpell(fp, upb, 100, MarshState.ms_scope, No);
	mypf(fp, ";");
	++*algp;
	break;
    case tk_int:
	/* Don't do this for plain int's: you don't know their size	*/
	if ((type->td_size == 1 || type->td_size == -1)
	&& (mode == INT_GOOD || mode == INT_CLIENT)) {
	    MustDeclMemCpy();
	    mypf(fp,"%n(void) memcpy(_adv, (char *) %s, sizeof(%s) * (",
		name, (type->td_size == 1) ? "long" : "short");
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, No);
	    mypf(fp, ")); _adv += sizeof(%s) * ",
		(type->td_size == 1) ? "long" : "short");
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, No);
	    mypf(fp, ";");
#if 0
	    *algp = -1;
	    return;
#else
	    /*XXX Jelke: fix alignment bug */
	    *algp += type->td_size == 1 ? 4 : 2;
	    break;	/* Out of switch */
#endif
	}
	/* Else: FALLTHROUGH	*/
    default:
	/*
	 *	No memcpy; create a loop.
	 *	This creates two nested blocks:
	 *	one for declarations, one for for.
	 *	Because msh() is called, *algp will be updated.
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
	    ExprSpell(fp, upb, 100, MarshState.ms_scope, No);
	    mypf(fp, ";%nfor (%s = 0; %s < %s; ++%s) {%>", ind, ind,	/*}*/
		max, ind);
	}
	/* Do it	*/
	msh(fp, newname, type, mode, algp);
	/* Close the blocks {{	*/
	mypf(fp, "%<%n}%<%n}");
    }

    /* Adjust alignment - we know somewhat more then the called msh():	*/
    if (upb->et_const) {	/* We know it exactly	*/
	*algp += (*algp - old_align) * upb->et_val;
    } else {
	*algp = type->td_align; /* Pretend we marshaled ONE	*/
    }
#if 0
    mypf(fp, " /* tk_arr: align %d => %d */", old_align, *algp);
#endif
} /* arrmsh */

/*
 *	Generate inline marshaling code.
 *	Also used to write the body of a marshaler.
 *	The only thing we know about the thing
 *	we marshal is its type so far, and an
 *	ASCII representation of the object we
 *	read from, containing the appropiate
 *	indices, struct-field-selectors, and
 *	what thies mere sigh.
 */
void
msh(fp, name, type, mode, algp)
    handle fp;		/* The file to write on		*/
    char *name;		/* ASCII			*/
    struct typdesc *type;
    int mode;
    int *algp;
{
    char newname[200];	/* BUG: not checked against overflow	*/
    char *m_func;

    /*
     *	Find either a marshaler-definition or a non-typedef
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
    } else if ((*algp % type->td_align) != 0) {
	Align(fp, type->td_align);
	*algp = type->td_align;
    }

    /*
     *	Do we know how to marshal this type?
     */
    if (type->td_marsh != NULL) {
	m_func = (mode == INT_CLIENT) ?
	    type->td_marsh->m_clm : type->td_marsh->m_svm;
    } else m_func = NULL;

    if (m_func != NULL) {	/* Call the defined marshaler	*/
	if (options & OPT_DEBUG) {
	    mypf(OUT_HANDLE, "I know how to marshal ");
	    TypeSpell(OUT_HANDLE, type->td_styp, No);
	    SoftSpell(OUT_HANDLE, type, name);
	    mypf(OUT_HANDLE, "\"\n");
	}
	MustDecl(type->td_marsh);
	mypf(fp, "%n_adv = %s(%s, ",
	    m_func, (MarshState.ms_bufnm == NULL) ?
		"_adv" : MarshState.ms_bufnm);
	MarshState.ms_bufnm = NULL;	/* Never use it again	*/
	dbg2("Prefixing %s\n", name);
	Prefix(fp, type);
	mypf(fp, "%s", name);
	if ((type->td_bits & TD_CONTINT) && mode!= INT_CLIENT)
	    mypf(fp, (mode == INT_GOOD) ? ", 1" : ", 0");
	mypf(fp, ");");
	if (mode == INT_CLIENT) {
	    mypf(fp, "%nif (_adv == 0) return AIL_CLN_IN_BAD;");
	} else {
	    mypf(fp, "%nif (_adv == 0) {%>%n");
	    mypf(fp, "_ret = AIL_SVR_OUT_BAD;%nbreak;%<%n}");
	}
    } else switch (type->td_kind) {

    /************************************************************\
     *								*
     *		Machine types should set *algp			*
     *								*
    \************************************************************/

    case tk_char: {
	++*algp;	/* This takes one byte	*/
	mypf(fp, "%n*_adv++ = %s;", name);
	break;
    }

    case tk_int: {			/* Should check signs?	*/
	if (type->td_size < 0) {	/* Short		*/
	    *algp += 2;			/* The size of a short	*/
	    switch (mode) {
	    case INT_GOOD:
	    case INT_CLIENT:
		mypf(fp, "%n*(short *)_adv = %s; _adv += 2;", name);
		break;
	    case INT_BAD:
		mypf(fp, "%n*(short *) _adv = (0xff & (%s>>8)) + ", name);
		mypf(fp, "(%s << 8); _adv += 2;", name);
		break;
	    default:
		assert(0);
	    }
	} else {			/* Int or long; the same to ail	*/
	    *algp += 4;
	    switch (mode) {
	    case INT_GOOD:
	    case INT_CLIENT:
		mypf(fp, "%n*(long *)_adv = %s; _adv += 4;", name);
		break;
	    case INT_BAD:
		mypf(fp, "%n{%> register long _tmp;%n_tmp = %s;", name);
		mypf(fp, "%n_tmp = (_tmp>>24)&0xff | (_tmp>>8)&0xff00 |");
		mypf(fp, "%>%n(_tmp<<8)&0xff0000 | (_tmp<<24)&0xff000000;%<");
		mypf(fp, "%n*(long *)_adv = _tmp; _adv += 4;%<%n}");
		break;
	    default:
		assert(0);
	    }
	}
        break;
    }

    case tk_arr: {
	assert(type->td_act != NULL);
	arrmsh(fp, name, type->td_act, type->td_prim, mode, algp);
	break;
    }

    case tk_struct: {
	struct itlist *mp;	/* Member-pointer	*/
	int old_align;		/* We know more		*/
	old_align = *algp;
	for (mp = type->td_meml; mp != NULL; mp = mp->il_next) {
	    sprintf(newname, "%s.%s", name, mp->il_name);
	    msh(fp, newname, mp->il_type, mode, algp);
	}
	/* Now we are at least at right alignment for this kinda struct: */
	if (type->td_esize->et_const)	/* Struct of constant size	*/
	    *algp = old_align + (int) type->td_esize->et_val;
	else *algp = type->td_align;
#if 0
	mypf(fp, "%n/* tk_struct: alignment %d => %d */", old_align, *algp);
#endif
	break;
    }

    case tk_ref: {
	struct typdesc *prim;
	prim = SameType(type->td_prim);
	/*
	 *	This if-statement isn't really necessary.
	 *	The code that would be generated by the
	 *	else-clause just looks a bit wierd:
	 *		_adv = u_func(_adv, (*struct_ptr).member);
	 *	instead of
	 *		_adv = u_func(_adv, struct_ptr->member);
	 */
	if (prim->td_kind == tk_struct) {
	    struct itlist *mp;	/* Member-pointer	*/
	    for (mp = prim->td_meml; mp != NULL; mp = mp->il_next) {
		sprintf(newname, "%s->%s", name, mp->il_name);
		msh(fp, newname, mp->il_type, mode, algp);
	    }
	} else {
	    /* BUG: tdef's referring to arrays too	*/
	    if (prim->td_kind == tk_arr) {
		msh(fp, name, prim, mode, algp);
	    } else {
		sprintf(newname, "(*%s)", name);
		msh(fp, newname, prim, mode, algp);
	    }
	}
	break;
    }

    case tk_copy: {
	if (type->td_prim->td_kind == tk_ref)
	    msh(fp, name, type->td_prim->td_prim, mode, algp);
	else
	    msh(fp, name, type->td_prim, mode, algp);
	break;
    }

    case tk_tdef: {
	if (m_func == NULL) {
	    /*
	     *	Maybe the servermarshaler was supplied, but the client not
	     */
	    msh(fp, name, type->td_prim, mode, algp);
	    break;
	} /* Else: give up by falling in default	*/
    }

    case tk_enum: {
	char newname[200];
	/* Cast: */
	sprintf(newname, "(long) %s", name);
	msh(fp, newname, ModifTyp(tk_int, 1, 0, 1), mode, algp);
	break;
    }

    default:
	mypf(ERR_HANDLE, "%r Can't marshal \"");
	TypeSpell(ERR_HANDLE, type->td_styp, No);
	SoftSpell(ERR_HANDLE, type, name);
	if (ThisOptr != NULL)	/* Marshaling for some operator?	*/
	    mypf(ERR_HANDLE, "\" of %s\n", ThisOptr->op_name);
	else mypf(ERR_HANDLE, "\"\n");
    }
} /* msh */

/*
 *	Marshal whatever is in the header.
 */
void
HeadMsh(fp, args, on, off)
    handle fp;		/* Which file			*/
    struct itlist *args;/* Which ones			*/
    int on, off;	/* Bits that must be on or off	*/
{
    int n = 0;
    for (; args != NULL; args = args->il_next)
	if (args->il_attr->at_hmem != NULL && /* In the header      */
	    BitsSetNotset(args->il_bits, on, off)) {
		++n;
		/* Exception for the capability:	*/
		if (args->il_attr->at_hmem == pseudofield) {
		    assert(on & AT_REPLY);	/* Must be the server	*/
		    /* We don't need to emit parens:	*/
		    assert(args->il_bits & AT_REDECL);
		    mypf(fp, "%n_hdr.h_port = ");
		    Prefix(fp, args->il_type);
		    mypf(fp, "%s.cap_port; _hdr.h_priv = ", args->il_name);
		    Prefix(fp, args->il_type);
		    mypf(fp, "%s.cap_priv;", args->il_name);
		} else {
		    struct typdesc *rt;	/* Real type we marshal		*/
		    rt = SameType(args->il_type);
		    while (rt->td_kind == tk_copy || rt->td_kind == tk_ref)
			rt = rt->td_prim;
		    rt = SameType(rt);
		    mypf(fp, "%n_hdr.%s = ", args->il_attr->at_hmem);
		    /* Is this always necessary? */
		    Cast(fp, AmType(args->il_attr->at_hmem));
		    Prefix(fp, args->il_type);
		    mypf(fp, "%s;", args->il_name);
		}
	    }
    if (n == 0) mypf(fp, "%n/* Nothing in the header */");
} /* HeadMsh */

/*
 *	This function writes the code to marshal parameters in stubs.
 */
void
Marshal(fp, direction, op, mode)
    handle fp;		/* The file to write on		*/
    int direction;	/* AT_REQUEST or AT_REPLY	*/
    struct optr *op;	/* Marshal which?		*/
    int mode;		/* Client? Good or Bad endian?	*/
{
    struct itlist *scope;
    struct itlist *arg;	/* Arguments of the stub	*/
    int align = 0;	/* Assume initial alignment	*/
    MarshState.ms_scope = scope =
	direction == AT_REQUEST ? op->op_client : op->op_server;
    resettmp();
    for (arg = scope; arg != NULL; arg = arg->il_next) {
	assert(arg->il_attr != NULL);
	/*
	 * Don't do anything if it shouldn't be marshaled.
	 */
	if (!(arg->il_bits & direction) || arg->il_attr->at_hmem != NULL)
	    continue;
	if (arg->il_bits & AT_ADVADV) {
	    struct typdesc *type = arg->il_type;

	    /* Advance the advance pointer */
	    assert(direction == AT_REPLY);	/* Only for the server	*/
	    /*
	    **	If the argument contains an integer we maybe need 
	    **	to byte swap the buffer.
	    */
	    if ((type->td_bits & TD_CONTINT) && (mode == INT_BAD)) {
#if 0
		/*
		** This doesnt work, see comments in routine
		** CharsOrIntsOnly.
		*/
		msh(fp, "_buf", type, INT_BAD, &align);
		continue;
#else
		/*
		**	The msh function can not handle byte swapping
		**	in the buffer so do not use the buffer if it is
		**	not a character array.
		*/
		assert(0);
#endif
	    }
	    if (MarshState.ms_bufnm == NULL) {
		mypf(fp, "%n_adv += ");
	    } else {
		mypf(fp, "%n_adv = %s + ", MarshState.ms_bufnm);
		MarshState.ms_bufnm = NULL;
	    }
	    if (arg->il_type->td_esize == NULL) {
		mypf(ERR_HANDLE, "%r Can't compute the size of %s\n",
		    arg->il_name);
	    } else {
		ExprSpell(fp, arg->il_type->td_esize, 100, scope, No);
		mypf(fp, "; /* for %s */", arg->il_name);
	    }
	    align = arg->il_type->td_align;
	    Moffset(arg->il_type->td_esize);
	    continue;
	}
	if (!(arg->il_bits & AT_NOMARSH)) {
	    assert(!(arg->il_bits & AT_ADVADV));
	    msh(fp, arg->il_name, arg->il_type, mode, &align);
	    Moffset(arg->il_type->td_esize);
	}
    }
} /* Marshal */
