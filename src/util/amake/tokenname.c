/*	@(#)tokenname.c	1.2	94/04/07 14:55:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* T O K E N   D E F I N I T I O N S */

#include	"global.h"
#include	"Lpars.h"
#include	"idf.h"

/*	T O K E N N A M E   S T R U C T U R E
 *
 *	Used for defining the name of a
 *	token as identified by its symbol
 */
struct tokenname	{
	int tn_symbol;
	char *tn_name;
};

/*	To centralize the declaration of %tokens, their presence in this
	file is taken as their declaration. The Makefile will produce
	a grammar file (tokenfile.g) from this file. This scheme ensures
	that all tokens have a printable name.
	Also, the "symbol2str.c" file is produced from this file.
*/

#if 0
PRIVATE
struct tokenname tkspec[] =	{	/* the names of the special tokens */
	{ID,        "identifier"},
	{ID_STRING, "string"},
	{REFID,     "variable"},
	{INTEGER,   "integer"},
	{0,	    ""}
};
#endif

#if 0
PRIVATE
struct tokenname tkcomp[] =	{	/* names of the composite tokens */
	{EQ,	 "=="},
	{NOT_EQ, "<>"},
	{ARROW,	 "=>"},
	{0,	 ""}
};
#endif

#define MAX_ID_LEN 12		/* no token larger than this */

PRIVATE
struct tokenname tkidf[] =	{	/* names of the identifier tokens */
	{AND,"and"},
	{BOOLEAN,"boolean"},
	{CLUSTER,"cluster"},
	{COMPUTED,"computed"},
	{CONDITION,"condition"},
	{CONFORM,"conform"},
	{DECLARE,"declare"},
	{DERIVE,"derive"},
	{DEFAULT,"default"},
	{DIAG,"diag"},
	{DO,"do"},
	{EXPORT,"export"},
    {FFALSE,"false"},
        {GENERIC, "generic"},
	{IF,"if"},
	{IGNORE,"ignore"},
	{IMPORT, "import"},
	{IN,"in"},
	{INCLUDE,"include"},
    	{INSTANCE,"instance"},
	{LIST,"list"},
	{NOT,"not"},
	{OR,"or"},
	{OUT,"out"},
	{POST,"post"},
	{PRE,"pre"},
	{RETURN,"return"},
	{SOURCES,"sources"},
	{STRING,"string"},
	{TARGETS,"targets"},
	{TMP,"tmp"},
	{TOOL,"tool"},
	{TRIGGER,"trigger"},
    {TTRUE,"true"},
	{UNKNOWN,"unknown"},
	{USE,"use"},
	{WHEN,"when"},
	{0, ""}
};

/* Some routines to handle tokennames */

#include <ctype.h>

PRIVATE void
reserve(resv)
	register struct tokenname *resv;
{
    /* The names of the tokens described in resv are entered
     * as reserved words. Lower case versions are added only conditonally,
     * because they require the user being very carefull to quote them
     * when specifying a pathname containing one.
     */
    register struct idf *p;

    while( resv->tn_symbol )	{
#ifdef ALLOW_UPPERCASE_KEYWORDS
	char upper[MAX_KEY_LENGTH];
#endif
	p = str2idf(resv->tn_name, 0);
	p->id_token = resv->tn_symbol;

#ifdef ALLOW_UPPERCASE_KEYWORDS
	{   register char *up, *s;
	    /* now lower case version */
	    for (s = resv->tn_name, up = upper; *s != '\0'; ) {
		*up++ = toupper(*s++);
	    }
	    *up = '\0';
	    p = str2idf(upper, 1);
	    p->id_token = resv->tn_symbol;
	}
#endif
		
	resv++;
    }
}

PUBLIC void
init_symbol_table()
/* Initialiseert de symbol table. */
{
    init_idf();
    reserve(tkidf);
}
