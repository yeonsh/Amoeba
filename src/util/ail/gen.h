/*	@(#)gen.h	1.2	94/04/07 14:32:59 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */


/*
 *	The generators all have the same prototype
 */
#define GEN(func)	void func ARGS((struct gen *arglist))

GEN(AllHeaders);
GEN(AllStubs);
GEN(ActDeAct);
GEN(CClientGen);
GEN(CommandGen);
GEN(DefGen);
GEN(DummyGen);
GEN(Dynamic);
GEN(EnumOrd);
GEN(Feature);
GEN(FlatHeader);
GEN(HeaderGen);
GEN(Idempotent);
GEN(IntActDeAct);
GEN(ImplGen);
GEN(ImplRepl);
GEN(LintGen);
GEN(MarshPair);
GEN(MonClientGen);
GEN(PassAct);
GEN(PtrActDeAct);
GEN(Language);
GEN(ListFiles);
GEN(ServerGen);
GEN(SetDir);
GEN(SetPostDeact);
GEN(ShowClassGen);
GEN(Verbose);
#ifdef PYTHON
GEN(xPythonClientGen);
GEN(xPythonDocGen);
GEN(AllPythonStubs);
GEN(AllPythonDocs);
GEN(xPythonSvGen);
#endif

void Generate ARGS ((char *name, struct gen *args));

/*
 *	And a few others:
 */
void FindOptrs ARGS((struct gen *, int search_deep, void (*func)()));

/*
 *	And the declaration of the patch table.
 *	All generators are listed in this table.
 *	The function Generate() calls the functions that implement them.
 *	Arguments to the generators are passed using a struct gen list.
 *	Generate() checks that the arguments conform to the bits in
 *	impl_bits. The generators won't be called if there was any error,
 *	but the bits are always checked, since yyparse() doesn't know
 *	anything about the specific generators.
 *	This design is intended to not bother future ail hackers with
 *	the parser.
 */
extern struct patch {
	char	*impl_name;	/* The semi-keyword to trigger it	*/
	int	impl_bits;	/* See below				*/
	char	*impl_remark;	/* A remark by the author about it	*/
	/* Which function should be called:	*/
	void	(*impl_func[N_LANGUAGES + 1])();
} genen[];			/* What a nice identifier!		*/
#define IM_IDUNI	0x01	/* Needs unique identifiers		*/
#define IM_NOTYPE	0x02	/* Doesn't like arguments with types	*/
#define IM_NOVAL	0x04	/* No values allowed			*/
#define IM_NOCLASS	0x08	/* Can also be called without class	*/
#define IM_NOARG	0x10	/* Doesn't want arguments at all	*/
#define IM_ONEARG	0x20	/* Wants exactly one argument		*/

/* Some useful combinations:	*/
#define IM_NOFP		(IM_NOTYPE|IM_NOVAL)	/* No fancy parameters	*/
