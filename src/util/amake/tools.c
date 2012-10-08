/*	@(#)tools.c	1.2	94/04/07 14:55:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "global.h"
#include "slist.h"
#include "type.h"
#include "expr.h"
#include "scope.h"
#include "tools.h"
#include "classes.h"

/* tools are kept on a tool list, which may be ITERATEd over */
PUBLIC struct slist *ToolList = NULL;


PRIVATE struct slist *
GetAttrList(tool, inout)
struct tool *tool;
int inout;
{
    struct slist *attrlist = NULL;
    struct type *arg_type;
    register struct cons *cons;

    for (cons = Head(tool->tl_scope->sc_scope); cons != NULL;
	 cons = Next(cons)) {
	register struct param *par = ItemP(cons, param);

	arg_type = par->par_type;
	if ((arg_type->tp_indicator & inout) != 0) {
	    /* T_LIST_OF variants are also cought */
	    if (arg_type->tp_attributes != NULL) {
		ITERATE(arg_type->tp_attributes->e_list, expr, arrow, {
		    Append(&attrlist, arrow->e_left->e_idp);
		});
	    }
	} else if ((arg_type->tp_indicator & T_TMP) != 0) {
	    tool->tl_flags |= TL_USES_TEMPS;
	}
    }
    return(attrlist);
}

PUBLIC void
EnterTool(tool)
struct tool *tool;
{
    Append(&ToolList, tool);
    tool->tl_in_attr = GetAttrList(tool, T_IN);
    tool->tl_out_attr = GetAttrList(tool, T_OUT);
    DeclareTool(tool);
    /* should eventually replace the current approach, i.e., this file
     * will become superfluous. While developing the algorithm just
     * keep the original version around, to avoid having neither
     * version running.
     */
}
