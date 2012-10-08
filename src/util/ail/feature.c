/*	@(#)feature.c	1.2	94/04/07 14:32:51 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include "gen.h"

static f_ready = 0;
static char	/* Identifiers recognized by this generator:	*/
	*s_def_inh,	/* Defined inherited class names	*/
	*s_eff_inh,	/* Effectively inherited classes	*/
	*s_nifty;

static void
f_init()
{
    s_def_inh = StrFind("def_inh");
    s_eff_inh = StrFind("eff_inh");
    s_nifty = StrFind("nifty");
} /* f_init */


	/****************************************\
	 *					*
	 *	Enumerated type support		*
	 *					*
	\****************************************/

static void
EnumTable(h, type, plain, stic)
    handle h;
    struct typdesc *type;
    Bool plain;		/* Enum constants are like 0, 1, 2 etc	*/
    Bool stic;		/* Declare static?			*/
{
    struct itlist *walk;
    assert(type->td_kind == tk_enum);
    if (plain) {	/* Only store the strings	*/
	mypf(h, "%n%schar *tab_%s[] = {%>",
	    stic ? "static " : "", type->td_name);
	for (walk = type->td_meml; walk != NULL; walk = walk->il_next) {
	    mypf(h, "%n\"%s\"", walk->il_name);
	    if (walk->il_next != NULL) mypf(h, ",");
	}
	mypf(h, "%<%n};\n");
    } else {		/* Store strings AND ordinals	*/
	mypf(h, "%n%sstruct {%>%nenum %s _ord;%nchar *_name;%<%n} tab_%s[] = {%>",
	    stic ? "static " : "", type->td_name, type->td_name);
	for (walk = type->td_meml; walk != NULL; walk = walk->il_next) {
	    mypf(h, "%n%s, \"%s\"", walk->il_name, walk->il_name);
	    if (walk->il_next != NULL) mypf(h, ",");
	}
	mypf(h, "%<%n};\n");
    }
} /* EnumTable */

/*
 *	Generate function that takes as argument a string,
 *	and returns the ordinal for some enumerated type,
 *	i.e, for type
 *		enum cijfer { een, twee, drie };
 *	generate a function that performs the coversion
 *	"twee" -> (enum cijfer) 2
 */
static void
enum_ord(h, type, plain, local)
    handle h;
    struct typdesc *type;
    Bool plain;		/* Ordinals are simply 0, 1, 2, etc	*/
    Bool local;		/* Declare an interal table		*/
{
    /* Header of the function:	*/
    mypf(h, "%n/*%n *\tOrdinal computation for enum %s by ail.%n */",
	type->td_name);
    switch (lang) {
    case L_ANSI:
	mypf(h, "\nenum %s\nord_%s(char *s)%n{%>", /* } */
	    type->td_name, type->td_name);
	break;
    case L_TRADITIONAL:
	mypf(h, "\nenum %s\nord_%s(s)%>%nchar *s;\n{", /* } */
	    type->td_name, type->td_name, type->td_name);
	break;
    default:
	mypf(ERR_HANDLE, "%r Cannot generate ord function in this language\n");
	return;
    }

    /* Declaration of a loop variable and the stringtable we need:	*/
    mypf(h, "%nint i;");
    if (local) EnumTable(h, type, plain, Yes);

    /* The body:	*/
    mypf(h, "%nfor (i = sizeof(tab_%s)/sizeof(tab_%s[0]); i--; )%>",
	type->td_name, type->td_name);
    if (plain) {
	mypf(h, "%nif (strcmp(s, tab_%s[i]) == 0)%>%nreturn (enum %s) i;%<%<",
	    type->td_name, type->td_name);
    } else {
	mypf(h, "%nif (strcmp(s, tab_%s[i]._name) == 0)%>", type->td_name);
	mypf(h, "%nreturn tab_%s[i]._ord;%<%<", type->td_name);
    }
    /* { */
    mypf(h,
    "%nreturn (enum %s) (((int) %s) - 1); /* Not found */%<%n} /* ord_%s */\n",
	type->td_name, type->td_meml->il_name, type->td_name);
} /* enum_ord */

/*
 *	Generate the reverse function, i.e. 2 -> "twee"
 */
static void
enum_str(h, type, plain, local)
    handle h;
    struct typdesc *type;
    Bool plain;
    Bool local;
{
    /* Header of the function:	*/
    mypf(h, "%n/* %n *\tReturn identifier of enum %s by ail.%n */",
	type->td_name);

    switch (lang) {
    case L_ANSI:
	mypf(h, "\nchar *\nstr_%s(enum %s ord)%n{%>", /* } */
	    type->td_name, type->td_name);
	break;
    case L_TRADITIONAL:
	mypf(h, "\nchar *\nstr_%s(ord)%>%nenum %s ord;\n{", /* } */
	    type->td_name, type->td_name);
	break;
    default:
	mypf(ERR_HANDLE, "%r Cannot generate str function in this language\n");
	return;
    }

    /* Local variables	*/
    if (!plain) mypf(h, "%nint i;");
    if (local) EnumTable(h, type, plain, Yes);

    /* The body:	*/
    if (plain) {
#ifdef NO_ENUM_COMPARISON
	switch(lang) {
	case L_TRADITIONAL:
	    mypf(h,
"%nreturn ((int) ord < (int) %s || (int) ord >= sizeof(tab_%s)/sizeof(tab_%s[0])) ?%>",
	    type->td_meml->il_name,type->td_name, type->td_name, type->td_name);
	    mypf(h, "%n(char *) 0 : tab_%s[(int) ord];%<", type->td_name);
	    break;
	case L_ANSI:
	    mypf(h,
"%nreturn (ord < %s || ord >= (enum %s) sizeof(tab_%s)/sizeof(tab_%s[0])) ?%>",
	    type->td_meml->il_name,type->td_name, type->td_name, type->td_name);
	    mypf(h, "%n(char *) 0 : tab_%s[ord];%<", type->td_name);
	    break;
	default:
	    assert(0);
	}
#else
	mypf(h,
"%nreturn (ord < %s || ord >= (enum %s) sizeof(tab_%s)/sizeof(tab_%s[0])) ?%>",
	type->td_meml->il_name, type->td_name, type->td_name, type->td_name);
	mypf(h, "%n(char *) 0 : tab_%s[ord];%<", type->td_name);
#endif
    } else {
	mypf(h, "%nfor (i = sizeof(tab_%s)/sizeof(tab_%s[0]); i--; )%>",
	    type->td_name, type->td_name);
	mypf(h, "%nif (tab_%s[i]._ord == ord) return tab_%s[i]._name;%<",
	    type->td_name, type->td_name);
	/*{*/
	mypf(h, "%nreturn (char *) 0; /* Illegal value */", type->td_name);
    }
    /* { */
    mypf(h, "%<%n} /* str_%s */\n", type->td_name);
} /* enum_str */

