/*	@(#)pars.y	1.2	94/04/07 14:37:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

%{
# define yyerror(s)	MyError(yystate, yychar)
/*	# define YYDEBUG	*/
# define PARSER
# include "ail.h"
# include "gen.h"

/*
 *	Used when parsing soft types:
 *	every call to the symboltable we still have to do is pushed
 *	on a stack. The bottom of the stack is marked with a '_'.
 *	The stack is popped by DeclTyp().
 */
struct {
    int st_func;		/* '*', '[' ']' or '(' for vi )	*/
    struct etree *st_max;	/* Used if st_func in '[' ']'	*/
    struct etree *st_actual;	/* Only if st_func == ']'	*/
} ststack[30], *stptr = &ststack[0];
# define RstMod()	typ = tk_dunno; long_cnt = short_cnt = sign = 0;
# define MarkTst()	(stptr++)->st_func = '_'

/*
 *	Modifier-stuff. 'int', 'char' and the like are modifiers too.
 */
static unsigned long_cnt, short_cnt;
static int sign, typ;
static struct typdesc *LastSimple;
/* Compiled by ClassRange, consumed by ClassDef:	*/
static int RangeAv, RangeLo, RangeHi;

static Ecnt;	/* Last value for enumerators. */

# ifdef YYDEBUG
extern yydebug;
# endif

/*
 *	Inheritance- and include- lists (used by class definitions.
 *	They are created by the parser, and then shipped to DefClass.
 */
static struct clist *inh_list;
static struct stind *incl_list;

/*
 *	Used by argument definition stuff:
 *	The attributes of an operand are hacked together.
 *	I don't know how to do this clean (i.e. without
 *	all these global variables).
 */
static struct attr attributes;
static int In_An_ArgDef = 0;	/* Some types are not allowed outside	*/
static int optr_val;		/* 0 if value must be computed by ail	*/

/*
 *	These have to do with marshaling
 */
static char *clm, *clu, *svm, *svu;	/* Marshal-function names	*/
static struct etree *t_asize, *t_msize;	/* Actual and maximum sizes	*/

%}

/*
 * The keywords, sorted in ascii-order. (KeywLookup depends on this)
 */
%token CHAR CLASS CONST DOUBLE ENUM EXTERN FLOAT GENERATE
%token IN INCLUDE INHERIT INT LONG MARSHAL OUT RIGHTS SHORT
%token SIGNED /* STRING */ STRUCT TYPEDEF UNION UNSIGNED VAR VOID WITH
/* Miscellaneous tokens: */
%token IDENT INTCONST STRCONST LEFT RIGHT CALL
%token AND OR EQUAL UNEQU LEQ GEQ DOUBLE_DOT

%union {
    long		t_int;
    char		*t_ptr;
    struct typdesc	*t_typ;
    struct itlist	*t_itl;
    struct etree	*t_etr;
    struct gen		*t_gen;
    struct m_name	*t_mar;
    struct alt		*t_alt;
    struct name		*t_nam;
}

%type <t_etr>	expr expr1 expr2 expr3 expr4 expr5 expr6 expr7
%type <t_etr>	expr8 expr9 expr10 expr99 FuncCall exprlist GenVal
%type <t_etr>	SizeActOrMax
%type <t_int>	INTCONST ConstExpr OpRights RightsList
%type <t_ptr>	IDENT SoftPart SoftPart2 STRCONST OptId
%type <t_typ>	SimpleType SueRef TdefInfo GenType
%type <t_itl>	ArgDef ArgDefs ArgPiece
%type <t_itl>	Members MemberList SuMembers
%type <t_gen>	GenArg GenArgs
%type <t_mar>	MarshInfo
%type <t_alt>	OpValues OpValList
%type <t_nam>	Enumerator EMembers Enumerators

%%

/************************\
 *			*
 *	Major syntax	*
 *			*
\************************/
AnyThings
    : /* Empty */
    | AnyThings AnyThing
    ;
