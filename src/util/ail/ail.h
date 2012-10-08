/*	@(#)ail.h	1.5	96/02/27 12:42:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	General headerfile for ail -- all sources include this one
 */

# include <stdio.h>	/* Only legal place to get your NULL...		*/
# include <string.h>
/*
 *	Manifest constants -- more should be moved here
 */
# define MAX_RIGHT	256	/* Value for highest possible right: 2^8 */
# define DFLT_VERSION	V_4_0	/* Default version -- see mhdr.c	*/

# ifndef PARSER
	/* For unknown reasons, y.tab.h is duplicated on y.tab.c	*/
#	include "pars.h"
# endif

# define  assert(e)	if(!(e)) { mypf(ERR_HANDLE, "Assert failure %s %d\n",\
				__FILE__, __LINE__); exit(12); } else

# if __STDC__		/* Or rather; prototypes */
# define ARGS(x)	x
# else
# define ARGS(x)	()
# endif
# include "lint.h"

/*
 *	Because we lack this in C:
 */
# define Bool	int
# define Yes	1
# define No	0

/********************************\
 *				*
 *	Manifest definitions:	*
 *				*
\********************************/

/* Max nesting level for soft-types when generating typedecl's: */
# define N_SOFT		200
# define MAX_ALIGN	8	/* Most restricted alignment	*/

/* Until everybody has Jack's new kernel (getreq, trans, putrep values)	*/
# define DIRTY_COMPAT_HACK	/* i.v.m. narigheid die Guido bedacht	*/
/* Until someone says ints should at least contain >20 bits:		*/
# define INTS_CAN_BE_SHORT
/* Util somebody fixes pcc: (which forbids enum comparison)		*/
# define NO_ENUM_COMPARISON	/* Only when Language(traditional)	*/

/*
 *	Debugging macro's
 */
# ifndef NO_DEB
# define DEBUG
# endif /* NO_DEB */
# ifdef DEBUG
# define dbg1(a)	if (options & OPT_DEBUG) mypf(OUT_HANDLE, a)
# define dbg2(a, b)	if (options & OPT_DEBUG) mypf(OUT_HANDLE, a, b)
# define dbg3(a, b, c)	if (options & OPT_DEBUG) mypf(OUT_HANDLE, a, b, c)
# define dbg4(a,b,c,d)	if (options & OPT_DEBUG) mypf(OUT_HANDLE, a,b,c,d)
# define dbg5(a,b,c,d,e) if (options & OPT_DEBUG) mypf(OUT_HANDLE, a,b,c,d,e)
# else /* DEBUG */
# define dbg1(a)
# define dbg2(a, b)
# define dbg3(a, b, c)
# define dbg4(a,b,c,d)
# define dbg5(a,b,c,d,e)
# endif /* DEBUG */

/*
 *	Options:
 */
# define OPT_DEBUG	0x010
# define OPT_BACKUP	0x020
# define OPT_NOCPP	0x040
# define OPT_NOOUTPUT	0x080
# define OPT_RECORDIN	0x100

# define MAX_HASH	31	/* Used in the ident-table		*/
# define LEX_EOF	'\0'	/* The code yacc wants for EOF		*/

/*
 *	Used by the C-clientstub generator
 */
# define B_ERROR	11	/* No buffer available	*/
# define B_NOBUF	12	/* No buffer required	*/
# define B_STACK	13	/* Buffer on stack	*/
# define B_MALLOC	14	/* Use malloc()/free()	*/
# define B_ALLOCA	15	/* Use alloca()		*/

/*
 *	Used to flag operators during generate clauses
 */
# define OF_IMPL_REPL	0x01	/* Implementation returns reply buffer	*/

/*
 *	Ail doesn't like discrimination between little- or bigendians.
 *	Integers are sent the way they are represented at the client.
 *	As the Dutch (used to) say: de klant is koning.
 *	So the server simply has to know wether the client is a
 *	good- or a bad- endian.
 *	Integers are treated as either:
 */
