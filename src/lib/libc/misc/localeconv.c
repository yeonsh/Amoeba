/*	@(#)localeconv.c	1.2	94/04/07 10:48:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * localeconv - set components of a struct according to current locale
 */

#include	<limits.h>
#include	<locale.h>

extern struct lconv _lc;

struct lconv *
localeconv()
{
	register struct lconv *lcp = &_lc;

	lcp->decimal_point = ".";
	lcp->thousands_sep = "";
	lcp->grouping = "";
	lcp->int_curr_symbol = "";
	lcp->currency_symbol = "";
	lcp->mon_decimal_point = "";
	lcp->mon_thousands_sep = "";
	lcp->mon_grouping = "";
	lcp->positive_sign = "";
	lcp->negative_sign = "";
	lcp->int_frac_digits = CHAR_MAX;
	lcp->frac_digits = CHAR_MAX;
	lcp->p_cs_precedes = CHAR_MAX;
	lcp->p_sep_by_space = CHAR_MAX;
	lcp->n_cs_precedes = CHAR_MAX;
	lcp->n_sep_by_space = CHAR_MAX;
	lcp->p_sign_posn = CHAR_MAX;
	lcp->n_sign_posn = CHAR_MAX;

	return lcp;
}
