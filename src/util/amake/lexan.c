/*	@(#)lexan.c	1.3	94/04/07 14:52:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "idf.h"
#include "scope.h"
#include "Lpars.h"
#include "parser.h"
#include "symbol2str.h"
#include "error.h"
#include "dbug.h"
#include "objects.h"
#include "slist.h"
#include "main.h"
#include "input.h"
#include "expr.h"
#include "assignment.h"
#include "tokenname.h"
#include "os.h"
#include "lexan.h"

/*
 * Een aantal speciale karakters waarmee rekening moet worden gehouden.
 */
#define QUOTE		'\''
#define BACKSLASH	'\\'
#define NEW_LINE	'\n'
#define CARRIAGE_RETURN	'\r'
#define FORMFEED	'\f'
#define SPACE		' '
#define TAB		'\t'
#define UNDER_LINE	'_'
#define DOT		'.'
#define NUMBER_SIGN	'#'
#define DOLLAR		'$'
#define PERCENT		'%'

/*
 * Amake special tokens:
 *
 * ( ) { }  & / [ ] ; , + ? \ : "
 * = == <> => <=>
 */

PRIVATE char SimpleSeps[] = "(){}&/[];,+?\\:"; /*separators van 1 karakter */
PRIVATE char CompoundSeps[] = "\"=<";
/*
 * CompoundSeps contains the seperators that need special attention.
 * The '"' introduces a mode-change, while the '=' and '<' might introduce
 * a multi character token.
 */

/*
 * The characters are divided over a number of classes, each representing
 * the set of tokens that could be started with such a character.
 */
#define C_GARBAGE   0	/* not starting any token			  */
#define C_ID_STRING 1	/* starts an identifier (path componen or string) */
#define C_COMMENT   3	/* starts a comment				  */
#define C_SEP       4	/* seperator consisting of only one character     */
#define C_SEP2	    5   /* compound seperator				  */
#define C_SPACE     6	/* white space					  */
#define C_NEWLINE   7	/* '\n', increases line number			  */
#define C_EOI       8	/* delivered by LoadChar at end of file		  */
#define C_INTEGER   9   /* starts integer, used in cache for cid attrs    */

PRIVATE short Class[NUM_CHARS];
/* A character c belongs to class Class[c] */

PRIVATE short
    InIdent[NUM_CHARS], InVarName[NUM_CHARS], InQuotedString[NUM_CHARS];
/*
 * InIdent[ch] == TRUE iff ch may appear in a name *after* the first character
 * of that name.
 * InVarName applies to the identifier directly following the '$'.
 */

PUBLIC struct token Tok;	/* Contains the last token read. */

PRIVATE struct token PushedBack;
/* When the parser inserts a token for error recovery, Tok is copied
 * in PushedBack. At a following later of the lexical analyser, PushedBack
 * will be copied back in Tok.
 */

PUBLIC struct f_info file_info;

PUBLIC void
HandleNewLine()
{
    file_info.f_line++;
}

/* directory search table */
#define DIRTABSIZE 10
PRIVATE char *dirtab[DIRTABSIZE+1] = {NULL};
PRIVATE char *context_tab[] = {"", NULL};

PUBLIC void
SetDirTab(obj_list)
struct slist *obj_list;
/* set the directory search table to the names of the objects in obj_list */
{
    int i = 0;

    ITERATE(obj_list, expr, obj_expr, {

	if (i >= DIRTABSIZE - 1) {
	    FatalError("too many file search directories");
	}

	/* free old entry */
	if (dirtab[i] != NULL) {
	    free(dirtab[i]);
	}

	if (obj_expr->e_object == Context) {
	    dirtab[i] = Salloc("", (unsigned)1);
	} else {
	    register char *name = SystemName(obj_expr->e_object);

	    dirtab[i] = Salloc(name, (unsigned)(strlen(name) + 1));
	}
	i++;
    });
    dirtab[i++] = Salloc("", (unsigned)1);
    dirtab[i] = NULL;
    DBUG_EXECUTE("lexan", {
	for (i = 0; i < DIRTABSIZE; i++) {
	    if (dirtab[i] == NULL) break;
	    DBUG_PRINT("lexan", ("dirtab[%d]: \"%s\"", i, dirtab[i]));
	}
    });
}