AnyThing
    : ClassDef ';'
    | ConstDef ';'
    | Impl ';'
    | Tdef ';'
    | MarshDef ';'
    | error		{ sxsync(); }
    ;

/****************************************\
 *					*
 *	These rules tell ail to		*
 *	actually generate code		*
 *					*
\****************************************/
Impl
    : GENERATE OptId
	{ StartGenerate($2); }
      '{' Generators '}'
	{ StopGenerate(); }
    ;
Generators
    : Generators ';' Generator
    | Generator
    ;

Generator
    : /* Empty */
    | IDENT
	{ Generate($1, (struct gen *) NULL); }
    | IDENT '(' GenArgs ')'
	{ Generate($1, $3); }
    ;
GenArgs
    : /* Empty */
	{ $$ = NULL; }
    | GenArg
	{ $$ = $1; }
    | GenArgs ',' GenArg
	{ $$ = $3; $3->gn_next = $1; }
    ;
GenArg
    : IDENT { RstMod(); } GenType GenVal
	{ register struct gen *gp;
	    gp = new(struct gen);
	    gp->gn_next = NULL;
	    gp->gn_name = $1;
	    gp->gn_type = $3;
	    gp->gn_value = $4;
	    $$ = gp;
	}
    ;
GenType
    : /* Empty */
	{ $$ = NULL; }
    | ':' SimpleType
	{ $$ = $2; }
    ;
GenVal
    : /* Empty */
	{ $$ = NULL; }
    | '=' expr
	{ $$ = $2; }
    ;

/****************************************\
 *					*
 *	Class definition rules.		*
 *					*
\****************************************/
ClassRange
    : /* Empty */
	{ RangeAv = 0; }
    | '[' ConstExpr DOUBLE_DOT ConstExpr ']'
	{ RangeAv = 1; RangeLo = $2; RangeHi = $4; }
    ;
ClassDef
    : CLASS IDENT ClassRange '{'
	{ inh_list = NULL; incl_list = NULL; }
      InhAndIncls
	{
	    DefClass($2, RangeAv, RangeLo, RangeHi, inh_list, incl_list);
	}
      Defs '}'
	{
	    CloseClass();
	}
    ;

InhAndIncls
    : /* Empty */
    | InhAndIncls InhAndIncl
    ;
InhAndIncl
    : INCLUDE IncludeList ';'
    | INHERIT InheritedClasses ';'
    ;
InheritedClasses
    : InheritedClasses ',' IDENT
	{ InheritClass($3); }
    | IDENT
	{ InheritClass($1); }
    ;
IncludeList
    : STRCONST
	{ IncludeString($1); }
    | IncludeList ',' STRCONST
	{ IncludeString($3); }
    ;

Defs	/* Definition within a class	*/
    :	/* Empty */
    | Defs Def
    ;
Def
    : /* Nearly-Empty */ ';'
    | ConstDef ';'
    | OpDef ';'
    | Tdef ';'
    | Rdef ';'
    | MarshDef ';'
    | error	{ sxsync(); }
    ;
ConstDef
    : CONST IDENT '=' ConstExpr
	{
	    struct typdesc *tp;
	    tp = ModifTyp(tk_int, 0, 0, 0);
	    SetIConst($2, $4, tp);
	}
    | CONST IDENT '=' STRCONST
	{
	    SetSConst($2, $4);
	}
    ;

/********************************\
 *				*
 *	Operator stuff		*
 *				*
\********************************/

OpValList
    : ConstExpr			/* Principle value */
	{ optr_val = $1; $$ = NULL; }
    | OpValList ',' ConstExpr	/* Alternate values */
	{   struct alt *ap;
	    ap = new(struct alt);
	    ap->alt_int = $3;
	    ap->alt_next = $1;
	    $$ = ap;
	}
    ;
OpValues
    : /* Empty */		{ $$ = NULL; optr_val = 0; }
    | '[' OpValList ']'		{ $$ = $2; }
    ;
RightsList
    : IDENT			{ $$ = GetRight($1); }
    | RightsList ',' IDENT	{ $$ |= GetRight($3); }
    ;
