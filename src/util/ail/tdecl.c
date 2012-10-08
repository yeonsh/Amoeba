/*	@(#)tdecl.c	1.2	94/04/07 14:39:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

/*
 *	This module emits declarations.
 *	These work quite close together:
 *	TypeSpell():	spells out a SimpleType,
 *	SoftSpell():	spells out a SoftPart,
 *	Declare "id" as a pointer to int like this:
 *	TypeSpell(type of id) spells out "int",
 *	so TypeSpell ignores the pointer-symbol.
 *	Then SoftSpell(type of id, id) spells
 *	" *id". SoftSpell also handles functions
 *	and arrays.
 */

char *
IoName(bits)
    int bits;
{
    switch (bits & AT_IO) {
    case AT_I: return "in";
    case AT_O: return "out";
    case AT_VI: return "var";
    case AT_IO: return "inout";
    default: mypf(ERR_HANDLE, "%d is a strange io-name\n", bits);
	exit(9);
    }
    /*NOTREACHED*/ /*FerSure*/
} /* IoName */

/*
 *	Spell the members of an enumerated type
 */
static void
EnumSpell(h, type)
    handle h;
    struct typdesc *type;
{
    struct itlist *p;
    long dflt_val = 0;	/* Default ordinal	*/
    assert(type->td_kind == tk_enum);
    p = type->td_meml;
    while (p != NULL) {
	struct name *conp;	/* Pointer to the constant	*/
	conp = p->il_econ;
	assert(conp->nm_kind == INTCONST);
	assert(conp->nm_type == type);
	if (conp->nm_ival != dflt_val)
	    mypf(h, "%n%s = %D", conp->nm_name, conp->nm_ival);
	else
	    mypf(h, "%n%s", conp->nm_name);
	dflt_val = conp->nm_ival + 1;
	p = p->il_next;
	if (p != NULL) mypf(h, ",");
    }
} /* EnumSpell */

/*
 *	Spell a sue. Use the "struct foo" style if
 *	it has a name, name the members otherwise.
 *	Name them also if called for definition.
 */
/* static */ void
SueSpell(h, t, def)
    handle h;
    struct typdesc *t;
    int def;	/* Called for a definition. */
{
    char *s;
    struct itlist *walk;
    switch (t->td_kind) {
    case tk_enum:	s = "enum";	break;
    case tk_struct:	s = "struct";	break;
    case tk_union:	s = "union";	break;
    default: assert(0);
    }
    if (t->td_name != NULL) {
	/* assert(def || (t->td_bits & TD_DECLED)); */
	mypf(h, "%s %s", s, t->td_name);
	if (!def) return;
    } else {
	mypf(h, "%s", s);
    }

    /* The thing has no name or is being defined; spell the members */
    mypf(h, " {%>");
    if (t->td_kind == tk_enum) {
	EnumSpell(h, t);
    } else {
	struct typdesc *LastSimple = NULL; /* Last arg to TypeSpell */
	for (walk = t->td_meml; walk != NULL; walk = walk->il_next) {

	    /*
	     *	Make a choice between using the
	     *	previously emitted SimpleType or
	     *	emitting a new one.
	     */
	    if (LastSimple == walk->il_type->td_styp) {
		mypf(h, ",");		/* Use old SimpleType */
	    } else {
		if (LastSimple != NULL) {
		    mypf(h, ";");	/* Close old list */
		}
		mypf(h, "%n");
		TypeSpell(h, LastSimple = walk->il_type->td_styp, No);
	    }

	    /*
	     *	And print the soft part + id
	     */
	    SoftSpell(h, walk->il_type, walk->il_name);
	    if (options & OPT_DEBUG)	/* And print the offset */
		mypf(h, " /*%d*/", walk->il_offs);
	}
	mypf(h, ";");
    }

    mypf(h, "%<%n}");
} /* SueSpell */

/*
 *	Spell out a SimpleType.
 *	Note that unnamed structs and
 *	unions aren't that simple...
 */