PRIVATE struct slist *FileStack = NULL;
PRIVATE void init_arrays();

PRIVATE void
UpdatePWD(incl)
struct object *incl;
{
    extern struct idf *Id_PWD;

    Id_PWD->id_variable->var_expr = ObjectExpr(incl->ob_parent);
}

PRIVATE struct object *
DoInsert(filename, table)
char *filename;
char *table[];
{
    char *result;

    if (InsertFile(filename, table, &result) != 1) {
	CheckInterrupt();	/* maybe that's the cause */
	FatalError("file `%s' not found", filename);
	/*NOTREACHED*/
    } else {
	struct object *incl = SystemNameToObject(result);

	DBUG_PRINT("lexan", ("`%s' will be included", result));
	UpdatePWD(incl);
	incl->ob_flags |= O_BEING_INCL;

	file_info.f_line = 1;
	file_info.f_file = Salloc(result, (unsigned)(strlen(result) + 1));
	
	return(incl);
    }
}

PUBLIC void
read_from(filename)
char *filename;
{
    struct object *incl;

    PushedBack.t_number = 0;		/* No token pushed back yet. */
    incl = DoInsert(filename, dirtab);
    Append(&FileStack, incl);
}

PRIVATE void next_token();		/* forward declaration */

PUBLIC int
lexan()
/* 'next_token' is the real lexical analyser, 'lexan' considers the
 * possibility of inserted tokens.
 */
{
    if (PushedBack.t_number) {		/* token was put aside */
	Tok = PushedBack;
	PushedBack.t_number = 0;
    } else {
	next_token(&Tok);
    }
    return(Tok.t_number);
}


PUBLIC int
read_ahead()
/* Let's the parser look one token further in order to resolve a few parsing
 * conflicts. Only returns the tokennumber, but that's enough.
 */
{
    if (!PushedBack.t_number) { /* no token pushed back, read new one */
	next_token(&PushedBack);
    }
    return(PushedBack.t_number);
}

#define UCHAR(ch)	((unsigned char) (ch))
/* Use UCHAR(c) to make posibly negative character values positive. This
 * is necessary when a character is used as index in the arrays InIdent and
 * Class.
 */

#define ID_ALLOC_SIZE 24

PRIVATE char
    *Str,	/* extendable buffer used in reading identifiers */
    *StrTail;	/* points to the character *before* the end of this buffer */
PRIVATE unsigned
    StrLen;	/* # characters allocated for this buffer */

PUBLIC void
init_lex()
{
    Str = Malloc(StrLen = ID_ALLOC_SIZE);
    StrTail = Str + StrLen - 2; /* a '\0' may be put at StrTail+1 */
    init_arrays();
}

PRIVATE int BetweenQuotes = FALSE;
/* Between '"'s the lexical analyser behaves differently:
 * spaces etc. are signifant, but variables should be preserved.
 * The text   "hello, $world."  is delivered to the parser as
 *
 * token('"') STRING('hello, ') REFID('world') STRING('.') token('"')
 */