OpRights
    : /* Empty */		{ $$ = 0; }
    | RIGHTS RightsList		{ $$ = $2; }
    ;
OpDef
    : IDENT OpValues '(' '*' ',' ArgDefs ')' OpRights
	{ DefOptr($1, optr_val, $6, OP_STARRED, $2, $8); }
    | IDENT OpValues '(' '*' ')' OpRights
	{ DefOptr($1, optr_val, (struct itlist*) NULL, OP_STARRED, $2, $6); }
    | IDENT OpValues '(' ArgDefs ')' OpRights
	{ DefOptr($1, optr_val, $4, 0, $2, $6); }
    | IDENT OpValues '(' ')' OpRights
	{ DefOptr($1, optr_val, (struct itlist *) NULL, 0, $2, $5); }
    ;

ArgDefs
    : ArgDefs ',' ArgDef	{ $$ = itcat($1, $3); }
    | ArgDef			{ $$ = $1; }
    ;
ArgDef
    : { RstMod(); }  Attribs FieldSpec ArgPiece
	{
	    if ($4 == NULL) $$ = NULL;	/* Syntax error somewhere	*/
	    else {
		$4->il_bits = attributes.at_bits;
		($$ = $4)->il_attr = new(struct attr);
		*($$->il_attr) = attributes;
	    }
	    In_An_ArgDef = 0;	/* No longer busy with an argdef	*/
	}
    ;

Attribs
    : { attributes.at_bits = 0; }
	VarAttr InAttr OutAttr
      {
	  In_An_ArgDef = 1;
	  attributes.at_act = NULL;
	  /* Set AT_REQUEST if neither of AT_IO are set: */
	  if ((attributes.at_bits & (AT_REPLY|AT_REQUEST)) == 0)
	      attributes.at_bits |= AT_REQUEST;
      }
    ;
VarAttr
    : /* Empty */
    | VAR
	{ attributes.at_bits |= AT_VAR; }
    ;
InAttr
    : /* Empty */
    | IN
	{ attributes.at_bits |= AT_REQUEST; }
    ;
OutAttr
    : /* Empty */
    | OUT
	{ attributes.at_bits |= AT_VAR | AT_REPLY; }
    ;

FieldSpec
    : /* Empty */	{ attributes.at_hmem = NULL; }
    | ':' IDENT		{ attributes.at_hmem = $2; }
    ;

ArgPiece	/* The part of an argument without attributes	*/
    : SimpleType { MarkTst(); } SoftPart
	{
	    if ($3[0] == '_') {
		mypf(ERR_HANDLE, "%w: Questionable argumentname %s\n", $3);
	    }
	    $$ = it($3, DeclTyp($1));
	}
    | error		{ sxsync(); $$ = NULL; }
    ;

/********************************\
 *				*
 *	Marshaling.		*
 *				*
\********************************/
RequestInfo
    : /* Empty	*/
	{ clm = svu = NULL; }
    | IN OptId ',' OptId
	{ clm = $2; svu = $4; }
    ;
ReplyInfo
    : /* Empty	*/
	{ clu = svm = NULL; }
    | OUT OptId ',' OptId
	{ clu = $2; svm = $4; }
    ;
SizeActOrMax
    : /* Empty */
	{ $$ = NULL; }
    | expr
	{ $$ = $1; }
    ;
SizeInfo
    : /* Empty	*/
	{ t_asize = t_msize = (struct etree *) NULL; } 
/*	This creates shift/reduce conflicts
    | SizeActOrMax
	{ t_asize = t_msize = $1; }
*/
    | SizeActOrMax ':' SizeActOrMax
	{ t_asize = $1; t_msize = $3; }
    ;

MarshInfo
    : WITH SizeInfo RequestInfo ReplyInfo
	{   /* Compile all info in one descriptor	*/
	    $$ = new(struct m_name);
	    $$->m_clm = clm;
	    $$->m_clu = clu;
	    $$->m_svm = svm;
	    $$->m_svu = svu;
	}
    ;
