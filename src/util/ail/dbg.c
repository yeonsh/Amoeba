/*	@(#)dbg.c	1.2	94/04/07 14:32:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"

/*
 *	Explain a type
 */
void
ShowType(type)
    struct typdesc *type;
{
    struct m_name *mn;
    /* stackchk(); */
    chkptr(type);
    assert(type != NULL);
    chkptr(type->td_parent);
    assert(type->td_parent != NULL);
/*  chkptr(type->td_parent->cl_name); Not required after all? */
    assert(type->td_parent->cl_name != NULL);
    mypf(OUT_HANDLE, "%p in %s: ", type, type->td_parent->cl_name);
    switch(type->td_kind) {
    case tk_dunno:
	mypf(OUT_HANDLE, "dunno");
	break;
    case tk_err:
	mypf(OUT_HANDLE, "error");
	break;
    case tk_char:
	mypf(OUT_HANDLE, "char size=%d sign=%d", type->td_size, type->td_sign);
	break;
    case tk_int:
	mypf(OUT_HANDLE, "int size=%d sign=%d", type->td_size, type->td_sign);
	break;
    case tk_float:
	mypf(OUT_HANDLE, "float size=%d sign=%d", type->td_size,type->td_sign);
	break;
    case tk_double:
	mypf(OUT_HANDLE, "double size=%d sign=%d",type->td_size,type->td_sign);
	break;
    case tk_void:
	mypf(OUT_HANDLE, "void");
	break;
    case tk_struct:
	if (type->td_name == NULL)
	    mypf(OUT_HANDLE, "struct <untagged>");
	else
	    mypf(OUT_HANDLE, "struct %s", type->td_name);
	if (type->td_meml != NULL)
	    mypf(OUT_HANDLE, ", 1st mbr: %s", type->td_meml->il_name);
	break;
    case tk_union:
	mypf(OUT_HANDLE, "union %s", type->td_name);
	break;
    case tk_enum:
	mypf(OUT_HANDLE, "enum %s", type->td_name);
	break;
    case tk_func:
	mypf(OUT_HANDLE, "func returning %p", type->td_prim);
	break;
    case tk_ptr:
	mypf(OUT_HANDLE, "pointer to %p", type->td_prim);
	break;
    case tk_ref:
	mypf(OUT_HANDLE, "reference to %p", type->td_prim);
	break;
    case tk_arr:
	mypf(OUT_HANDLE, "array of %p", type->td_prim);
	break;
    case tk_tdef:
	mypf(OUT_HANDLE, "typedef %s to %p", type->td_tname, type->td_prim);
	assert(type->td_prim != NULL);
	break;
    case tk_copy:
	mypf(OUT_HANDLE, "copy of %p", type->td_prim);
	break;
    default:
	mypf(OUT_HANDLE, "***BUG*** td_kind = %d", type->td_kind);
    }
    if (type->td_bits & TD_CONTINT) mypf(OUT_HANDLE, " int-order dependent");
    if (type->td_bits & TD_CONTFLT) mypf(OUT_HANDLE, " float-order dependent");
    mn = type->td_marsh;
    if (mn == NULL) mypf(OUT_HANDLE, "\nNo marsh");
    else mypf(OUT_HANDLE, "\nmarsh(%p, %p, %p, %p)",
	mn->m_clm, mn->m_clu, mn->m_svm, mn->m_svu);
    if (type->td_esize == NULL) {
	mypf(OUT_HANDLE, ", no esize");
    } else {
	chkptr(type->td_esize);
	mypf(OUT_HANDLE, ", %s esize",
	    type->td_esize->et_const ? "constant" : "dynamic");
    }
    if (type->td_msize == NULL) {
	mypf(OUT_HANDLE, ", no msize\n");
    } else {
	chkptr(type->td_msize);
	mypf(OUT_HANDLE, ", %s msize\n",
	    type->td_msize->et_const ? "constant" : "dynamic");
    }
} /* ShowType */

/*
 *	Routine for debugging purposes:
 */
/*ARGSUSED*/
void
ShowTypTab(p)
    struct gen *p;
{
    static counter = 0;
    struct name *walk;
    struct clist *clp;

    if (p != NULL) {
	struct name *nm;
	nm = FindName(p->gn_name, ThisClass->cl_scope);
	if (nm == NULL) {
	    mypf(ERR_HANDLE, "%r type \"%s\"not found\n", p->gn_name);
	} else {
	    TypeSpell(OUT_HANDLE, nm->nm_type, Yes);
	}
	return;
    }
    mypf(OUT_HANDLE, "\n%w Global types - run %d:\n", ++counter);
    for (walk = GlobClass.cl_scope; walk != NULL; walk = walk->nm_next)
	switch (walk->nm_kind) {
	case TYPEDEF:
	case STRUCT:
	case UNION:
	case ENUM:
	    ShowType(walk->nm_type);
	case CALL:
	    break;
	}
    for (clp = ClassList; clp != NULL; clp = clp->cl_next) {
	mypf(OUT_HANDLE, "\nClass %s:\n", clp->cl_this->cl_name);
	for (walk = clp->cl_this->cl_scope; walk != NULL; walk = walk->nm_next)
	switch (walk->nm_kind) {
	    case TYPEDEF:
	    case STRUCT:
	    case UNION:
	    case ENUM:
		ShowType(walk->nm_type);
	    case CALL:
		break;
	}
    }
} /* ShowTypTab */