PRIVATE int
read_ident(tokp, first_ch)
struct token *tokp;
int first_ch;
/* read identifier (string or part of filename), keyword or var. reference */
{
    register int ch;
    unsigned so_far;
    register char *p;
    short *InToken;		/* will be pointed InVarName or InIdent */

    if (BetweenQuotes) {
	if (first_ch == DOLLAR) {
	    LoadChar(ch);
	    if (!InVarName[UCHAR(ch)]) {
		LexError("'%c' deleted", first_ch); PushBack();
		return(FALSE);
	    }
	    InToken = InVarName;
	} else {
	    ch = first_ch;
	    InToken = InQuotedString;
	}
    } else {
	switch (first_ch) {
	case DOLLAR:
	case PERCENT:
	    LoadChar(ch);
	    if (!InVarName[UCHAR(ch)]) {
		LexError("'%c' deleted", first_ch); PushBack();
		return(FALSE);
	    }
	    InToken = InVarName;
	    break;
	case QUOTE:
	    LoadChar(ch);	/* skip quote */
	    InToken = InIdent;
	    break;
	default:
	    ch = first_ch;
	    InToken = InIdent;
	    break;
	}
    }

    p = Str;
    for (;;) {		/* till identifier is finished */ 
	while (p <= StrTail) {
	    if (InToken[UCHAR(ch)]) {
		*p++ = ch;
		LoadChar(ch);
	    } else if (first_ch == QUOTE) {
		if (ch == QUOTE) {
		    LoadChar(ch);
		    if (ch == QUOTE) {	/*2 quotes inside quoted name*/
			*p++ = QUOTE;
			LoadChar(ch);
		    } else {		/* ending quote read */
			goto Done;
		    }
		} else {
		    if (isprint(ch) || ch == '\t') {
			*p++ = ch;
			LoadChar(ch);
		    } else if (ch == '\n') {
			*p++ = ch;
			LoadChar(ch);
			HandleNewLine();
		    } else {
			LexError("garbage character ('\\%03o') deleted",
				 UCHAR(ch));
			LoadChar(ch);
			goto Done;
		    }
		}
	    } else goto Done;
	}

	so_far = p - Str;
	Str = Realloc(Str, StrLen += ID_ALLOC_SIZE);
	StrTail = Str + StrLen - 2;
        p = Str + so_far;
    }
Done:
    *p = '\0';		/* terminate string, will surely fit */
    PushBack();		/* last character read doesn't belong to this token */

    if (BetweenQuotes) {
	if (first_ch == DOLLAR) {
	    tokp->t_number = REFID;
	    tokp->t_idp = str2idf(Str, 1);
	} else {
	    tokp->t_number = STRING;
	    tokp->t_string = Salloc(Str, (unsigned)(p - Str + 1));
	}
    } else {
	switch (first_ch) {
	case DOLLAR:
	    tokp->t_number = REFID;
	    tokp->t_idp = str2idf(Str, 1);
	    break;
	case PERCENT:
	    if ((tokp->t_idp = str2idf(Str, 1))->id_token != 0) {
		tokp->t_number = tokp->t_idp->id_token;
	    } else {
		LexError("`%s' is not a keyword", Str);
		return(FALSE);
	    }
	    break;
	case QUOTE:
	    tokp->t_number = STRING;
	    tokp->t_string = Salloc(Str, (unsigned)(p - Str + 1));
	    break;
	default:
	    tokp->t_number = ID;
	    tokp->t_idp = str2idf(Str, 1);
	    break;
	}
    }
    return(TRUE);
}