MarshDef
    : MARSHAL IDENT MarshInfo
	{
	    struct typdesc *type;
	    type = IsTD($2);
	    if (type == NULL) {
		mypf(ERR_HANDLE, "%r type %s not defined\n", $2);
		type = &ErrDesc;
	    } else {
		;
	    }
	    RecordMarshInfo(type, $3, t_asize, t_msize, Yes);
	}
    ;

/************************\
 *			*
 *	Types.		*
 *			*
\************************/
Tdef
    : SueRef /* It is legal to say "struct foo;" in the middle of nowhere */
    | TYPEDEF { RstMod(); MarkTst(); } TdefInfo
    ;
TdefInfo
    : SimpleType SoftPart
	{ (void) NewTD($2, DeclTyp($1)); $$ = $1; }
    | TdefInfo ',' { MarkTst(); } SoftPart
	{ (void) NewTD($4, DeclTyp($1)); $$ = $1; }
    ;
SimpleType
    : Modifiers		{ $$ = ModifTyp(typ,long_cnt,short_cnt,sign); }
    | SueRef		{ $$ = $1; }
/*
    | STRING		{ $$ = &StrDesc; }
*/
    | IDENT
	{
	    $$ = IsTD($1);
	    if ($$ == NULL) {
		mypf(ERR_HANDLE, "%r unknown type %s\n", $1);
		$$ = &ErrDesc;
	    }
	}
    | error		{ sxsync(); $$ = &ErrDesc; }
    ;
OptId
    : /* Empty	*/
	{ $$ = NULL; }
    | IDENT
	{ $$ = $1; }
    ;

RightDefs
    : RightDefs ',' IDENT '=' ConstExpr
	{ SetRight($3, (unsigned) $5); }
    | IDENT '=' ConstExpr
	{ SetRight($1, (unsigned) $3); }
    ;
Rdef
    : RIGHTS RightDefs
    ;

/*** Standard types. ***/
Modifiers
    : Modifier
    | Modifiers Modifier
    ;
Modifier
    : LONG		{ ++long_cnt; }
    | DOUBLE		{ m_set(tk_double); }
    | SHORT		{ ++short_cnt; }
    | UNSIGNED		{
			    if (sign) {
				mypf(ERR_HANDLE, "%r Sign specified twice\n");
				typ = tk_err;
			    } else sign = 1;
			}
    | SIGNED		{
			    if (sign) {
				mypf(ERR_HANDLE, "%r Sign specified again\n");
				typ = tk_err;
			    } else sign = -1;
			}
    | INT		{ m_set(tk_int); }
    | FLOAT		{ m_set(tk_float); }
    | CHAR		{ m_set(tk_char); }
    | VOID		{ m_set(tk_void); }
    ;

/*** C's way of creating pointer- function- and arraytypes ***/
SoftPart
    : '*' SoftPart	{ (stptr++) -> st_func = '*'; $$ = $2; }
    | SoftPart2		{ $$ = $1; }
    ;
SoftPart2
    : SoftPart2 Array	{ $$ = $1; }
    | SoftPart2 '(' ')' { (stptr++)->st_func = '('; $$ = $1; /*)*/ }
    | '(' SoftPart ')'	{ $$ = $2; }
    | IDENT		{ $$ = $1; }
    ;
Array
    : '[' ']'
	{
	    stptr->st_max = (struct etree *) NULL;
	    stptr->st_actual = (struct etree *) NULL;
	    (stptr++)->st_func = '['; /* Fool vi: ] */
	}
    | '[' expr ']'
	{
	    /* Use actual size as maximum as well (we DO know it!)	*/
	    stptr->st_max = stptr->st_actual = $2;
	    (stptr++)->st_func = '['; /*]*/
	}
    | '[' expr ':' expr ']'
	{
/* This check should be done somewhere else
	    if (!In_An_ArgDef) {
		mypf(ERR_HANDLE, "%r open array outside proc definition\n");
		$4 = 0;
	    }
*/
	    stptr->st_max = $4;
	    stptr->st_actual = $2;
	    (stptr++)->st_func = '[';	/*]*/
	}
    ;

