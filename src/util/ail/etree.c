/*	@(#)etree.c	1.3	96/02/27 12:43:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

/*
 *	This file contains primitives to handle
 *	expression-trees.
 *	They fall into these catagories:
 *	functions that build leafs, non-leafs,
 *	the function that destructs them,
 *	and the ones that inspect them; either
 *	by spelling litterally or by returning
 *	their value.
 */

/* All expressions are unique, and kept in a list: 		*/
static struct etree *DiadE = NULL, *MonadE = NULL, *NoadE = NULL;
/* This expression is symbolical; only it's address is used	*/
static struct etree Self = { '@', No, NULL, NULL /* , Missing field */ };

static void SpellPar ARGS((handle, char *, struct itlist *, Bool));
static void LeafSpell ARGS((handle, struct etree *, struct itlist *, Bool));
static void MonoSpell ARGS((handle, struct etree *, struct itlist *, Bool));
static void DiadSpell ARGS((handle, struct etree *, struct itlist *, Bool));

/************************************************\
 *						*
 *	The functions that build non-leafs:	*
 *						*
\************************************************/

/*
 *	Create a new node for a diadic operator
 */
struct etree *
Diadic(optr, left, right)
    int optr;
    struct etree *left, *right;
{
    struct etree *ret;
    assert(left != NULL);
    assert(right != NULL);
    if (optr == 'm')	/* These lines are a temp placeholder; 'm' means max */
	mypf(ERR_HANDLE, "%w Operator 'm' in etree.c not implemented\n");
if (optr == ',' && left->et_optr != ',') mypf(ERR_HANDLE, "Bah!\n");

    /* Optimise out +0 and *1	*/
    if (optr == '+') {
	if (left->et_const && left->et_val == 0)
	    return right;
	if (right->et_const && right->et_val == 0)
	    return left;
    } else if (optr == '*') {
	if (left->et_const && left->et_val == 1)
	    return right;
	if (right->et_const && right->et_val == 1)
	    return left;
    }

    /* Did I make such a descriptor yet? */
    for (ret = DiadE; ret != NULL; ret = ret->et_next)
	if (ret->et_optr == optr &&
	  ret->et_left == left && ret->et_right == right) {
	    /* Found! */
	    return ret;
	}
    /* Not found; put new descriptor in DiadE-list */
    ret = new(struct etree);
    ret->et_next = DiadE;
    DiadE = ret;
    /* Assign */
    ret->et_left = left;
    ret->et_right = right;
    ret->et_optr = optr;
    ret->et_const = left->et_const && right->et_const &&
	optr != '.' && optr != CALL && optr != ','; /* Never constant	*/
    /*
     *	Compute constant value, if any
     */
    if (ret->et_const) {
	switch(optr) {
	    case OR:
		ret->et_val = left->et_val || right->et_val;
		break;
	    case AND:
		ret->et_val = left->et_val && right->et_val;
		break;
	    case '|':
		ret->et_val = left->et_val | right->et_val;
		break;
	    case '^':
		ret->et_val = left->et_val ^ right->et_val;
		break;
	    case '&':
		ret->et_val = left->et_val & right->et_val;
		break;
	    case EQUAL:
		ret->et_val = left->et_val == right->et_val;
		break;
	    case UNEQU:
		ret->et_val = left->et_val != right->et_val;
		break;
	    case LEQ:
		ret->et_val = left->et_val <= right->et_val;
		break;
	    case GEQ:
		ret->et_val = left->et_val >= right->et_val;
		break;
	    case '<':
		ret->et_val = left->et_val < right->et_val;
		break;
	    case '>':
		ret->et_val = left->et_val > right->et_val;
		break;
	    case LEFT:
		ret->et_val = left->et_val << right->et_val;
		break;
	    case RIGHT:
		ret->et_val = left->et_val >> right->et_val;
		break;
	    case '+':
		ret->et_val = left->et_val + right->et_val;
		if (options & OPT_DEBUG)
		    mypf(OUT_HANDLE, "%D + %D = %D\n",
			left->et_val, right->et_val, ret->et_val);
		break;
	    case '-':
		ret->et_val = left->et_val - right->et_val;
		break;
	    case '*':
		ret->et_val = left->et_val * right->et_val;
		break;
	    case '/':
		ret->et_val = left->et_val / right->et_val;
		break;
	    case '%':
		ret->et_val = left->et_val % right->et_val;
		break;
	    default: assert(0);
	}
    }
    return ret;
} /* Diadic */