/*
 *	This routine decides upon the format of tab_<enum-tag>,
 *	and whether to declare it locally, and does the appropiate
 *	calls to EnumTable, enum_str and enum_ord based upon what
 *	functionality is needed.
 */
void
ConvEnum(h, type, str, ord)
    handle h;
    struct typdesc *type;
    Bool str, ord;	/* Which functions we need	*/
{
    Bool plain;
    int prev;
    struct itlist *walk;

    /* Some checks	*/
    if (type->td_kind != tk_enum) {
	mypf(ERR_HANDLE, "%r not an enum tag\n");
	return;
    }
    if (type->td_name == NULL) {	/* Can this happen at all?	*/
	mypf(ERR_HANDLE, "%r enum has no tag\n");
	return;
    }
    if (!type->td_defd) {
	mypf(ERR_HANDLE, "%r enum %s not complete\n", type->td_name);
	return;
    }
    if (!str && !ord) return;	/* We don't have to do a thing	*/

    /* Compute plain	*/
    prev = -1; plain = Yes;
    for (walk = type->td_meml; walk != NULL; walk = walk->il_next) {
	assert(walk->il_econ->nm_type == type);
	if (++prev != walk->il_econ->nm_ival) {
	    plain = No;
	    break;
	}
    }

    /* Do it:	*/
    if (str && ord) {	/* Need both	*/
	EnumTable(h, type, plain, No);
	enum_str(h, type, plain, No);
	enum_ord(h, type, plain, No);
    } else {		/* One		*/
	if (str) enum_str(h, type, plain, Yes);
	else enum_ord(h, type, plain, Yes);
    }
} /* ConvEnum */

/*
 *	The generator for enum support.
 *	Generates funtions to convert between
 *	strings and their ordinal values.
 */
void
EnumOrd(p)
    struct gen *p;
{
    struct name *nm;
    struct class *clp;
    char fname[1024];
    handle h;
    sprintf(fname, "conv_%s.c", p->gn_name);
    h = OpenFile(fname);
    /* Find the enum type we're talking about */
    if (!WhereIsName(p->gn_name, &clp, &nm)) {
	mypf(ERR_HANDLE, "%r enum %s not defined\n", p->gn_name);
    } else {
	/* If globally declared, we simply repeat the definition	*/
	if (clp == &GlobClass) Tdecl(h, nm->nm_type);
	else mypf(h, "#include \"%s.h\"", clp->cl_name);
	ConvEnum(h, nm->nm_type, Yes, Yes);
    }
    CloseFile(h);
} /* EnumOrd */

/*
 *	Garbage can for code generators.
 */
void
/*ARGSUSED*/
Feature(p)
    struct gen *p;
{
    handle fp;
    char fname[1024];
    sprintf(fname, "%s_feature.c", ThisClass->cl_name);
    fp = OpenFile(fname);
    if (!f_ready) f_init();
    for (; p != NULL; p = p->gn_next) {
	if (p->gn_name == s_def_inh) {
	    register struct clist *clp;
	    mypf(fp,
	    "\nchar *def_inh[] = { /* Inherited by definition */\n");
	    for (clp= ThisClass->cl_definh; clp != NULL; clp= clp->cl_next)
		mypf(fp, "\t\"%s\",\n", clp->cl_this->cl_name);
	    mypf(fp, "\t(char *) 0\n};\n");
	} else if (p->gn_name == s_eff_inh) {
	    register struct clist *clp;
	    mypf(fp, "\nchar *eff_inh[] = { /* Effectively inherited */\n");
	    /* Own class-name as zeroth element	*/
	    mypf(fp, "\t\"%s\",\n", ThisClass->cl_name);
	    for (clp= ThisClass->cl_effinh; clp != NULL; clp= clp->cl_next)
		mypf(fp, "\t\"%s\",\n", clp->cl_this->cl_name);
	    mypf(fp, "\t(char *) 0\n};\n");
	} else if (p->gn_name == s_nifty) {
	    mypf(OUT_HANDLE, "That\'s really nifty! To try the word nifty!\n");
	    mypf(OUT_HANDLE, "Bet you read the sources to get this remark!\n");
	    mypf(OUT_HANDLE, "Or did you write them :-?\n");
	} else {
	    mypf(ERR_HANDLE, "%r %s unrecognized\n", p->gn_name);
	}
    }
    CloseFile(fp);
} /* Feature */