/*** Structures, unions, enums. ***/
SueRef
    : STRUCT IDENT SuMembers	{ $$ = DefSU(tk_struct, $2, $3); }
    | UNION IDENT SuMembers	{ $$ = DefSU(tk_union, $2, $3); }
    | ENUM IDENT EMembers	{ $$ = DefEnum($2, $3); }
    | STRUCT SuMembers		{ $$ = DefSU(tk_struct, (char *)NULL, $2); }
    | UNION SuMembers		{ $$ = DefSU(tk_union, (char *)NULL, $2); }
    | ENUM EMembers		{ $$ = DefEnum((char *) NULL, $2); }
    | STRUCT IDENT		{ $$ = FindSUE(tk_struct, $2); }
    | UNION IDENT		{ $$ = FindSUE(tk_union, $2); }
    | ENUM IDENT		{ $$ = FindSUE(tk_enum, $2); }
    ;
Enumerator
    : IDENT
	{
	    $$ = new(struct name);
	    $$->nm_next = NULL;
	    $$->nm_name = $1;
	    $$->nm_kind = INTCONST;
	    $$->nm_ival = ++Ecnt;
	}
    | IDENT '=' ConstExpr
	{
	    if($3 <= Ecnt) {
		mypf(ERR_HANDLE,
		    "%r specified value for enumerator %s illegal\n", $1);
	    }
	    $$ = new(struct name);
	    $$->nm_next = NULL;
	    $$->nm_name = $1;
	    $$->nm_kind = INTCONST;
	    $$->nm_ival = Ecnt = $3;
	}
    ;
Enumerators
    : Enumerators ',' Enumerator
	{
	    $3->nm_next = $1; /* Note: wrong order */
	    $$ = $3;
	}
    | { Ecnt = -1; } Enumerator			{ $$ = $2; }
    ;
EMembers
    : '{' Enumerators '}'			{ $$ = $2; }
    ;
SuMembers
    : '{' MemberList '}'			{ $$ = $2; }
    ;
MemberList
    : MemberList { RstMod(); } Members ';'	{ $$ = itcat($1, $3); }
    | /* Empty */				{ $$ = NULL; }
    ;
Members
    : SimpleType { LastSimple = $1; MarkTst(); } SoftPart
	{ $$ = it($3, DeclTyp($1)); }
    | Members ',' { MarkTst(); } SoftPart
	{ $$ = itcat($1, it($4, DeclTyp(LastSimple))); }
    | error	{ sxsync(); $$ = NULL; }
    ;

/************************\
 *			*
 *	expressions	*
 * (I don't like %prec)	*
 *			*
\************************/
ConstExpr
    : expr		{ $$ = GetVal($1); }
    ;
expr
    : error		{ sxsync(); $$ = Cleaf((long) 1); }
    | expr OR expr1	{ $$ = Diadic(OR, $1, $3); }
    | expr1		{ $$ = $1; }
    ;
expr1
    : expr1 AND expr2	{ $$ = Diadic(AND, $1, $3); }
    | expr2		{ $$ = $1; }
    ;
expr2
    : expr2 '|' expr3	{ $$ = Diadic('|', $1, $3); }
    | expr3		{ $$ = $1; }
    ;
expr3
    : expr3 '^' expr4	{ $$ = Diadic('^', $1, $3); }
    | expr4		{ $$ = $1; }
    ;
expr4
    : expr4 '&' expr5	{ $$ = Diadic('&', $1, $3); }
    | expr5		{ $$ = $1; }
    ;
expr5
    : expr5 EQUAL expr6	{ $$ = Diadic(EQUAL, $1, $3); }
    | expr5 UNEQU expr6	{ $$ = Diadic(UNEQU, $1, $3); }
    | expr6		{ $$ = $1; }
    ;
expr6
    : expr6 '<' expr7	{ $$ = Diadic('<', $1, $3); }
    | expr6 '>' expr7	{ $$ = Diadic('>', $1, $3); }
    | expr6 LEQ expr7	{ $$ = Diadic(LEQ, $1, $3); }
    | expr6 GEQ expr7	{ $$ = Diadic(GEQ, $1, $3); }
    | expr7		{ $$ = $1; }
    ;