# define INT_GOOD	21	/* Server; we know its a good-endian	*/
# define INT_BAD	22	/* Server; it's a bad-endian		*/
# define INT_CLIENT	23	/* We're king				*/

/*
 *	State while generating marshalcode
 */
struct m_state {
	Bool ms_errlab;	/* Need the "error" label	*/
	Bool ms_memcpy;	/* Used memcpy => declare	*/
	Bool ms_shcast;	/* Need short for pcc bug	*/
	int  ms_offset;	/* Offset within buffer		*/
	char *ms_bufnm;	/* Name of the buffer		*/
	int ms_mode;	/* Good/Bad endian, client	*/
	struct itlist *ms_scope;	/* For ExprSpell		*/
	struct itlist *ms_check;	/* Which member is unchecked	*/
};

/*
 *	Used by C-server generator
 */
# define BS_ARG		-1	/* Buffersize is an argument		*/
# define BS_AIL		-2	/* Ail has to compute it		*/

/*
 *	Expressiontrees are unique, which make the moral pressure
 *	to free them lighter, and make pointer compares work
 */
struct etree {			/* Used for expression-trees		*/
    int et_optr;		/* Contains '+', INTCONST and the like	*/
    short et_const;		/* Is this expression constant?		*/
    struct etree *et_left, *et_right, *et_next;
    union {
	long uet_val;		/* Value if et_const			*/
	char *uet_id;		/* Value for IDENT			*/
    } et_uet;
};
# define et_val	et_uet.uet_val
# define et_id	et_uet.uet_id

/*
 *	The scanner takes care that all strings be unique,
 *	so comparing strings need not be done by strcmp().
 */
struct stind {
    char *st_str;		/* Actual string.	*/
    int st_len;			/* It's length.		*/
    struct stind *st_nxt;	/* Next node.		*/
};

/********************************************************\
 *							*
 *		Symbol table:				*
 *							*
\********************************************************/

struct itlist {
    struct typdesc *il_type;	/* Type of this entry			*/
    struct itlist *il_next;	/* To make lists			*/
    union {
	struct attr *iu_attr;	/* If used as an argument to an operator */
	struct name *iu_econ;	/* If used for enum-constant		*/
	int iu_offs;		/* If used as struct-member		*/
    } il_ui;
    char *il_name;		/* Its name				*/
    int il_bits;		/* Modified version of il_attr->at_bits	*/
};
# define il_attr il_ui.iu_attr
# define il_econ il_ui.iu_econ
# define il_offs il_ui.iu_offs
/* Never knew I was so fond of bitfields until I wrote this program:	*/
# define BitsSetNotset(bits, on, off) (((bits) & on) == on && !(bits & off))

struct attr {			/* Parameter attributes			*/
    int at_bits;		/* See below				*/
    int at_max;			/* Maximum buffersize			*/
    char *at_hmem;		/* Marshal in header			*/
    struct stind *at_act;	/* Id's in actual-size expression	*/
};
/*
 *	Bits in at_bits.
 */
# define AT_REQUEST	0x0001	/* The parameter goes in: request	*/
# define AT_REPLY	0x0002	/* The parameter goes out: reply	*/
# define AT_VAR		0x0004	/* Implied by AT_OUT, forced by "var"	*/
# define AT_NODECL	0x0010	/* Don't declare this one		*/
# define AT_CHARBUF	0x0020	/* It's a character array		*/
# define AT_NOMARSH	0x0040	/* Used copy in caller/message		*/
# define AT_ADVADV	0x0100	/* Server, don't marshal, but adv _adv	*/
# define AT_REDECL	0x0200	/* Server, AmComp returned ALMOST	*/
# define AT_CAST	0x0400	/* Server, lonely char[]		*/
# define AT_PYTHON	0x0800
/*
 *	Useful combinations of AT_REQUEST, AT_REPLY and AT_VAR
 */
# define AT_I		0x0001	/* in, call-by-value			*/
# define AT_O		0x0006	/* out, call-by-reference		*/
# define AT_VI		0x0005	/* in, call-by-reference (=var)		*/
# define AT_IO		0x0007	/* in-out, call-by-reference		*/

