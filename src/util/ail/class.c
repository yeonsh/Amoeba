/*	@(#)class.c	1.2	94/04/07 14:31:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
/*
 *	This file deals with entering operators
 *	and classes in the symboltable.
 */

/* All the classes we seen	*/
struct clist *ClassList = NULL;

/*
 *	NewClass: make a new class-descriptor,
 *	and link it in the class-list.
 *	No checks.
 */
struct class *
NewClass(name)
    char *name;
{
    struct class *cl;
    struct clist *li;
    cl = new(struct class);
    cl->cl_name = name;
    cl->cl_align = 1;	/* Least restrictive alignment requirement */
    /* Put in the ClassList: */
    li = new(struct clist);
    li->cl_this = cl;
    li->cl_next = ClassList;
    ClassList = li;
    return cl;
} /* NewClass */

/*
 *	DefClass: put a class-descriptor in the symboltable.
 *	If all the operators in the class have been declared, the
 *	parser calls CloseClass().
 */
void
DefClass(name, has_range, lo, hi, inherit, incls)
    char *name;
    int has_range;		/* [2..3] specified?		*/
    int lo, hi;			/* Limits if any		*/
    struct clist *inherit;	/* Inheritancelist		*/
    struct stind *incls;	/* Which .h files to #include	*/
{
    struct clist *cwalk;
    if (options & OPT_DEBUG) ShowTypTab((struct gen *) NULL);
    /* Check for duplicate class definition: */
    dbg2("DefClass(%s)\n", name);
    chkptr(ClassList);
    for (cwalk = ClassList; cwalk != NULL; cwalk = cwalk->cl_next) {
	chkptr(cwalk);
	chkptr(cwalk->cl_this);
	if (cwalk->cl_this->cl_name == name) {
	    mypf(ERR_HANDLE, "%r redefinition of class %s\n", name);
	    return;
	}
    }
    if (has_range && hi < lo) {
	register int swap;
	swap = hi; hi = lo; lo = swap;
    }
    ThisClass = NewClass(name);
    ThisClass->cl_lo = lo;
    ThisClass->cl_hi = hi;
    ThisClass->cl_last = lo - 1;
    ThisClass->cl_flags = has_range ? 0 : CL_NORANGE;
    ThisClass->cl_definh = inherit;
    ThisClass->cl_bsize = -1;
    ThisClass->cl_rights = ThisClass->cl_inhrights = 0;
    ThisClass->cl_scope = NULL;
    ThisClass->cl_effinh = InhStuff(inherit);
    ThisClass->cl_incl = incls;
    n_optrs = 0;
    for (cwalk = inherit; cwalk != NULL; cwalk = cwalk->cl_next)
	if (cwalk->cl_this->cl_align > ThisClass->cl_align)
	    ThisClass->cl_align = cwalk->cl_this->cl_align;
} /* DefClass */

/*
 *	Did some checks, used to be a big function...
 */
void
CloseClass()
{
    ThisClass = &GlobClass;
} /* CloseClass */

/*
 *	FindClass: find a class-descriptor in the symboltable.
 */
struct class *
FindClass(id)
    char *id;
{
    struct clist *walk;
    for (walk = ClassList; walk != NULL; walk = walk->cl_next)
	if (walk->cl_this->cl_name == id) return walk->cl_this;
    mypf(ERR_HANDLE, "%r unknown class %s\n", id);
    return NULL;
} /* FindClass */

/*
 *	Copy an itlist.
 *	Generators want to do this in order to get
 *	"their view on the world"
 */
struct itlist *
CopyItList(list)
    struct itlist *list;
{
    struct itlist *front, *tail;
    if (list == NULL) return NULL;	/* Empty list	*/
    /* First element:	*/
    tail = front = new(struct itlist);
    *front = *list;
    /* The rest:	*/
    while ((list = list->il_next) != NULL) {
	tail = tail->il_next = new(struct itlist);
	*tail = *list;
    }
    assert(tail->il_next == NULL);
    return front;
} /* CopyItList */

/*
 *	Same as above, but selectively, and skipping any header parameters
 */
