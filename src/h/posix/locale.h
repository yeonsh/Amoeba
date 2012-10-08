/*	@(#)locale.h	1.2	94/04/06 16:53:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * locale.h - localization
 */

#if	!defined(_LOCALE_H)
#define	_LOCALE_H

struct lconv {
	char	*decimal_point;		/* "." */
	char	*thousands_sep;		/* "" */
	char	*grouping;		/* "" */
	char	*int_curr_symbol;	/* "" */
	char	*currency_symbol;	/* "" */
	char	*mon_decimal_point;	/* "" */
	char	*mon_thousands_sep;	/* "" */
	char	*mon_grouping;		/* "" */
	char	*positive_sign;		/* "" */
	char	*negative_sign;		/* "" */
	char	int_frac_digits;	/* CHAR_MAX */
	char	frac_digits;		/* CHAR_MAX */
	char	p_cs_precedes;		/* CHAR_MAX */
	char	p_sep_by_space;		/* CHAR_MAX */
	char	n_cs_precedes;		/* CHAR_MAX */
	char	n_sep_by_space;		/* CHAR_MAX */
	char	p_sign_posn;		/* CHAR_MAX */
	char	n_sign_posn;		/* CHAR_MAX */
};

#ifdef __STDC__
#define NULL	((void *)0)
#else
#ifndef NULL
#define NULL	0
#endif
#endif /* __STDC__ */

#define	LC_ALL		1
#define	LC_COLLATE	2
#define	LC_CTYPE	3
#define	LC_MONETARY	4
#define	LC_NUMERIC	5
#define	LC_TIME		6

#include "_ARGS.h"

char         *setlocale  _ARGS((int _category, const char *_locale));
struct lconv *localeconv _ARGS((void));

#endif	/* _LOCALE_H */