/*
 *	To store alternate procedure-codes, which are
 *	only known to the server.
 */
struct alt {
    int alt_int;
    struct alt *alt_next;
};

/*
 *	There are two "structures" associated with an operator,
 *	which actually describe the layout of the messagebuffer
 *	at request and reply time. In-buffer alignment is done
 *	by the structure-layout routine.
 *	The required buffersize is simply the size of the biggest
 *	structure.
 */

struct optr {			/* Operator description			*/
    char *op_name;		/* Name of the client lib func		*/
    char *op_ailword;		/* Header field for machine info	*/
    int op_ih, op_oh;		/* # parameters in the header, in & out	*/
    int op_ib, op_ob;		/* # parameters in the buffer, in & out	*/
    int op_val;			/* Value for h_command			*/
    int op_rights;		/* Required rights for this operation	*/
    int op_flags;		/* See below				*/
    struct typdesc *op_in;	/* Structure for request buffer		*/
    struct typdesc *op_out;	/* Structure for reply buffer		*/
    struct itlist *op_order;	/* Order of arguments as defined	*/
    struct itlist *op_args;	/* Order of arguments on the wire	*/
    struct itlist *op_client;	/* As used by clientstub		*/
    struct itlist *op_server;	/* As used by serverstub		*/
    struct alt *op_alt;		/* List of alternative opcodes		*/
};

# define OP_CLIENT	0x0001	/* Client-analysis done yet		*/
# define OP_SERVER	0x0002	/* op_server valid			*/
# define OP_STARRED	0x0004	/* Has a single '*' as first arg	*/
# define OP_OUTSTAR	0x0020	/* '*' has 'out' attribute - not used	*/
# define OP_INTSENS	0x0040	/* An integer sensitive operation	*/
# define OP_FLTSENS	0x0080	/* A float passed in the request	*/
# define OP_VARSENS	0x0100	/* Has no fixed size request		*/
# define OP_SENS	0x01c0	/* Useful bitpattern			*/
# define OP_NORPC	0x0200	/* Ail can't layout the messages	*/
# define OP_INCOMPAT	0x0400	/* Has a short, float or char parameter	*/

struct name {			/* How we store identified stuff	*/
    char *nm_name;		/* Litteral name, unique	        */
    struct name *nm_next;
    int nm_kind;        	/* Which union fields to use   		*/
    struct typdesc *nm_type;	/* SUE, TYPEDEF, enum constants		*/
    union {
        struct optr *u_optr;	/* CALL -- procedure    */
        long u_ival;		/* INTCONST		*/
	char *u_sval;		/* STRCONST		*/
    } name_u;
};

/* Make inner member accessible:        */
# define nm_sval		name_u.u_sval
# define nm_ival		name_u.u_ival
# define nm_optr		name_u.u_optr

struct clist {			/* List of classes			*/
    struct class *cl_this;	/* This one				*/
    struct clist *cl_next;	/* The next one				*/
};

struct class {			/* Groups operators			*/
    char *cl_name;		/* The name of it			*/
    struct clist *cl_definh;	/* Inheritance definition		*/
    struct clist *cl_effinh;	/* Effective inheritance		*/
    int cl_bsize;		/* Server-buffersize			*/
    int cl_align;		/* Alignment required for getreq buf	*/
    unsigned int cl_rights;	/* Mask of rights defined here		*/
    unsigned int cl_inhrights;	/* Mask of inherited rights		*/
    int cl_lo, cl_hi;		/* Reserved optr-value			*/
    int cl_last, cl_flags;	/* Last one used, flags			*/
    struct name *cl_scope;	/* Which name are defined here		*/
    struct stind *cl_incl;	/* Inclusion-list.			*/
};
# define CL_NORANGE	0x0002	/* No range specified; no optrs allowed	*/

struct m_name {			/* Marshaling information		*/
    char *m_clm, *m_clu;	/* Client marshaling routines names	*/
    char *m_svm, *m_svu;	/* Server marshaling routines names	*/
};