static struct itlist *
CopyIts(list, set)
    struct itlist *list;
    int set;
{
    struct itlist *front, *tail;
    /* Skip first irrelevant members	*/
    while (list != NULL && (list->il_attr->at_hmem != NULL ||
      !(list->il_bits & set)))
	list = list->il_next;
    if (list == NULL) {
	return NULL;
    }
    /* First element:	*/
    tail = front = new(struct itlist);
    *front = *list;
    /* The rest:	*/
    while ((list = list->il_next) != NULL)
	if (list->il_attr->at_hmem == NULL && (list->il_bits & set)) {
	    tail = tail->il_next = new(struct itlist);
	    *tail = *list;
	}
    tail->il_next = NULL;
    return front;
} /* CopyIts */

/*
 *	Predict whether a C-compiler will do argumentwidening on this
 *	type on function-call evaluation.
 *	This doesn't work well with prototypes
 *	If we don't tackle this problem in ail, it is far too simple
 *	to automate the process of bug creation.
 */
/* static */ int
ArgWiden(t)
    struct typdesc *t;
{
    /* Argument widening happens with:	*/
    t = SameType(t);
    switch (t->td_kind) {
    case tk_int:
	return t->td_size < 0;
    case tk_char:
    case tk_float:
	return Yes;
    default:
	return No;
    }
    /*NOTREACHED*/
} /* ArgWiden */

/*
 *	Construct an expression yielding the sum
 *	of the sizes of certain arguments in a list.
 *	This routine is called by the routines that
 *	generate stubs, in order to calculate the
 *	buffer size they need, so parameters that are
 *	in the header are not considered, and holes
 *	(for alignment) are. The size of a hole
 *	cannot always be predicted compile-time, in
 *	which case we are pessimistic.
 *	They have private copies of the original
 *	argumentlist, which might be modified.
 *	In particular, they can add bits in the il_bits
 *	field. Selection of which arguments are
 *	considered and which are not is on these bits.
 *	In "needed" are the bits that should be 1,
 *	in "forbidden" are the bits that should be 0,
 *	i.e. the forbidden bits.
 *
 *	"Maxf" indicates wether we're interested in the
 *	actual size or in the maximum size.
 *
 *	A return value of NULL indicates an error.
 */

struct etree *
SizeSum(args, needed, forbidden, maxf)
    struct itlist *args;	/* The list to be searched		*/
    int needed, forbidden;	/* Which args are we interested in?	*/
    int maxf;			/* Want maximum? (otherwise use actual)	*/
{
    struct etree *sum;
    sum = NULL;
    for (;args != NULL; args = args->il_next) {
	assert(args->il_attr != NULL);
	assert(args->il_bits & 0x03);
	if (args->il_attr->at_hmem != NULL) continue;
	if (((args->il_bits & needed) == needed) &&
	   ((args->il_bits & forbidden) == 0)) {
	    struct etree *etter;
	    etter = maxf ? args->il_type->td_msize : args->il_type->td_esize;
	    /*
	     *	Here we got a problem: ClientAnalysis uses a
	     *	try-and-error method to decide. In some cases,
	     *	we DON'T want this to be an error.
	     */
	    if (etter == NULL) {
		mypf(ERR_HANDLE, "%r %s: unknown size\n", args->il_name);
		return NULL;
	    }
	    if (sum == NULL) sum = etter;
	    else sum = Diadic('+', sum, etter);
	}
    }
    /* Did we see any parameters?	*/
    if (sum == NULL) sum = Cleaf((long) 0);
    return sum;
} /* SizeSum */

/*
 *	Move in/out parameters to the front of a parameter-list.
 */
/* static */ struct itlist *
InOutFirst(args)
    struct itlist *args;
{
    struct itlist *inout, *rest, *next;
    if (args == NULL) return NULL;
    inout = rest = NULL;
    /* Split	*/
    for (;args != NULL; args = next) {
	next = args->il_next;
	if ((args->il_bits & (AT_REQUEST|AT_REPLY)) == (AT_REQUEST|AT_REPLY)) {
	    args->il_next = inout;
	    inout = args;
	} else {
	    args->il_next = rest;
	    rest = args;
	}
    }
    /* Find tail of inout	*/
    if (inout == NULL) return rest;
    for (args = inout; args->il_next != NULL; args = args->il_next)
	;
    /* Join	*/
    args->il_next = rest;
    return inout;
} /* InOutFirst */

/*
 *	Split a parameterlist in a fixed size and a dynamic size part
 */