/*
 *	List the arguments to an operator
 */
void
ShowArgs(arg)
    struct itlist *arg;
{
    if (arg == NULL) {
	mypf(OUT_HANDLE, "\tNo arguments\n");
    } else do {
	mypf(OUT_HANDLE, " %s: type %p", arg->il_name, arg->il_type);
	if (arg->il_bits & AT_REQUEST) mypf(OUT_HANDLE, " in");
	if (arg->il_bits & AT_REPLY) mypf(OUT_HANDLE, " out");
	if (arg->il_bits & AT_VAR) mypf(OUT_HANDLE, " var");
	if (arg->il_bits & AT_NODECL) mypf(OUT_HANDLE, " nodecl");
	if (arg->il_bits & AT_CHARBUF) mypf(OUT_HANDLE, " charbuf");
	if (arg->il_bits & AT_ADVADV) mypf(OUT_HANDLE, " advadv");
	if (arg->il_bits & AT_REDECL) mypf(OUT_HANDLE, " redecl");
	if (arg->il_bits & AT_CAST) mypf(OUT_HANDLE, " cast");
	if (arg->il_attr->at_hmem != NULL)
	    mypf(OUT_HANDLE, " (%s)", arg->il_attr->at_hmem);
	mypf(OUT_HANDLE, "\n");
	arg = arg->il_next;
    } while (arg != NULL);
} /* ShowArgs */

/*
 *	Show all the operators
 */
void
ShowOptrs(scope)
    struct name *scope;
{
    mypf(OUT_HANDLE, "\n");
    for (; scope != NULL; scope = scope->nm_next) if (scope->nm_kind == CALL) {
	struct optr *op;
	op = scope->nm_optr;
	mypf(OUT_HANDLE, "Operator %s (%d) in: h%d,b%d, out: h%d,b%d",
	    op->op_name, op->op_val, op->op_ih,op->op_ib, op->op_oh,op->op_ob);
	if (op->op_flags & OP_STARRED) mypf(OUT_HANDLE, " starred");;
	if (op->op_flags & OP_INTSENS) mypf(OUT_HANDLE, " int-sens");
	if (op->op_flags & OP_VARSENS) mypf(OUT_HANDLE, " var-sens");

#if 1
	mypf(OUT_HANDLE, "\nIn-buf struct:\n");
	ShowType(op->op_in);
	mypf(OUT_HANDLE, "\nOut-buf struct:\n");
	ShowType(op->op_out);
#else
	mypf(OUT_HANDLE, "\nOrder field:\n"); ShowArgs(op->op_order);
	mypf(OUT_HANDLE, "Args field:\n"); ShowArgs(op->op_args);
	mypf(OUT_HANDLE, "Client field:\n"); ShowArgs(op->op_client);
	mypf(OUT_HANDLE, "Server field:\n"); ShowArgs(op->op_server);
#endif
	mypf(OUT_HANDLE, "\n");
    }
} /* ShowOptrs */

/*
 *	List all classes + contents
 */
void
ShowClasses()
{
    struct clist *clp;
    for (clp = ClassList; clp != NULL; clp = clp->cl_next) {
	mypf(OUT_HANDLE,"\n***Class %s:***\n", clp->cl_this->cl_name);
	mypf(OUT_HANDLE, "Operators are:\n");
	ShowOptrs(clp->cl_this->cl_scope);
    }
} /* ShowClasses */

/*
 *	Same function, with the right prototype
 *	so I can call it legitimately from Generate()
 */
/*ARGSUSED*/
void
ShowClassGen(args)
    struct gen *args;
{
    ShowClasses();
} /* ShowClassGen */

/*ARGSUSED*/
void
Verbose(p)
    struct gen *p;
{
    struct clist *clp;
    mypf(OUT_HANDLE, "Class %s [%d..%d]\n", ThisClass->cl_name,
	ThisClass->cl_lo, ThisClass->cl_hi);
    mypf(OUT_HANDLE, "Effective inheritance:");
    clp = ThisClass->cl_effinh;
    if (clp == NULL) mypf(OUT_HANDLE, "none.");
    else while (clp != NULL) {
	register struct class *here;
	here = clp->cl_this;
	mypf(OUT_HANDLE, " %s [%d..%d]", here->cl_name,
	    here->cl_lo, here->cl_hi);
	clp = clp->cl_next;
    }
    ShowOptrs(ThisClass->cl_scope);
} /* Verbose */