/*
 *	Important remarks on sizes:
 *	First of all, to avoid confusion: I am not interested in the
 *	sizes which the compiler will compute, since it is better to
 *	simply declare local variables, and have the compiler compute
 *	those sizes.
 *	The computed sizes refer to in-buffer sizes. Mostly they are
 *	compile-time constant, otherwise they must be computed by the
 *	stubs, sometimes with the help of a programmer-supplied
 *	expression/routine.
 *	A maximum size has the advantage that we don't need to call a
 *	memory allocator, even if the actual size must be computed.
 *	Sometimes the semantics require a maximum size that is dynamic
 *	as well (consider a read-to-eof request, where the client (not
 *	the clientSTUB) has buffer of run-time computed size).
 *
 *	In the next struct, td_esize is the dynamic size, and td_msize
 *	the maximum one. If they are NULL, the size is unknown.
 */

/*
 *	Descriptor for a type.
 *	Types are unique by nature (not by decision).
 */
struct typdesc {
    int td_kind;			/* Type-kind: see below		*/
    int td_bits;			/* See below			*/
    int td_align;			/* Alignment requirements	*/
    struct etree *td_msize;		/* Maximum size			*/
    struct etree *td_esize;		/* Actual size			*/
    struct m_name *td_marsh;		/* Marshaling info		*/
    struct typdesc *td_next;		/* Too make a list		*/
    struct typdesc *td_styp;		/* SimpleType for decl		*/
    struct class *td_parent;		/* Where it was defined		*/
    union {
	struct {			/* Info for basic types		*/
	    int bt_size;		/* Long/short info		*/
	    int bt_sign;		/* Unsigned/signed		*/
	} un_bt;
	struct {			/* Info for structs, unions	*/
	    struct itlist *ag_meml;	/* Members			*/
	    char *ag_name;		/* Name of this sue		*/
	    int ag_defd;		/* Has been defined		*/
	} un_ag;
	struct {			/* Info for others		*/
	    struct typdesc *st_prim;	/* Type derived from		*/
	    struct etree *st_bound;	/* For arrays			*/
	    union {
		struct etree *st_act;	/* For arrays			*/
		char *st_tname;		/* For typedefs			*/
	    } st_un;
	} un_st;
    } td_un;
};

/* Make inner members easily accessible:	*/
# define td_size td_un.un_bt.bt_size
# define td_sign td_un.un_bt.bt_sign
# define td_meml td_un.un_ag.ag_meml
# define td_name td_un.un_ag.ag_name
# define td_defd td_un.un_ag.ag_defd
# define td_prim td_un.un_st.st_prim
# define td_bound td_un.un_st.st_bound
# define td_act  td_un.un_st.st_un.st_act
# define td_tname td_un.un_st.st_un.st_tname

/* Bits in td_bits: */
# define TD_CONTXT	0x0001	/* Is contextually usable	*/
# define TD_DECLED	0x0002	/* Declared yet			*/
# define TD_MARK	0x0008	/* To mark reacheable types	*/
# define TD_CPREF	0x0010	/* C passes this type by ref	*/
# define TD_VARSIZE	0x0020	/* Has variable size		*/
# define TD_CONTINT	0x0040	/* Contains an integer		*/
# define TD_CONTFLT	0x0080	/* Has float; not in use yet	*/
/* Bits that types pass on to each other:	*/
# define TD_INHERIT	(TD_VARSIZE|TD_CONTINT|TD_CONTFLT)

/*
 *	A gen is an argument to a generator
 */
struct gen {			/* Describes the generate stuff		*/
    struct gen *gn_next;	/* Next argument			*/
    struct typdesc *gn_type;	/* This arg's type			*/
    struct etree *gn_value;	/* Its value				*/
    char *gn_name;		/* And its name				*/
};

/*
 *	Values for td_kind.
 *	Yacc generates token values outside the ASCII range,
 *	which makes it save to mix character constants and
 *	tokens here.
 */
