/*	@(#)qelem.h	1.2	94/04/07 10:38:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

struct qelem {
	struct qelem	*q_forw;
	struct qelem	*q_back;
	/*char		q_data[]; */
};