/*
 *	Create a node for a monadic operator.
 */
struct etree *
Monadic(optr, opnd)
    int optr;
    struct etree *opnd;
{
    struct etree *ret;
    assert(opnd != NULL);
    /* There are two unary nil-operators: parens and the '+'. */
    if (optr == '+' || optr == '(')	/* ')' for vi */
	return opnd;
    /* Got such a descriptor yet? */
    for (ret = MonadE; ret != NULL; ret = ret->et_next)
	if (ret->et_optr == optr && ret->et_right == opnd) {
	    /* Found! */
	    return ret;
	}
    /* Not found; create descriptor */
    ret = new(struct etree);
    ret->et_next = MonadE;
    MonadE = ret;
    /* Assign */
    ret->et_const = opnd->et_const && optr != ',';
    ret->et_left = NULL;
    ret->et_right = opnd;
    ret->et_optr = optr;
    /* Compute if constant */
    if (ret->et_const) switch(optr) {
	case '-':
	    ret->et_val = - opnd->et_val;
	    break;
	case '!':
	    ret->et_val = ! opnd->et_val;
	    break;
	case '~':
	    ret->et_val = ~ opnd->et_val;
	    break;
	default:
	    assert(0);
    }
    return ret;
} /* Monadic */

/************************************************\
 *						*
 *	The functions that create leafs:	*
 *						*
\************************************************/

/*
 *	Return pointer to self. How this expression is printed
 *	depends on the context in which it must be evaluated.
 */
struct etree *
Sleaf()
{
    return &Self;
} /* Sleaf */

/*
 *	Create leaf for ident.
 *	Yields a constant if there is one with this name.
 */
struct etree *
Ileaf(id, con)
    char *id;
    Bool con;	/* May be replaced with a constant	*/
{
    struct etree *ret;
    struct class *clp;
    struct name *nm;
    if (con && WhereIsName(id, &clp, &nm)) {
	if (nm->nm_kind != INTCONST) {
	    mypf(ERR_HANDLE, "%r %s is not an integer constant\n", id);
	} else return Cleaf(nm->nm_ival);
    }
    /* It is definitly gonna be an IDENT-node */
    for (ret = NoadE; ret != NULL; ret = ret->et_next)
	if (ret->et_optr == IDENT && ret->et_id == id) {
	    return ret;
	}
    /* Not found */
    ret = new(struct etree);
    ret->et_next = NoadE;
    NoadE = ret;
    ret->et_const = 0;
    ret->et_id = id;
    ret->et_left = ret->et_right = NULL;
    ret->et_optr = IDENT;
    return ret;
} /* Ileaf */

/*
 *	Create a node for a constant.
 */
struct etree *
Cleaf(val)
    long val;
{
    struct etree *ret;
    for (ret = NoadE; ret != NULL; ret = ret->et_next)
	if (ret->et_optr == INTCONST && ret->et_val == val) {
	    return ret;
	}
    ret = new(struct etree);
    ret->et_next = NoadE;
    NoadE = ret;
    ret->et_const = 1;
    ret->et_val = val;
    ret->et_left = ret->et_right = NULL;
    ret->et_optr = INTCONST;
    return ret;
} /* Cleaf */

/********************************************************\
 *							*
 *	Functions that tell what's in an etree:		*
 *							*
\********************************************************/

/*
 *	Say "yes" if the expression is a constant 0.
 */
int
IsZero(p)
    struct etree *p;
{
    assert(p != NULL);
    return p->et_const && p->et_val == 0;
} /* IsZero */

/*
 *	List all identifiers in an expression.
 *	BUG: barfs on IDENTs when they are
 *	the names of functions (i.e. left child of a CALL).
 */
struct stind *
UsedIdents(p)
    struct etree *p;
{
    struct stind *left, *right, *w;
    if (p == NULL) return NULL;
    if (p->et_optr == IDENT) {
	left = new(struct stind);
	left->st_str = p->et_id;
	left->st_nxt = NULL;
	return left;
    }
    left = UsedIdents(p->et_left);
    right= UsedIdents(p->et_right);
    if (left == NULL) return right;
    /* Glue them together	*/
    for (w = left; w->st_nxt != NULL; w = w->st_nxt)
	;
    w->st_nxt = right;
    return left;
} /* UsedIdents */