# define tk_dunno	'?'	/* For the parser: modifiers	*/
# define tk_err		'e'	/* Previously detected error	*/
# define tk_char	CHAR
# define tk_int		INT
# define tk_float	FLOAT
# define tk_double	DOUBLE
# define tk_void	VOID	/* No type			*/
# define tk_string	STRING
# define tk_struct	STRUCT
# define tk_union	UNION	/* Useless			*/
# define tk_enum	ENUM	/* Useless			*/
# define tk_func	'('	/* Useless			*/
# define tk_ptr		'*'	/* Useless			*/
# define tk_arr		'['	/* Array/buffer			*/
# define tk_tdef	TYPEDEF	/* typedef -- another type	*/
# define tk_ref		'^'	/* Reference -- internal	*/
# define tk_copy	'c'	/* Copied value (server side)	*/

/*
 *	Values for 'lang' -- the target language
 */
# define L_TRADITIONAL	1	/* K&R C		*/
# define L_ANSI		2	/* Ansi X3J11		*/
# define L_LINT		3	/* lint			*/
# define L_PYTHON_SC	4	/* Python stub code	*/
# define L_DEFAULT	L_TRADITIONAL	/* Probably changed in the future */
# define N_LANGUAGES	L_PYTHON_SC	/* How many languages there are */

/*
 *	For the file-handling stuff:
 */
typedef int handle;	/* Replacement of fprintf's FILE *	*/
# define OUT_HANDLE	-1	/* Equivalent with stdout	*/
# define ERR_HANDLE	-2	/* Equivalent with stderr	*/

/*
 *	For operator flagging while generating:
 */
# define FL_IMPLBUF	0x01	/* Implementation has buffer (Impl_repl_buf) */
# define FL_IMPLREPL	0x02	/* Implementation calls putrep		*/
# define FL_IDEMPOTENT	0x04	/* Client may retry in case of RPC_FAILURE */
# define FL_CLIENTMON	0x08	/* Put monitor support in clientstub	*/
# define FL_PASSACT	0x10	/* Pass value of activate function to impl */

/*
 *	Functions declarations
 */
void		AddBuf ARGS((handle h, char *buffer, int size));
void		AddChar ARGS((handle h, int ch));
void		Align ARGS((handle h, int n_byte));
char		*AlignType ARGS((struct optr *));
int		AmComp ARGS((struct typdesc *par, struct typdesc *field));
void		AmEmpty ARGS((void));
int		AmFind ARGS((char *member));
void		AmInit ARGS((void));
void		AmOk ARGS((char *, struct typdesc *, int));
struct typdesc	*AmType ARGS((char *fieldname));
char		*AmWhere ARGS((struct typdesc *, int));
struct typdesc	*ArrayOf ARGS((struct typdesc *,
		    struct etree *act, struct etree *max));
void		arrmsh ARGS((handle, char *, struct etree *,
		    struct typdesc *, int, int *));
void		arrunmsh ARGS((handle, char *, struct etree *,
		    struct typdesc *, int, int *));
handle		BackPatch ARGS((handle parent));
void		Cast ARGS((handle h, struct typdesc *));
Bool		CharsOnly ARGS((struct typdesc *));
Bool		CharsOrIntsOnly ARGS((struct typdesc *));
struct etree	*Cleaf ARGS((long value));
void		CheckExpr ARGS((struct itlist *, struct stind *, int));
void		CheckOptrVal ARGS((int));
void		ClientOptr ARGS((handle, struct optr *));
void		CloseClass ARGS((void));
void		CloseFile ARGS((handle));
void		CommandOptr ARGS((handle, struct optr *));
void		ConvEnum ARGS((handle, struct typdesc *, Bool str, Bool ord));
struct itlist	*CopyItList ARGS((struct itlist *));
void		DeclList ARGS((handle, struct itlist *));
void		DefClass ARGS((char *name, int has_range, int lo, int hi,
			struct clist *inherit, struct stind *includes));
struct typdesc	*DefEnum ARGS((char *name, struct name *mlist));
void		DefOptr ARGS((char *name, int value, struct itlist *args,
			int flags, struct alt *ap, int rights));