expr7
    : expr7 LEFT expr8	{ $$ = Diadic(LEFT, $1, $3); }
    | expr7 RIGHT expr8	{ $$ = Diadic(RIGHT, $1, $3); }
    | expr8		{ $$ = $1; }
    ;
expr8
    : expr8 '+' expr9	{ $$ = Diadic('+', $1, $3); }
    | expr8 '-' expr9	{ $$ = Diadic('-', $1, $3); }
    | expr9		{ $$ = $1; }
    ;
expr9
    : expr9 '*' expr99	{ $$ = Diadic('*', $1, $3); }
    | expr9 '/' expr99	{ $$ = Diadic('/', $1, $3); }
    | expr9 '%' expr99	{ $$ = Diadic('%', $1, $3); }
    | expr10		{ $$ = $1; }
    ;
expr10
    : '+' expr10	{ $$ = $2; }
    | '-' expr10	{ $$ = Monadic('-', $2); }
    | '~' expr10	{ $$ = Monadic('~', $2); }
    | '!' expr10	{ $$ = Monadic('!', $2); }
    | expr99
    ;
expr99
    : INTCONST		{ $$ = Cleaf($1); }
    | IDENT		{ $$ = Ileaf($1, Yes); }
    | '@'		{ $$ = Sleaf();	}
    | '(' expr ')'	{ $$ = $2; }
    | expr99 '.' IDENT	{ $$ = Diadic('.', $1, Ileaf($3, No)); }
    | FuncCall		{ $$ = $1; }
    ;

/*
 * f(a,b,c) is stored as:
 *		 CALL
 *		/    \
 *	       f      ,
 *		     / \
 *		    ,	a
 *		   / \
 *		  ,   b
 *		   \
 *		    c
 * which makes CALL a unary operator, and ',' both unary and binary.
 * Note that the ',' operator as C thinks about it is not implemented.
 */
FuncCall
    : IDENT '(' exprlist ')'	{ $$ = Diadic(CALL, Ileaf($1, No), $3); }
    ;
exprlist
    : expr			{ $$ = Monadic(',', $1); }
    | expr ',' exprlist		{ $$ = Diadic(',', $3, $1); }
    ;

%%

/*
 *	Put a class in the inh_list
 */
static void
InheritClass(id)
    char *id;	/* The name of the class	*/
{
    struct class *tmp;
    tmp = FindClass(id);
    if (tmp != NULL) {	/* Else: an error	*/
	struct clist *lp, **tailp;
	lp = new(struct clist);
	lp->cl_next = (struct clist *) NULL;
	lp->cl_this = tmp;
	/* Attach to the end	*/
	for (tailp = &inh_list; *tailp != NULL; tailp = &((*tailp)->cl_next))
		;
	*tailp = lp;
    }
} /* InheritClass */

/*
 *	Put a filename in the incl_list
 *	This list must be in the right order.
 */
static void
IncludeString(name)
    char *name;
{
    struct stind **where;
    /* Find the end of the current list:	*/
    for (where = &incl_list; *where != NULL; where = &(*where)->st_nxt)
	;
    *where = new(struct stind);
    (*where)->st_str = name;
    /* st_len not used	*/
    (*where)->st_nxt = NULL;
} /* IncludeString */

/*
 *	sxsync(): try to recover from a syntax-
 *	error by discarding boring tokens.
 */
/* static */ void
sxsync()
{
    static long old_n_read = -1;
    if (old_n_read != n_read) {
	/*
	 * Read something since last error.
	 * If there is just one token missing,
	 * we don't have to eat anything...
	 */
    } else {
# ifdef YYDEBUG
	if (yydebug) dbg1("Sync: skipping ");
# endif
	/* Skip at least 1 token */
	if (yychar != LEX_EOF) yychar = yylex();
	/* Vi: here's your {(; say thanks! */
	while (yychar != LEX_EOF && yychar != ';' && yychar != '}' &&
	  yychar != ')' && yychar != ',' && yychar != ']') {
# ifdef YYDEBUG
	    if (yydebug) dbg2(" %s", TokenToString(yychar));
# endif
	    yychar = yylex();
	}
	if (yychar == LEX_EOF) fatal("Can't recover");
# ifdef YYDEBUG
	if (yydebug) dbg1("\n");
# endif
	yyerrok;
    }
    old_n_read = n_read;
} /* sync */