void
TypeSpell(h, t, def)
    handle h;
    struct typdesc *t;
    Bool def;	/* Used for a definition */
{
    int l; char *s;
    /* It should be a simple one. */
    assert(t->td_styp == t);
    switch(t->td_kind) {
    case tk_tdef:
	mypf(h, "%s", t->td_tname);
	break;
    case tk_void:
	mypf(h, "void");
	break;
    case tk_char:
	assert(t->td_size==0);
	switch(t->td_sign) {
	case -1: mypf(h, "signed char"); break;
	case 0:  mypf(h, "char"); break;
	case 1:  mypf(h, "unsigned char"); break;
	default: assert(0);
	}
	break;
    case tk_int:
    case tk_float:
    case tk_double:
	if (t->td_sign == 1) mypf(h, "unsigned ");
	l = t->td_size;
	if (l < 0) {
	    l = -l;
	    s = "short ";
	} else s = "long ";
	while (l--) mypf(h, s);
	switch(t->td_kind) {
	    case tk_int: s = "int"; break;
	    case tk_double: s = "double"; break;
	    case tk_float: s = "float"; break;
	    default: assert(0);
	}
	mypf(h, "%s", s);
	break;
    case tk_struct:
    case tk_union:
    case tk_enum:
	SueSpell(h, t, def);
	break;
    default:
	mypf(ERR_HANDLE, "BUG: TypeSpell kind %d\n", t->td_kind);
	assert(0);
    }
} /* TypeSpell */

/*
 *	Spell out a type/identifier combination.
 *	SoftSpell(pointer to unsigned int, dada) should print:
 *	" *dada", as the SimpleType should be spelled by
 *	TypeSpell yet. Note the leading space.
 */
void
SoftSpell(h, t, id)
    handle h;
    struct typdesc *t;
    char *id;
{
    char soft[N_SOFT], *head, *tail;
    int just_pointed = 0;
    /*
     *	Construct the softpart.
     *	If the last softconstruct was not a '*', and
     *	current one is, I need to group things with
     *	parentheses. just_pointed indicates this state.
     *	head and tail point somewhere in the middle of
     *	soft, which is the string generated.
     */
    head = &soft[sizeof(soft)/2];
    /* Copy the id, make tail point to closing '\0' */
    strcpy(head, id);
    for (tail = head; *tail != '\0'; ++tail)
	;
    /* Follow the same path as before to a SimpleType */
    for (; t != t->td_styp; t = t->td_prim) {
	/* BUG: Near-to-correct test against soft[] overflow */
	if (head < &soft[0] || tail >= &soft[sizeof(soft)-1])
	    fatal("Type too soft. Cut down please."); /* Any better message? */
	assert(t != NULL);
	switch (t->td_kind) {
	case tk_ref:
	    /* Ignore compiler generated pointers if referenced type if CPREF */
	    if (t->td_prim->td_bits & TD_CPREF) break;
	    /*FALLTHROUGH*/
	case tk_ptr:
	    just_pointed = 1;
	    *--head = '*';
	    break;
	case tk_arr:
	    if (just_pointed) {	/* Generate grouping parens */
		*--head = '(';
		*tail++ = ')';
		just_pointed = 0;
	    }
	    if (t->td_bound != NULL && t->td_bound->et_const) {
		sprintf(tail, "[%ld]", (int) t->td_bound->et_val);
		do ++tail; while (*tail != '\0');
	    } else {
		*tail++ = '['; *tail++ = ']';
	    }
	    break;
	case tk_func:
	    if (just_pointed) {	/* Generate grouping parens */
		*--head = '(';
		*tail++ = ')';
		just_pointed = 0;
	    }
	    *tail++ = '(';
	    *tail++ = ')';
	    break;
	case tk_copy:
	    /*
	     *	tk_copy is generated by ServerAnalysis.
	     *	A copy-of-ref-to-x is special: it must be
	     *	declared as x in the serverstub.
	     */
	    if (t->td_prim->td_kind == tk_ref)
		t = t->td_prim;	/* To be skipped before next iteration	*/
	    break;
	default:
	    assert(0);
	}
    }
    *tail = '\0';
    *--head = ' ';	/* The leading space */
    mypf(h, head);
} /* SoftSpell */

/*
 *	Emit a type-cast.
 *	One cannot cast to array or functions.
 *	So we must strip all typedefs.
 */
