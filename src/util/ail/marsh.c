/*	@(#)marsh.c	1.3	96/02/27 12:43:41 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"

/*
 *	This file holds functions that should generate the (un)marshalers
 */

/*
 *	(Un)marshaling can sometimes fail.
 *	This routine generates cleanup code for
 *	that by remembering that an error label
 *	must be generated and generating a jump
 *	to it and flagging the return variable.
 */
void
Fail(fp, svr, dir)
    handle fp;
    Bool svr;
    int dir;
{
    MarshState.ms_errlab = Yes;	/* Tell caller to provide us the label	*/
    if (svr) {
	mypf(fp, "%n_hdr.h_status = %s;", dir == AT_REQUEST ?
	    "AIL_SVR_IN_BAD" : "AIL_SVR_OUT_BAD");
    } else {
	mypf(fp, "%n_ret = %s;", dir == AT_REQUEST ?
	    "AIL_CLN_IN_BAD" : "AIL_CLN_OUT_BAD");
    }
    mypf(fp, "%ngoto _fout; /* Marshaling error */");
} /* Fail */

/*
 *	See if we can "steal" an argument of this type from the caller.
 *	The stubs can use this to avoid copying e.g. a string to a local
 *	buffer (in case of a client) or variable (in case of the server).
 */
Bool
CharsOnly(t)
    struct typdesc *t;
{
    t = SameType(t);
    switch (t->td_kind) {
    case tk_char:
    case tk_void:
# ifdef STRING
    case tk_string:	/* Is this true for languages != C?	*/
# endif
	return Yes;
    case tk_struct:
    case tk_union:
# if 0
    {
	struct itlist *itp;
	for (itp = t->td_meml; itp != NULL; itp = itp->il_next)
	    if (!CharsOnly(itp->il_type)) return No;
	return Yes;
    }
# endif
    case tk_err:
    case tk_int:
    case tk_float:
    case tk_double:
    case tk_enum:
    case tk_func:
    case tk_ptr:
    case tk_tdef:	/* SameType returned a typedef => has marshfuncs */
	return No;
    case tk_ref:
    case tk_copy:
    case tk_arr:
	return CharsOnly(t->td_prim);
    }
    /*NOTREACHED*/
} /* CharsOnly	*/

/*
**	I could not get the byte swap routine for the buffer in the
**	server working. So only characters may be passed on in the
**	buffer to the impl_?? routine.
*/
Bool
CharsOrIntsOnly(t)
    struct typdesc *t;
{
#if 0
    t = SameType(t);
    switch (t->td_kind) {
    case tk_char:
    case tk_void:
    case tk_int:
# ifdef STRING
    case tk_string:	/* Is this true for languages != C?	*/
# endif
	return Yes;
    case tk_struct:
    case tk_union:
# if 0
    {
	struct itlist *itp;
	for (itp = t->td_meml; itp != NULL; itp = itp->il_next)
	    if (!CharsOrIntsOnly(itp->il_type)) return No;
	return Yes;
    }
# endif
    case tk_err:
    case tk_float:
    case tk_double:
    case tk_enum:
    case tk_func:
    case tk_ptr:
    case tk_tdef:	/* SameType returned a typedef => has marshfuncs */
	return No;
    case tk_ref:
    case tk_copy:
    case tk_arr:
	return CharsOrIntsOnly(t->td_prim);
    }
    /*NOTREACHED*/
#endif
    return CharsOnly(t);
} /* CharsOrIntOnly	*/


/*
 *	Make sure a type can be marshaled.
 */
void
TouchType(t)
    struct typdesc *t;
{
    while (t->td_kind == tk_ref || t->td_kind == tk_copy)
	t = t->td_prim;
    if (t->td_marsh != NULL) MustDecl(t->td_marsh);
} /* TouchType */

/*
 *	Advance the advancepointer such that
 *	it is aligned at an n-byte boundary.
 *	Don't move it if at such a boundary yet.
 *	Should only be used if the gap is not
 *	of constant size.
 */