struct typdesc	*DefPRF ARGS((struct typdesc *prim, int kind));
struct typdesc	*DefSU ARGS((int kind, char *name, struct itlist *members));
struct etree	*Diadic ARGS((int optr, struct etree *, struct etree *));
struct itlist	*DoesOwnRepl ARGS((struct optr *));
void		DoToDo ARGS((void));
void		DoDecl ARGS((handle whichfile, Bool server));
void		ExprSpell ARGS((handle, struct etree *, int,
			struct itlist *, Bool));
void		Fail ARGS((handle, Bool is_svr, int direction));
void		fatal ARGS((char *));
struct class	*FindClass ARGS((char *));
struct name	*FindName ARGS((char *id, struct name *where));
struct typdesc	*FindSUE ARGS((int kind, char *name));
void		GenerAdd ARGS((char *name));
void		GenServer ARGS((Bool, Bool));
char		*getenv ARGS((const char *));
int		GetOptrVal ARGS((void));
int		GetRight ARGS((char *name));
long		GetVal ARGS((struct etree *p));
void		HeadMsh ARGS((handle, struct itlist *, int, int));
void		HeadUnmsh ARGS((handle, struct itlist *, int, int));
struct etree	*Ileaf ARGS((char *id, Bool con));
void		ImplInit ARGS((void));
void		Indent ARGS((handle h));
void		IndentChange ARGS((handle h, int offset));
struct clist	*InhStuff ARGS((struct clist *));
Bool		InsName ARGS((struct name *));
char		*IoName ARGS((int bits));
struct typdesc	*IsTD ARGS((char *tag));
int		IsZero ARGS((struct etree *));
struct itlist	*it ARGS((char *name, struct typdesc *type));
struct itlist	*itcat ARGS((struct itlist *list1, struct itlist *list2));
void		ListGenerators ARGS((int));
void		MainLoop ARGS((Bool, int, Bool));
void		Marshal ARGS((handle, int, struct optr *, int));
void		Minit ARGS((char *));
struct typdesc	*ModifTyp ARGS((int kind, unsigned lc, unsigned sc, int sign));
void		Moffset ARGS((struct etree *));
struct etree	*Monadic ARGS((int optr, struct etree *opnd));
void		Mreinit ARGS((char *));
void		msh ARGS((handle, char *, struct typdesc *, int, int *));
char		*MyAlloc ARGS((unsigned int));
void		mypf ARGS((handle, char *fmt, ...));
void		MustDecl ARGS((struct m_name *));
void		MustDeclMemCpy ARGS((void));	/* ArMe GuIdO...	*/
struct class	*NewClass ARGS((char *));
struct name	*NewName ARGS((char *id, int kind));
struct typdesc	*NewTD ARGS((char *id, struct typdesc *t));
Bool		NoMarsh ARGS((struct itlist *, char *));
handle		OpenFile ARGS((char *name));
void		OptrHead ARGS((handle, struct optr *, int, int));
void		Prefix ARGS((handle, struct typdesc *));
struct typdesc	*RecordMarshInfo ARGS((struct typdesc*, struct m_name*,
			struct etree *act, struct etree *max, Bool));