PRIVATE void
next_token(tokp)
struct token *tokp;
/* The real lexical analyser; it puts the next token in *tokp. */
{
    register int ch;

    if (BetweenQuotes) {
	for (;;) {
	    LoadChar(ch);
	    if (ch == '"') {
		BetweenQuotes = FALSE;
		tokp->t_number = '"';
		return;
	    } else if (read_ident(tokp, ch)) {
		return;
	    }
	}
    } else {
	for (;;) {			/* until a token has been found */
	    LoadChar(ch);
	    switch (Class[UCHAR(ch)]) {
	    case C_ID_STRING:
		if (read_ident(tokp, ch)) {
		    return;
		} else {
		    continue;
		}
	    case C_SEP:
		tokp->t_number = ch;	/* the token is ch itself */
		return;
	    case C_SEP2:
		switch (ch) {
		case '<':
		    LoadChar(ch);
		    if (ch == '>') {		/* "<>" */
			tokp->t_number = NOT_EQ; return;
		    }
		    PushBack();
		    tokp->t_number = '<';
		    return;
		case '=':
		    LoadChar(ch);
		    if (ch == '=') {		/* "==" */
			tokp->t_number = EQ;
		    } else if (ch == '>') {
			tokp->t_number = ARROW;
		    } else {
			PushBack(); tokp->t_number = '=';
		    }
		    return;
		case '"':
		    BetweenQuotes = TRUE;
		    tokp->t_number = ch;
		    return;
		default:
		    CaseError("lexan() [1]: %d", ch);
		}
		continue;
	    case C_COMMENT:		/* '#' starts comment till eol / eof */
		LoadChar(ch);
		while (Class[UCHAR(ch)] != C_NEWLINE) {
		    if (ch == EOI) {
			tokp->t_number = EOFILE;	/* end of file */
			return;
		    }
		    LoadChar(ch);
		}
		HandleNewLine();
		continue;
	    case C_SPACE:
		continue;				/* look further */
	    case C_NEWLINE:
		HandleNewLine();
		continue;				/* look further */
	    case C_GARBAGE:
		if (isprint(ch)) {
		    LexError("illegal character '%c' deleted", ch);
		} else {
		    LexError("garbage character ('\\%03o') deleted",
			     UCHAR(ch));
		}
		continue;				/* look further */
	    case C_EOI:
		tokp->t_number = EOFILE;		/* end of file */
		return;
	    case C_INTEGER:
	    {
		register long val = ch - '0';
		
		LoadChar(ch);
		while (isdigit(ch)) { /* no overflow checking, yet */
		    val = 10 * val + ch - '0';
		    LoadChar(ch);
		}
		PushBack();
		tokp->t_number = INTEGER;
		tokp->t_integer = val;
		return;
	    }
	    default:
		CaseError("lexan() [2]: %d", Class[UCHAR(ch)]);
	    }
	}
    }
}

PRIVATE char *
Symbol(tok)
int tok;
{
    return(tok == EOFILE ? "end of file" : symbol2str(tok));
}

LLmessage(tok_number)
int tok_number;
/* LLmessage is called by the parser, when tokens are to be skipped or
 * inserterted. This function must take care to properly initialise
 * (with dummy values) the token's attributes, so that they can be trusted.
 */
{
    static char err_string[80];
    extern int LLsymb;

    switch (tok_number) {
    case -1:
	ParseError("end of file expected"); return;
    case 0: /* current token is to be skipped */
	(void) strcpy(err_string, symbol2str(LLsymb));
	(void) strcat(err_string, " deleted before ");
	(void) strcat(err_string, Symbol(read_ahead()));
	ParseError("%s", err_string);	/* watch out for '%' in err_string */
	return;
    case ID:
    case REFID:
	PushedBack = Tok;
	Tok.t_idp = ErrorId();
	break;
    case STRING: {
	struct idf *idp;

	PushedBack = Tok;
	idp = ErrorId();   /* we only need the string, not the idf */
	Tok.t_string = idp->id_text;
	break;
    }
    default:
	PushedBack = Tok;
    }
    Tok.t_number = tok_number;
    (void) strcpy(err_string, symbol2str(Tok.t_number));
    (void) strcat(err_string, " inserted before ");
    (void) strcat(err_string, Symbol(PushedBack.t_number));
    ParseError("%s", err_string);	/* watch out again */
}

PUBLIC void
InsertNonIncluded()
{
    while (!IsEmpty(FileStack)) {
	struct object *incl = ItemP(TailOf(FileStack), object);

	if ((incl->ob_flags & O_IS_INCL) != 0) {
	    DBUG_PRINT("lexan", ("`%s' has already been included",
				 SystemName(incl)));
	    Remove(&FileStack, TailOf(FileStack));
	} else if ((incl->ob_flags & O_BEING_INCL) != 0) {
	    DBUG_PRINT("lexan", ("returning to file `%s'", SystemName(incl)));
	    UpdatePWD(incl);
	    return;
	} else {
	    incl = DoInsert(SystemName(incl), context_tab);
	    return;
	}
    }
}

PUBLIC struct object *
CurrentFile()
{
    if (FileStack != NULL) {
	return(ItemP(TailOf(FileStack), object));
    } else {
	return(NULL);
    }
}