/*
 *	Declare a type composed of a simpletype (ctyp),
 *	and the symbols from the stack.
 *	Return the produced type.
 */
/* static */ struct typdesc *
DeclTyp(ctyp)
    struct typdesc *ctyp;
{
    chkptr(ctyp);
    while  ((--stptr)->st_func != '_') { /* While stack not empty */
	switch(stptr->st_func) {
	case '[':	/*] Array	*/
	    ctyp = ArrayOf(ctyp, stptr->st_actual, stptr->st_max);
	    break;
	case '*':
	    ctyp = PointerTo(ctyp);
	    break;
	case '(':
	    ctyp = FuncOf(ctyp);
	    break;
	default:
	    fatal("There's something weird on the typestack.\n");
	}
    }
    return ctyp;
} /* DeclTyp */

/*
 *	Bind an identifier to a type.
 *	Make a list containing only this node.
 */
struct itlist *
it(name, type)
    char *name;
    struct typdesc *type;
{
    struct itlist *itp;
    chkptr(name);
    chkptr(type);
    itp = new(struct itlist);
    itp->il_next = NULL;
    itp->il_attr = NULL;
    itp->il_name = name;
    itp->il_type = type;
    return itp;
} /* It */

/*
 *	Concatnate two itlists, check for double id's.
 */
struct itlist *
itcat(l1, l2)
    struct itlist *l1, *l2;
{
    struct itlist *l1p, *tail, **l2p;
    if(l1 == NULL) return l2;
    if(l2 == NULL) return l1;
    chkptr(l1);
    chkptr(l2);
    /* Find tail of first list, checking id's in the second. */
    for(l1p = l1; l1p != NULL; l1p = l1p->il_next) {
	if(l1p->il_next == NULL) tail = l1p;	/* Found tail */
	for(l2p = &l2; *l2p != NULL; ) {	/* id != ids in l2? */
	    if((*l2p)->il_name == l1p->il_name) {
		struct itlist *hold;
		mypf(ERR_HANDLE, "%r Duplicate name in memberlist\n");
		/* Remove duplicate entry: */
		hold = *l2p;
		*l2p = (*l2p)->il_next;
		FREE(hold);
	    } else {
		/* Advance l2p: */
		l2p = &(*l2p)->il_next;
	    }
	}
    }
    tail->il_next = l2;
    return l1;
} /* itcat */

static void
m_set(n)
    int n;
{
    if(typ == tk_dunno) typ = n;
    else {
	if(typ != tk_err) mypf(ERR_HANDLE, "%r Illegal type combination\n");
	typ = tk_err;
    }
} /* m_set */

/*
 *	Say something more useful than "syntax error"
 */
static void
MyError(state, tok)
    int state, tok;
{
    extern char *Serr();
    mypf(ERR_HANDLE, "%r unexpected \"%s\"; %s",
	TokenToString(tok), Serr(state));
    if (options & OPT_DEBUG)
	mypf(ERR_HANDLE, "(dbg) state=%d, token=%d\n", state, tok);
    NoKeywords = No;	/* Otherwise it 'll never get turned off	*/
} /* MyError */

/*
 *	A fatal error has occured, stop.
 */
void
fatal(s)
    char *s;
{
    extern errno;
    extern char *error_string();
    mypf(ERR_HANDLE, "%r FATAL:%s %s\n", errno ? error_string(errno) : "", s);
    if (getenv("DUMP_CORE") != (char *) NULL) {
	if (errno) perror("Errno");
	abort();
    } else {
	exit(2);
    }
} /* fatal */