long
GetVal(p)
    struct etree *p;
{
    struct name *nm;

    if (p->et_const) {
	return p->et_val;
    } else if (p->et_optr == IDENT) {
	if ((nm = WhereIsIConst(p->et_id)) != NULL) {
	    return nm->nm_ival;
	}
	mypf(ERR_HANDLE, "%r %s is not an integer constant\n", p->et_id);
    } else {
	mypf(ERR_HANDLE, "%r Expression should be constant\n");
    }

    /* If we come here, an error occurred. Still we have to return something */
    return 1;
} /* GetVal */

/********************************************************\
 *							*
 *	To print an expression-tree on a file		*
 *							*
\********************************************************/

/*
 *	Compute precedence of node. 'atoms' have precedence 0,
 *	unary expressions 1, and binary expressions higher ones.
 *	Associativity is not a big problem: except for assignment and unary
 *	operators, all operators of C are left-to-right associative.
 *	Assignment is not an operator in ail, so we can just take any binary
 *	operator as left-to-right associative.
 */

static int
Prec(p)
    struct etree *p;
{
    assert(p != NULL);
    if (p->et_left == NULL && p->et_right != NULL) /* Unary expression */
		return 1;
    switch(p->et_optr) {
    /* 'atoms': */
    case IDENT:	case INTCONST: case CALL: case ',': case '@':
		return 0;
    /* Binary operators: */
    case '*':	case '/':   case '%':
		return 2;
    case '+':	case '-':
		return 3;
    case LEFT:	case RIGHT:
		return 4;
    case '<':	case '>':
		return 5;
    case EQUAL:	case UNEQU:
		return 6;
    case '&':
		return 7;
    case '^':
		return 8;
    case '|':
		return 9;
    case AND:
		return 10;
    case OR:
		return 11;
# ifndef NDEBUG
    default:
		mypf(ERR_HANDLE, "BUG: %d; ", p->et_optr);
		fatal("Prec()\n");
# endif /* NDEBUG */
    } /* Switch */
    assert(0);
    /*NOTREACHED*/
} /* Prec */

/*
 *	Emit a unary operator + operand.
 */
static void
MonoSpell(fp, p, scope, UseHval)
    handle fp;
    struct etree *p;
    struct itlist *scope;
    Bool UseHval;	/* Use value in header, if any */
{
    /* This IS actually in the tree, but should be handled elsewhere:	*/
    /* assert(p->et_optr != ','); */
    mypf(fp, "%s", TokenToString(p->et_optr));
    ExprSpell(fp, p->et_right, Prec(p), scope, UseHval);
} /* MonoSpell */

/*
 *	Emit a binary operator + operands.
 */
static void
DiadSpell(fp, p, scope, UseHval)
    handle fp;
    struct etree *p;
    struct itlist *scope;
    Bool UseHval;	/* Use value in header, if any */
{
    if (p->et_optr != CALL) {
	int prior;
	prior = Prec(p);
	ExprSpell(fp, p->et_left, prior, scope, UseHval);
	mypf(fp, "%s", TokenToString(p->et_optr));
	ExprSpell(fp, p->et_right, prior - 1, scope, UseHval);	/* - 1 for assoc */
    } else {
	assert(p->et_left->et_optr == IDENT);
	mypf(fp, "%s(", p->et_left->et_id);
	/*
	 * "Er, where can I find argument-street, mister?"
	 * "First road to the right, then turn left until
	 *  you're at the end of the argumentlist."
	 * "Thanks!"
	 * "You're welcome. By the way; there's a map at the parser"
	 * "Ah! So I should have fucked the manual?"
	 * "... No comments on that."
	 */
	for (p = p->et_right; ; p = p->et_left) {
if (p->et_optr != ',') mypf(ERR_HANDLE, "Optr: %d ('%c')\n", p->et_optr, p->et_optr);
if (p == NULL) mypf(ERR_HANDLE, "Maar hij is ook NULL\n");
	    assert(p->et_optr == ',');
	    ExprSpell(fp, p->et_right, 100, scope, UseHval);
	    if (p->et_left == NULL) break;	/* Last argument	*/
	    mypf(fp, ", ");
	}
	mypf(fp, ")");
    }
} /* DiadSpell */