void
Align(h, n)
    handle h;
    int n;
{
    switch (n) {
    case 0:
    case 1:
	/* Ignore	*/
	break;
    default:
	mypf(h, "%n_adv = (char *) ((long) (_adv + %d) & ~0x%x); /* Realign */",
	    n-1, n-1);
    }
} /* Align */

/*
 *	Marshaled some bytes; update MarshState.ms_offset
 */
void
Moffset(e)
    struct etree *e;
{
    if (e != NULL && e->et_const) {
	int n;
	n = e->et_val;
	assert(n > 0);
	if (MarshState.ms_offset >= 0)
	    MarshState.ms_offset += n;
    } else MarshState.ms_offset = -1;	/* Invalidate	*/
} /* Moffset */

/*
 *	Initialise the marshaling state
 */
void
Minit(s)
    char *s;
{
    MarshState.ms_errlab = No;
    MarshState.ms_memcpy = No;
    MarshState.ms_shcast = No;
    MarshState.ms_offset = 0;
    MarshState.ms_bufnm = s;
    MarshState.ms_scope = NULL;
    MarshState.ms_check = NULL;
} /* Minit */

/*
 *	Reinit happens when you scan the buffer again
 */
void
Mreinit(s)
    char *s;
{
    MarshState.ms_offset = 0;
    MarshState.ms_bufnm = s;
} /* Mreinit */

/*
 *	Print the header of a marshaling function
 *	This header depends on the structure of the type at hand.
 *	Used prototype: (optional arguments [bracketed like this]).
 * char *<id>(char *_adv, type [*]_arg [, int _eok] [, char *_tail]);
 */
/* static */ void
Mhead(h, name, type, var, server)
    handle h;		/* Where we print on	*/
    char *name;		/* Name of the function	*/
    struct typdesc *type; /* For which type	*/
    Bool var;		/* Pass by reference?	*/
    Bool server;	/* Bothered by byteswapping?	*/
{
    Bool NeedStop;
    Minit((char *) NULL);

    if (type->td_esize == NULL) {
	mypf(h, "%n/* Occupies an unknown amount of memory */%n");
	NeedStop = Yes;
    } else if (type->td_esize->et_const) {
	NeedStop = No;
	mypf(h, "%n/* Takes %D bytes of memory */\n", type->td_esize->et_val);
    } else {
	NeedStop = Yes;
	mypf(h, "%n/* Takes a dynamic amount of memory */%n");
    }

    if (type->td_esize != type->td_msize) {
	if (type->td_msize == NULL) {
	    mypf(h, "/* Occupies an unknown maximum amount of memory */%n");
	} else if (type->td_msize->et_const) {
	    mypf(h, "/* Takes at most %D bytes of memory */\n",
		type->td_msize->et_val);
	} else {
	    mypf(h, "/* Takes a dynamic maximum amount of memory */%n");
	}
    }

    switch (lang) {
    case L_ANSI:
	mypf(h, "char *%n%s(char *_adv, %s %s", /*)*/
	    name, type->td_tname, var ? "*_arg" : "_arg");
	mypf(h, /*(*/ "%s%s)%n",
	    (server && (type->td_bits & TD_CONTINT)) ? ", int _eok" : "",
	    NeedStop ? ", char *_tail" : "");
	break;
    case L_TRADITIONAL:
	mypf(h, "char *%n%s(_adv, _arg", name);
	mypf(h, /*(*/ "%s%s)",
	    (server && (type->td_bits & TD_CONTINT)) ? ", _eok" : "",
	    NeedStop ? ", _tail" : "");
	mypf(h, "%>%nchar *_adv;%n%s %s;",
	    type->td_tname, var ? "*_arg" : "_arg");
	if (server && (type->td_bits & TD_CONTINT)) mypf(h, "%nint _eok;");
	if (NeedStop) mypf(h, "%nchar *_tail;");
	mypf(h, "%<%n");
	break;
    default:
	fatal("Unknown language for marshaler");
    }
} /* Mhead */

/*
 *	Write a marshaler for a typedef.
 */
