/*	@(#)dummy.c	1.2	94/04/07 14:32:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"

/*
 * Dummy act routine - from '{' to '}'.
 * Assumes '_priv' is the private pointer, and _arg is an objnum *
 */

static char *act_str[] = {
    "{%>%n",
	"objnum obj;%n",
	"rights_bits rights;%n",
	"if ((obj = prv_number(_priv)) < 0%n",
	"|| obj >= MAX_OBJECTS%n",
	"|| !obj_tab[obj].inuse]%n",
	"|| prv_encode(_priv, &rights, &obj_tab[obj].random)) {%>%n",
	    "MON_EVENT(\"act: bad cap\");%n",
	    "return STD_CAPBAD;%<%n",
	"}%n",
	"*_arg = obj;%n",
	"return STD_OK;%<%n",
    "}%n",
    NULL
}, *deact_str[] = {
    "{%>%n",
    "}%n",
    NULL,
};

void
dummy_act(fp)
    handle fp;
{
    mypf(fp, "errstat\n%s", ActFunc);
    switch (lang) {
    case L_TRADITIONAL:
	mypf(fp, "(_priv, _arg)%>%nprivate *_priv;%nobjnum *_arg;%<\n");
	break;
    case L_ANSI:
	mypf(fp, "(private *_priv, objnum *_arg)\n");
	break;
    default:
	myfp(ERR_HANDLE, "%r (%s:%d) unknown language\n", __FILE__, __LINE__);
	break;
    }
}