/* static */ void
SplitFixDyn(orig, fix, dyn)
    struct itlist *orig, **fix, **dyn;
{
    struct itlist *next;
    *fix = *dyn = NULL;		/* Empty these lists	*/
    for (; orig != NULL; orig = next) {
	next = orig->il_next;
	if (orig->il_type->td_esize != NULL &&	/* Size known	*/
	  orig->il_type->td_esize->et_const) {	/* and constant	*/
	    /* Put in fix-list	*/
	    orig->il_next = *fix;
	    *fix = orig;
	} else {
	    /* Put in dyn-list	*/
	    orig->il_next = *dyn;
	    *dyn = orig;
	}
    }
} /* SplitFixDyn */

/*
 *	Filter out stuff with an error-type
 *	so I don't have to check everywhere
 */
static struct itlist *
DisposeErrors(list)
    struct itlist *list;
{
    struct itlist *walk, *prev;
    prev = NULL; walk = list;
    while (walk != NULL) {
	if (walk->il_type == &ErrDesc) {	/* Filter	*/
	    if (prev == NULL) {		/* Is first	*/
		list = walk = walk->il_next;
	    } else {			/* Isn't	*/
		prev->il_next = walk = walk->il_next;
	    }
	} else {			/* Proceed	*/
	    prev = walk;
	    walk = walk->il_next;
	}
    }
    return list;
} /* DisposeErrors */

/*
 *	This routine checks that all header-hints in the the list
 *	are valid, and reserves the corresponding headerfields,
 *	so ail won't ship two values in the same field.
 */
/* static */ void
CheckHeadHints(walk)
    struct itlist *walk;
{
    AmEmpty();			/* No headerfields in use yet		*/
    for (; walk != NULL; walk = walk->il_next) {
	if (walk->il_attr->at_hmem != NULL) {
	    AmOk(walk->il_attr->at_hmem, walk->il_type,
		walk->il_bits & (AT_REQUEST|AT_REPLY));
	    /*
	     *	Redeclare in server if the types are not equivalent,
	     *	but only big enough, or it is a capability.
	     *	This analysis cannot be done by ServerAnalysis(),
	     *	because the "am_almost" flag has been overwritten many
	     *	times in between. Therefor, the client-routine has
	     *	to clear this bit (DeclList looks at it).
	     */
	    if (am_almost /* && (walk->il_bits & AT_VAR) */ )
		walk->il_bits |= AT_REDECL;
	}
    }
} /* CheckHeadHints */

/*
 *	Locate headerfields wherever possible.
 *	The op_args field is not yet filled, and op_order is not
 *	ours. If there are any ints, floats and the like in the
 *	buffer, the clientstub passes a bitfield in the header
 *	indicating how the server should respond. If this is the
 *	case, set op->op_ailword to the name of the choosen field.
 */
/* static */ void
FillHeader(op, walk)
    struct optr *op;
    struct itlist *walk;
{
    int sens = 0;
    int ailwordsaved = 0;

