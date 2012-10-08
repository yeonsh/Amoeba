/*	@(#)tools.h	1.2	94/04/07 14:55:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


#define TL_USES_TEMPS	0x01	/* tool uses temporary files */

struct tool {
    struct idf		*tl_idp;
    int			tl_flags;
    struct scope	*tl_scope;		/* parameters & io spec */
    struct expr		*tl_condition;
    struct expr		*tl_trigger;
    struct expr		*tl_pre;
    struct expr		*tl_action;
    struct expr		*tl_post;
    struct slist        *tl_in_attr;
    struct slist        *tl_out_attr;
} *new_tool P(( void ));

/* allocation definitions of struct tool */
#include "structdef.h"
DEF_STRUCT(struct tool, h_tool, cnt_tool)
#define new_tool()	NEW_STRUCT(struct tool, h_tool, cnt_tool, 10)
#define free_tool(p)	FREE_STRUCT(struct tool, h_tool, p)

EXTERN struct slist *ToolList;

void EnterTool P((struct tool *tool ));