/*
 *	SpellPar checks that the id is in the currently processed
 *	operator, and places it on a file, possibly preceded by a
 *	'*', produced by Prefix().
 */
static void
SpellPar(fp, id, scope, UseHval)
    handle fp;
    char *id;
    struct itlist *scope;
    Bool UseHval;	/* Use value in header, if any */
{
    struct itlist *p;
    assert(id != NULL);
    for (p = scope; p != NULL; p = p->il_next) {
	chkptr(p);
	if (p->il_name == id) {
	    if (p->il_attr->at_hmem != NULL && UseHval) {
		mypf(fp, "_hdr.%s", p->il_attr->at_hmem);
	    } else {
		Prefix(fp, p->il_type);
		mypf(fp, id);
	    }
	    return;
	}
    }
    mypf(ERR_HANDLE, "%r %s is not a parameter\n", id);
} /* SpellPar */

static void
LeafSpell(fp, p, scope, UseHval)
    handle fp;
    struct etree *p;
    struct itlist *scope;
    Bool UseHval;
{
    switch(p->et_optr) {
    case '@':
	mypf(fp, "<Self>");
	mypf(ERR_HANDLE, "%w Emitted useless code (self)\n");
	break;
    case INTCONST:
	mypf(fp, "%D", p->et_val);
	break;
    case IDENT:
	SpellPar(fp, p->et_id, scope, UseHval);
	break;
    default:
	assert(0);
    }
} /* LeafSpell */

/*
 *	Emit a complete expression,
 *	emit parenthesis wherever needed.
 */
void
ExprSpell(fp, p, prior, scope, UseHval)
    handle fp;
    struct etree *p;
    int prior;	/* Priority of superseding expression	*/
    struct itlist *scope;
    Bool UseHval;	/* Use value in header, if any */
{
    assert(p != NULL);
    assert(prior>=0);
    chkptr(scope);
    if (p->et_const) mypf(fp, "%D", p->et_val);
    else {
	if (Prec(p) > prior) mypf(fp, "(");
	if (p->et_left != NULL) {
	    DiadSpell(fp, p, scope, UseHval);
	} else if (p->et_right != NULL) {
	    MonoSpell(fp, p, scope, UseHval);
	} else {
	    LeafSpell(fp, p, scope, UseHval);
	}
	if (Prec(p) > prior) mypf(fp, ")");
    }
} /* ExprSpell */

/*
 *	Warning: comment out of date!!
 *	(So what? it is not used anymore either)
 *
 *	Check if the identifiers in ids are valid.
 *	Basically, I check for the existence and type.
 *	Due to the way MakeMsg() works, id's might be
 *	in either of TWO (2) lists.
 */
void
CheckExpr(list, ids, io)
    struct itlist *list;	/* It should be in this list	*/
    struct stind *ids;
    int io;			/* Which directions are ok	*/
{
    for (; ids != NULL; ids = ids->st_nxt) {
	char *id;
	id = ids->st_str;
	for (; list != NULL; list = list->il_next) {
	    if (list->il_name == id)
		break;
	}
	/* Did we find the parameter? */
	if (list == NULL) { /* No */
	    mypf(ERR_HANDLE, "%r parameter %s not found\n", id);
	} else {	/* Check type and in/out attributes	*/
	    struct typdesc *typ;
	    typ = list->il_type;
	    /* Skip optional typedefs	*/
	    while (typ->td_kind == tk_tdef) {
		assert(typ->td_kind == tk_ref);
		typ = typ->td_prim;
	    }
	    switch(typ->td_kind) {
	    /* These types are ok: */
	    case tk_int:
	    case tk_char:
	    case tk_err:
		break;
	    /* These are not: */
	    default:
		mypf(ERR_HANDLE, "%r parameter %s has wrong type\n", id);
	    }
	    if ((list->il_attr->at_bits & io) == 0) {
		mypf(ERR_HANDLE, "%r parameter %s has wrong attributes\n", id);
	    }
	}
    }
} /* CheckExpr */