PUBLIC int
AtEoIF()
{
    struct object *current = CurrentFile();

    assert(current != NULL);
    if (ParsingConstruction) {
	Warning("unfinished construction at end of file");
    }
    current->ob_flags &= ~O_BEING_INCL;
    current->ob_flags |= O_IS_INCL;
    
    Remove(&FileStack, TailOf(FileStack));

    return(2); /* let input package call InsertIfYouWish */
}

int
AtEoIT()
{
        return 0;
}

InsertIfYouWish()
{
    InsertNonIncluded();
}

PUBLIC void
LexanInsert(obj_list)
struct slist *obj_list;
{
    struct cons *cons;

    for (cons = Tail(obj_list); cons != NULL; cons = Prev(cons)) {
	struct object *incl = ItemP(cons, expr)->e_object;
	char *filename = SystemName(incl), *result;

	if (FindFile(filename, dirtab, &result) != 1) {
	    FatalError("file '%s' not found", filename);
	    /*NOTREACHED*/
	}
	incl = SystemNameToObject(result);
	/* this is the real object, probably not the original incl! */

	if ((incl->ob_flags & O_BEING_INCL) != 0) {
	    FatalError("recursive include of `%s'", SystemName(incl));
	} else if ((incl->ob_flags & O_IS_INCL) != 0) {
	    DBUG_PRINT("lexan", ("`%s' already included", SystemName(incl)));
	} else {
	    DBUG_PRINT("lexan", ("`%s' put on FileStack", SystemName(incl)));
	    Append(&FileStack, incl);
	}
    }
    InsertNonIncluded();
}

PRIVATE void
init_arrays()
/* Initialises arrays Class, InIdent and InVarName. */
{
    register int i;
    register char *p;

    for (i = 0; i < NUM_CHARS; i++) {
	InQuotedString[i] = isprint(i);
    }
    InQuotedString[DOLLAR] = InQuotedString['"'] = FALSE;

    for (i = 0; i < NUM_CHARS; i++) {
	Class[i] = C_GARBAGE;
	InVarName[i] = InIdent[i] = FALSE;
    }
    for (i = '0'; i <= '9'; i++) {
	InVarName[i] = InIdent[i] = TRUE;
	Class[i] = C_INTEGER;
    }
    for (i = 'a'; i <= 'z'; i++) {
	Class[i] = C_ID_STRING; InVarName[i] = InIdent[i] = TRUE;
    }
    for (i = 'A'; i <= 'Z'; i++) {
	Class[i] = C_ID_STRING; InVarName[i] = InIdent[i] = TRUE;
    }
    InVarName[UNDER_LINE] = InIdent[UNDER_LINE] = TRUE;
    InIdent['-'] = InIdent[DOT] = TRUE; /* not allowed in variable names */

    Class['-'] = Class[DOT] = Class[UNDER_LINE] =
	Class[PERCENT] = Class[DOLLAR] = Class[QUOTE] = C_ID_STRING;

    for (p = SimpleSeps; *p; p++) {
	Class[*p] = C_SEP;
    }
    for (p = CompoundSeps; *p; p++) {
	Class[*p] = C_SEP2;
    }
    Class[NUMBER_SIGN] = C_COMMENT;

#ifdef AtariSt /* On DOS-like systems, treat carriage returns as spaces */
    Class[CARRIAGE_RETURN] =
#endif
    Class[SPACE] = Class[TAB] = Class[FORMFEED] = C_SPACE;

    Class[NEW_LINE] = C_NEWLINE;
    Class[UCHAR(EOI)] = C_EOI;
}

PUBLIC int
IsIdent(str)
char *str;
{
    register char *s;

    if (Class[UCHAR(*str)] != C_ID_STRING) {
	return(FALSE);
    }

    for (s = str; *s != '\0'; s++) {
	if (!InIdent[UCHAR(*s)]) {
	    return(FALSE);
	}
    }

    return(TRUE);
}

