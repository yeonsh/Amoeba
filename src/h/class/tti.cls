/*	@(#)tti.cls	1.3	94/04/06 15:54:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "server/tty/tty.h"

class tti [TTI_INTERRUPT .. TTI_SIGNAL] {
	tti_interrupt [ TTI_INTERRUPT ] (*,
		in	char intrchars[1]);
	/* An interrupt character has been hit.
	   (In theory, more than one character may be passed,
	   but we ignore that possibility for now.) */

	tti_signal [ TTI_SIGNAL ] (*,
		in:h_extra	int sig);
};