struct typdesc	*SameType ARGS((struct typdesc *));
Bool		ScopeType ARGS((handle file, struct name *scope));
char		*Serr ARGS((int));
void		ServerAnalysis ARGS((struct optr *));
void		ServerOptr ARGS((struct optr *, Bool rights_check));
void		SetFlag ARGS((int optr_value, int bit));
void		SetIConst ARGS((char *id, long val, struct typdesc * e_type));
void		SetSConst ARGS((char *id, char *val));
void		SetRight ARGS((char *name, unsigned value));
void		SetVersion ARGS((char *version_name));
void		ShowType ARGS((struct typdesc *));
void		ShowTypTab ARGS((struct gen *));
struct etree	*SizeSum ARGS((struct itlist *, int, int, int));
struct etree	*Sleaf ARGS((void));
void		SoftSpell ARGS((handle, struct typdesc *, char *));
void		StartGenerate ARGS((char *name));
void		StopGenerate ARGS((void));
char		*StrFind ARGS((char *));
void		SymInit ARGS((void));
void		Tdecl ARGS((handle, struct typdesc *));
char		*TokenToString ARGS((int));
void		TouchType ARGS((struct typdesc *));
void		TypeSpell ARGS((handle, struct typdesc *, Bool));
void		UnDeclTypes ARGS((void));
void		UnFlagOp ARGS((void));
void		Unmarshal ARGS((handle, int, struct optr *, int));
void		unmsh ARGS((handle, char *, struct typdesc *, int, int *));
struct stind	*UsedIdents ARGS((struct etree *));
int		WhereIsName ARGS((char *id, struct class **, struct name **));
struct name	*WhereIsTD ARGS((char *id));
struct name	*WhereIsIConst ARGS((char *id));
#ifdef PYTHON
void		xDumpFlags ARGS((int iFlags));
void		xDumpTree ARGS((TpsPythonTree psPTree));
void		xDumpEntry ARGS((TpsPythonTree psTree));
void		xDumpStack ARGS((TpsPythonTree apsStack[], int sp));
void		xPythonDocGen ARGS((struct gen *psArgs));
void		xPythonClientGen ARGS((struct gen *psArgs));
void		AllPythonStubs ARGS((struct gen *psArgs));
void		AllPythonDocs ARGS((struct gen *psArgs));

#endif
/*
 *	Semi functions:
 */
# define PointerTo(obj)		DefPRF(obj, tk_ptr)
# define FuncOf(obj)		DefPRF(obj, tk_func)
# define RefTo(obj)		DefPRF(obj, tk_ref)
# define CopyOf(obj)		DefPRF(obj, tk_copy)
# define new(type)		(type *) MyAlloc(sizeof(type))
# define FREE(p)		free((char *) p)

/*
 *	Extern data-definitions.
 */
extern enum en_act { AC_NO, AC_GEN} Activate;
extern char *ActFunc, *DeactFunc, *ActType;
extern int am_almost;		/* State left after a call to	*/
extern int am_leftinint;	/* AmWhere or AmOk.		*/
extern struct typdesc *cap_typ;	/* The type capability		*/
extern struct clist *ClassList;	/* All the classes		*/
extern char *client_stub_return_type;	/* Litteral		*/
extern char *general_header_file;	/* Litteral		*/
extern char *dfltoutdir;	/* Default directory for output	*/
extern int dfltoutlang;		/* Runtime default language	*/
extern int dynamic;		/* How to allocate memory in C	*/
extern int ErrCnt;		/* # errors seen so far		*/
extern struct typdesc ErrDesc;	/* The error typedescriptor	*/
extern char *FileName;		/* Current source-filename	*/
extern char *genname;		/* Name of active generator	*/
extern struct class GlobClass;	/* The universe-class		*/
extern struct class *ImplClass;	/* Copy of ThisClass (ImplClass)*/
extern int lang;		/* Target language		*/
extern char *languages[];	/* Names of supported languages	*/
extern int LineNumber;		/* Source line-number		*/
extern char *mainloop_return_type;	/* Litteral		*/
extern int MaxFilenameLen;	/* To truncate filenames	*/
extern struct m_state MarshState;	/* Marshaling state	*/
extern int MonitorAll;		/* Need to monitor all stubs	*/
extern int PostDeact;		/* deact called after putrep	*/
extern int n_optrs;		/* # operators are defined	*/
extern long n_read;		/* # characters read		*/
extern int NoKeywords;		/* For the scanner: don't recognize keywords */
extern int options;		/* Option bits			*/
extern char *outdir;		/* Directory containing outputs	*/
extern char *pseudofield;	/* The capability field in hdr	*/
extern int RetryCount;		/* # retries we want to make	*/
extern struct typdesc StrDesc;	/* The string typedescriptor	*/
extern struct class *ThisClass;	/* Current class		*/
extern struct optr *ThisOptr;	/* Current operator diagnostics	*/
extern int WarnCnt;		/* # warnings seen so far	*/