    for (; walk != NULL; walk = walk->il_next) {
	struct typdesc *type;
	type = walk->il_type;
	if (walk->il_attr->at_hmem == NULL) {	/* Not yet allocated	*/

	    /*
	     *	Try finding a headerfield unless it would
	     *	eat the last in-int and we need an ailword.
	     *	Means we have to scan the rest of the arguments
	     *	if am_leftinint==1!
	     */
#if 0
	    if (type->td_kind == tk_int && am_leftinint == 1 &&

	    /* XXXX Fix by Jack */
	    if ( AmComp(type, ModifTyp(tk_int, 1, 0, -1)) && am_leftinint == 1 &&
#else	    /*   New fix by Jelke */
	    if (am_leftinint == 1 && SameType(type)->td_kind == tk_int &&
#endif
	      (walk->il_bits & AT_REQUEST)) {
		/* Scan the rest of the args	*/
		struct itlist *ahead;
		for (ahead=walk->il_next; ahead!=NULL; ahead=ahead->il_next) {
		    if (ahead->il_type->td_bits & (TD_CONTINT|TD_CONTFLT))
			ailwordsaved++;
		}
		if (ailwordsaved) {
		    /* Reserve an ailword before it's too late	*/
		    op->op_ailword =
			AmWhere(ModifTyp(tk_int, 0, 1, -1), AT_REQUEST);
		    if (op->op_ailword == NULL) {
			fatal("BUG: no headerfield for ailword left\n");
		    }
		}
	    }
	    /* If all is safe (don't spoil the last in-int), call AmWhere */
	    if (type->td_kind != tk_int || am_leftinint > 0)
		walk->il_attr->at_hmem = AmWhere(walk->il_type,
		    walk->il_bits & (AT_REQUEST|AT_REPLY));
	    /* Did it work?	*/
	    if (walk->il_attr->at_hmem != NULL) {
		/*
		 *	ReDecl means that the server should redeclare this
		 */
		if (am_almost) walk->il_bits |= AT_REDECL;
	    } else {
		/*
		 *	We cannot put it in the header, meaning that
		 *	the Amoeba kernel won't do magic things for us.
		 *	We'll have to ship an ailword. Or..?
		 */
		if (type->td_bits & TD_CONTFLT) sens |= OP_FLTSENS;
		if (type->td_bits & TD_VARSIZE) sens |= OP_VARSENS;
		if (type->td_bits & TD_CONTINT) sens |= OP_INTSENS;
#if 0
This does not work:
		/*
		 *	Try not to set OP_INTSENS.
		 *	Three ints can be put in the header safely, so
		 *	it doesn't always make sense to ship an ailword.
		 *	Ints that travel in the buffer require an ailword
		 *	anyway.
		 *	AmWhere keeps track of how many ints are left in
		 *	the header in am_leftinint.
		 */
		if (type->td_bits & TD_CONTINT) {
		    if ((type->td_bits & TD_VARSIZE) || 
			(walk->il_bits & AT_REQUEST) && am_leftinint == 1) {
			sens |= OP_INTSENS;
		    }
		}
#endif
		/* Did we just set the first sens flag?	*/
		if (sens && op->op_ailword == NULL) {
		    /* Reserve an ailword before it's too late	*/
		    op->op_ailword =
			AmWhere(ModifTyp(tk_int, 0, 1, -1), AT_REQUEST);
		    if (op->op_ailword == NULL) {
			fatal("BUG: no headerfield for ailword left\n");
		    }
		}
	    }
	}
    }
    op->op_flags |= sens;	/* Set new flags	*/
} /* FillHeader */

/*
**	Locate an ailword if an argument in walk contains an integer
**	or a float.
*/
static void
IntDepend(op, walk)
    struct optr *op;
    struct itlist *walk;
{
    struct typdesc *type;
    int sens = 0;

    for (; walk != NULL; walk = walk->il_next) {
	type = walk->il_type;
    	if (type->td_bits & TD_CONTINT) sens |= OP_INTSENS;
    	if (type->td_bits & TD_CONTFLT) sens |= OP_FLTSENS;
	if (sens && op->op_ailword == NULL) {
	    op->op_ailword = AmWhere(ModifTyp(tk_int, 0, 1, -1), AT_REQUEST);
    	    if (op->op_ailword == NULL) {
    	        fatal("BUG: no headerfield for ailword left\n");
    	    }
    	}
    }
    op->op_flags |= sens;
} /* IntDepend */

/*
 *	Define an operator -- called by yyparse().
 *
 *	Overall layout:
 *	- Create the descriptor, initialise
 *	- Traverse the arguments to compute the sizes of both RPC
 *	  messages. This distinguishes between fixed sizes and
 *	  dynamic sizes. Mark character arrays. If any size is unknown,
 *	  the operator is marked non-generatable. No errormessage
 *	  is generated, since it might be generated by a programmer.
 *
 *	- If it is generatable, the RPC messages are constructed:
 *	  first we split the list in a fixed-size and a dynamic-size
 *	  part. Within these parts, in-out parameters are moved to
 *	  the front. Then the headerfield hints are verified, and
 *	  the remaining header fields are assigned, and the lists
 *	  are re-joined.
 *
 *	- Then the types of VAR-parameters are patched, and lonely
 *	  parameters are marked as such for optimisation purposes.
 */
void
DefOptr(name, value, args, flags, ap, rights)
    char *name;
    int value;
    struct itlist *args;
    int flags;
    struct alt *ap;		/* Alternate values		*/
{
    struct name *nm;
    struct optr *op;
    struct itlist *walk;
    struct itlist *members;	/* Members for buffer struct	*/
    struct itlist *dyn, *fixed;	/* To split the argumentlist	*/
#if 0
    struct typdesc *type;	/* Declared here: bug in gcc?	*/
#endif

    chkptr(name);
    chkptr(args);
    if ((ThisClass->cl_flags & CL_NORANGE) && (flags & OP_STARRED)) {
	mypf(ERR_HANDLE,
	    "%r attempt to create operator %s in rangeless class\n", name);
	return;
    }

    nm = NewName(name, CALL);
    if (nm == NULL) return;		/* Name clash	*/
    if (flags & OP_STARRED) {
	/* Check or choose the value */
	if (value == 0) value = GetOptrVal();
	CheckOptrVal(value);
    } else {
	/* e.g: optr = 5 ();	*/
	if (value != 0)
	    mypf(ERR_HANDLE,
    "%r attempt to define non-RPC function with command value %d\n", value);
    }

    CheckHeadHints(args);		/* Verify & reserve header par	*/
    /* Install */
    nm->nm_optr = op = new(struct optr);
    dbg4("DefOptr(%s): name at %p, op at %p\n", name, nm, op);
    op->op_name = name;
    op->op_val = value;
    op->op_client = op->op_server = NULL;
    op->op_ailword = NULL;		/* No need for machine info yet	*/
    op->op_flags = flags;		/* Read & written by FillHeader	*/
    op->op_order = CopyItList(args);	/* Remember actual definition	*/
    /* Just to make sure	*/
    op->op_alt = ap;
    op->op_rights = rights;

    /*
     *	Reorder:
     */
    SplitFixDyn(args, &fixed, &dyn);	/* Put variadic args at the end	*/
    dyn = InOutFirst(dyn);
    fixed = InOutFirst(fixed);		/* To get these in the header	*/
    /*
    **	Locate an ailword if the dynamic part contains an integer.
    */
    IntDepend(op, dyn);
    FillHeader(op, fixed);		/* Locates the headerfields	*/
    assert((op->op_flags & OP_SENS) ^ (op->op_ailword == NULL));

    /*
     *	Join dyn and fixed, and put the result back in op->op_args.
     */
    if (dyn == NULL) {
	op->op_args = fixed;
    } else if (fixed == NULL) {
	op->op_args = dyn;
    } else {
	/* Find end of fixed part	*/
	for (walk = fixed; walk->il_next != NULL; walk = walk->il_next)
	    ;
	walk->il_next = dyn;
	op->op_args = fixed;
    }

    /*
     *	Construct the structure definition for request and reply
     *	messages. This calculates the required buffer sizes as a
     *	"side effect" (on purpose!)
     *	Also update the alignment administration for ThisClass
     */
    members = CopyIts(op->op_args, AT_REQUEST);
    op->op_in = DefSU(tk_struct, (char *) NULL, members);
    members = CopyIts(op->op_args, AT_REPLY);
    op->op_out = DefSU(tk_struct, (char *) NULL, members);
    if (flags & OP_STARRED) {
	int algn = op->op_in->td_align > op->op_out->td_align ?
		op->op_in->td_align : op->op_out->td_align;
	if (ThisClass->cl_align < algn) ThisClass->cl_align = algn;
    }

    /*
     *	Assign to op->op_[io][pb] fields.
     */
    op->op_ih = op->op_oh = op->op_ib = op->op_ob = 0;
    for (walk = op->op_args; walk != NULL; walk = walk->il_next) {
	if (walk->il_attr->at_hmem == NULL) {
	    if (walk->il_bits & AT_REQUEST) ++op->op_ib;
	    if (walk->il_bits & AT_REPLY) ++op->op_ob;
	} else {
	    if (walk->il_bits & AT_REQUEST) ++op->op_ih;
	    if (walk->il_bits & AT_REPLY) ++op->op_oh;
	}
    }

#if 0
    /*
     *	Check the dynamic array-bounds expressions,
     *	select character arrays.
     */
    if (op->op_flags & OP_STARRED) {
	for (walk = dyn; walk != NULL; walk = walk->il_next) {
	    assert(walk->il_attr->at_hmem == NULL);
	    assert(walk->il_type->td_kind == tk_arr);
	    CheckExpr(fixed, walk->il_attr->at_act, walk->il_bits);
	}
    }
#endif

    /*
     *	Patch type of VAR parameters, discover problems
     *	with ansi prototypes vs. traditional C.
     */
    for (walk = op->op_args; walk != NULL; walk = walk->il_next) {
	if (walk->il_bits & AT_VAR) {
	    walk->il_type = RefTo(walk->il_type);
	} else {
	    if (ArgWiden(walk->il_type)) op->op_flags |= OP_INCOMPAT;
	}
    }
} /* DefOptr */
