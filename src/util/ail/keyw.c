/*	@(#)keyw.c	1.2	94/04/07 14:33:24 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# ifdef YYDEBUG
extern yydebug;
# endif

static struct {
    char *kw_litt;
    int kw_tok; /* Token to be returned if keyword is found. */
} keyword[] = {
    { "char",		CHAR	},
# define FIRSTKEYW CHAR
    { "class",		CLASS	},
    { "const",		CONST	},
    { "double",		DOUBLE	},
    { "enum",		ENUM	},
    { "extern",		EXTERN	},
    { "float",		FLOAT	},
    { "generate",	GENERATE },
    { "in",		IN	},
    { "include",	INCLUDE },
    { "inherit",	INHERIT	},
    { "int",		INT	},
    { "long",		LONG	},
    { "marshal",	MARSHAL	},
    { "out",		OUT	},
    { "rights",		RIGHTS	},
    { "short",		SHORT	},
    { "signed",		SIGNED	},
/*
    { "string",		STRING	},
*/
    { "struct",		STRUCT	},
    { "typedef",	TYPEDEF	},
    { "union",		UNION	},
    { "unsigned",	UNSIGNED },
    { "var",		VAR	},
    { "void",		VOID	},
    { "with",		WITH	}
# define LASTKEYW WITH
};

/*
 * Binary search for keyword s; returns -1 if not found.
 * Lo is the lowest, hi-1 the highest possible index in keyword[].
 */
int
KeywLookup(s)
    char *s;
{
    int lo, mid, hi;
# ifdef YYDEBUG
    if (yydebug) mypf(OUT_HANDLE, "%s\n", s);
# endif
    lo = 0; hi = sizeof(keyword)/sizeof(keyword[0]);
    while (hi > lo) {
	int i;
	mid = (lo + hi)/2;
	i = strcmp(s, keyword[mid].kw_litt);
	if (i < 0) hi = mid;
	else if (i > 0) lo = mid + 1;
	else return keyword[mid].kw_tok;	/* Found. */
    }
    return -1;
}

/*
 *	Make the human-readable version of a token.
 *	As this routine is also used by some code-
 *	generators, it is neccesary that both operators
 *	and keywords are not quoted.
 *	Because the routine is used to generate the
 *	error messages, is must also be capable of
 *	spelling 'identifier'.
 */
char *
TokenToString(t)
    int t;
{
    static char charlike[2] = " ";	/* Initializes the trailing \0 */
    static char s[16];	/* Enough to hold keywords. */
    if (0<t && t<256) {
	charlike[0] = t;
	return charlike;
    }
    /* assert(t>=0); */
    switch(t) {
    case 0:		return "end-of-file";
    case IDENT:		return "identifier";
    case INTCONST:	return "integer-constant";
    case STRCONST:	return "string-litteral";
    case LEFT:		return "<<";
    case RIGHT:		return ">>";
    case AND:		return "&&";
    case OR:		return "||";
    case EQUAL:		return "==";
    case UNEQU:		return "!=";
    case LEQ:		return "<=";
    case GEQ:		return ">=";
    case DOUBLE_DOT:	return "..";
    default:
	/* It's a keyword. */
	/* assert(t <= LASTKEYW); */
	sprintf(s, "%s", keyword[t - FIRSTKEYW].kw_litt);
	return s;
    }
    /*NOTREACHED*/
} /* TokenToString */