/* static */ void
MakeMarsh(type, server)
    struct typdesc *type;
    Bool server;
{
    handle out, top, decls; /* Output + two backpatches */
    char fname[200];
    char *name;
    int align;
    name = server ? type->td_marsh->m_svm : type->td_marsh->m_clm;
    sprintf(fname, "%s.c", name);
    out = OpenFile(fname);
    mypf(out, "/*\n *\t%s marshaler for typedef %s by ail.\n */\n\n",
	server ? "Server" : "Client", type->td_tname);
    mypf(out, "#include <ailamoeba.h>\n");
    mypf(out, "#include \"%s.h\"\n", ThisClass->cl_name);
    top = BackPatch(out);
    Mhead(out, name, type, No, server);
    mypf(out, "{%>");
    decls = BackPatch(out);	/* Remember where _pcc might be inserted */
    align = type->td_align;
    /* BUG: the hooks for float are missing	*/
    if (server && (type->td_bits & TD_CONTINT)) {
	mypf(out, "%nif (_eok) {%>");
	msh(out, "_arg", type->td_prim, INT_GOOD, &align);
	mypf(out, "%<%n} else {%>");
	Mreinit((char *) NULL);
	align = type->td_align;
	msh(out, "_arg", type->td_prim, INT_BAD, &align);
	mypf(out, "%<%n}");
    } else {
	msh(out, "_arg", type->td_prim, INT_CLIENT, &align);
    }
    mypf(out, "%nreturn _adv;%<%n} /* %s */%n", name);
    if (MarshState.ms_shcast) {
	MarshState.ms_shcast = No;
	mypf(decls, "%nshort _pcc;");
    }
    DoDecl(top, server);	/* Other declarations		*/
    CloseFile(out);
} /* MakeMarsh */

/*
 *	Write an unmarshaler for a typedef.
 */
/* static */ void
MakeUnMarsh(type, server)
    struct typdesc *type;
    Bool server;
{
    handle out, top;
    char fname[200];
    char *name;
    int align = type->td_align;
    name = server ? type->td_marsh->m_svu : type->td_marsh->m_clu;
    sprintf(fname, "%s.c", name);
    out = OpenFile(fname);
    mypf(out, "/*\n *\t%s unmarshaler for typedef %s by ail.\n */\n\n",
	server ? "Server" : "Client", type->td_tname);
    mypf(out, "#include <ailamoeba.h>\n");
    mypf(out, "#include \"%s.h\"\n", ThisClass->cl_name);
    top = BackPatch(out);
    Mhead(out, name, type, Yes, server);
    Minit((char *) NULL);
    mypf(out, "{%>");
    if (server && (type->td_bits & TD_CONTINT)) {
	mypf(out, "%nif (_eok) {%>");
	unmsh(out, "_arg", RefTo(type->td_prim), INT_GOOD, &align);
	mypf(out, "%<%n} else {%>");
	Mreinit((char *) NULL);
	align = type->td_align;
	unmsh(out, "_arg", RefTo(type->td_prim), INT_BAD, &align);
	mypf(out, "%<%n}");
    } else {
	unmsh(out, "_arg", RefTo(type->td_prim), INT_CLIENT, &align);
    }
    mypf(out, "%nreturn _adv;%<%n} /* %s */%n", name);
    DoDecl(top, server);	/* Other declarations		*/
    CloseFile(out);
} /* MakeUnMarsh */

/*
 *	Generate marshal functions for a specific type,
 *	The real work is done by Make[Un]Marsh().
 */
/*ARGSUSED*/
void
MarshPair(args)
    struct gen *args;
{
    struct typdesc *type;
    type = IsTD(args->gn_name);
    if (type == NULL) {
	mypf(ERR_HANDLE, "%r %s is not a typedef\n", args->gn_name);
	return;
    }
    if (type->td_marsh == NULL) {
	mypf(ERR_HANDLE,
	    "%r type %s not declared to use marshaling functions\n",
	    args->gn_name);
	return;
    }
    if (type->td_marsh->m_clm) MakeMarsh(type, No);
    if (type->td_marsh->m_clu) MakeUnMarsh(type, No);
    if (type->td_marsh->m_svm) MakeMarsh(type, Yes);
    if (type->td_marsh->m_svu) MakeUnMarsh(type, Yes);
} /* MarshPair */