void
Cast(h, type)
    handle h;
    struct typdesc *type;
{
    mypf(h, "(");
    while (type->td_kind == tk_tdef || type->td_kind == tk_copy)
	type = type->td_prim;
    if (type->td_kind == tk_arr) type = PointerTo(type->td_prim);
    else if (type->td_kind == tk_func) type = PointerTo(type);
    assert(type->td_kind != tk_func);
    TypeSpell(h, type->td_styp, No);
    SoftSpell(h, type, "");
    mypf(h, ")");
} /* Cast */

/*
 *	Declare a type if it belongs to this class.
 *	The only types that can be declared in C are typedefs and sues.
 */
void
Tdecl(h, type)
    handle h;
    struct typdesc *type;
{
    struct itlist *walk;
    if ((type->td_bits & (TD_CONTXT|TD_DECLED)) ||
	(type->td_parent != ThisClass)) {
	    return;
    }
    dbg2("Declaring %p\n", type);
    /* Mark this one as declared: */
    type->td_bits |= TD_DECLED;
    switch (type->td_kind) {
    case tk_struct:
    case tk_union:
    case tk_enum:
	for (walk = type->td_meml; walk != NULL; walk = walk->il_next)
	    Tdecl(h, walk->il_type);
	assert(type->td_name != NULL);
	SueSpell(h, type, 1);
	break;
    case tk_tdef:
	Tdecl(h, type->td_prim);
	mypf(h, "typedef ");
	TypeSpell(h, type->td_prim->td_styp, No);
	SoftSpell(h, type->td_prim, type->td_tname);
#if 0
	/* See if there are any other typedefs with the same basetype	*/
	for (ahead = type->td_next; ahead != NULL; ahead = ahead->td_next) {
	    if (ahead->td_kind == tk_tdef && !(ahead->td_bits & TD_DECLED) &&
		ahead->td_parent == ThisClass &&
		ahead->td_prim->td_styp == ahead->td_prim->td_styp) {
		    /* Found one. */
		    ahead->td_bits |= TD_DECLED;
		    mypf(h, ",");
		    SoftSpell(h, ahead->td_prim, ahead->td_tname);
	    }
	}
#endif
	break;
    case tk_func:
    case tk_arr:
    case tk_ptr:
	mypf(ERR_HANDLE, "%d: ", type->td_kind);
	assert(0);
    default:
	assert(0);	/* Ints and the lot need not be declared */
    }
    /* Terminate declaration(s): */
    mypf(h, ";\n\n");
} /* Tdecl */

/*
 *	Declare all types in a scope, return whether you did something.
 */
Bool
ScopeType(h, scope)
    handle h;
    struct name *scope;
{
    Bool did_something = No;
    UnDeclTypes(); /* Mark all our types as not declared */
    for (; scope != NULL; scope = scope->nm_next) {
	switch (scope->nm_kind) {
	case TYPEDEF:
	case STRUCT:
	case UNION:
	case ENUM:
	    did_something = Yes;
	    Tdecl(h, scope->nm_type);
	    break;
	}
    }
    return did_something;
} /* ScopeType */

/*
 *	Declare some variables.
 */
void
DeclList(h, itt)
    handle h;
    struct itlist *itt;
{
    while (itt != NULL) {
	assert(itt->il_attr != NULL);
	if (itt->il_bits & AT_NODECL) {
	    mypf(h, "%n/* %s %s ", IoName(itt->il_bits), itt->il_name);
	    if (itt->il_attr->at_hmem != NULL)
		mypf(h, "(%s) ", itt->il_attr->at_hmem);
	    mypf(h, "not declared */");
	} else {
	    mypf(h, "%n");
	    TypeSpell(h, itt->il_type->td_styp, No);
	    SoftSpell(h, itt->il_type, itt->il_name);
	    mypf(h, "; /* %s", IoName(itt->il_bits));
	    if (itt->il_bits & AT_REDECL) mypf(h, " redeclared");
	    if (itt->il_attr->at_hmem != NULL)
		mypf(h, " (%s)", itt->il_attr->at_hmem);
	    mypf(h, " */");
	}
	itt = itt->il_next;
    }
} /* DeclList */
