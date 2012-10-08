/*	@(#)generic.c	1.3	94/04/07 14:50:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* portions from: */
/* cs [file] - C-program bracket & parenthesis checker	*/
/* (c) Erik Baalbergen - june 9, 1983; revised juli 1, 1987	*/

#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "alloc.h"
#include "idf.h"
#include "scope.h"
#include "error.h"
#include "input.h"
#include "lexan.h"
#include "expr.h"
#include "slist.h"
#include "dbug.h"
#include "dump.h"
#include "buffer.h"
#include "assignment.h"
#include "execute.h"
#include "generic.h"

/*
 * syntax of generic construct:
 *
 * GENERIC ID '(' ID [',' ID]* ')' '{'
 *     <Amake Constructions>
 * '}'
 *
 * syntax of generic instantiation:
 *
 * INSTANCE ID '(' <expr> [',' <expr>]* ')'
 *
 * example:
 * GENERIC deftype(Type, Pattern) {
 *     DERIVE f [type = $Type]           WHEN matches($f, $Pattern);
 *     DERIVE f [def-pat = $Pattern]     WHEN get($f, type) == $Type;
 * };
 *
 * INSTANCE deftype(C-incl, '%.h');
 * INSTANCE deftype(C-src, '%.c');
 * INSTANCE deftype(loadable, '%.o');
 */

PRIVATE int
GenericSkip(ch)
char ch;
{
    register int c;

    LoadChar(c);
    while (c != ch) {
	if (c == EOI) {
	    return(FALSE);
	} else if (c == '\n') {
	    HandleNewLine();
	}
	BufferAdd(c);
	LoadChar(c);
    }
    BufferAdd(c);
    return(TRUE);
}

#define STACKSIZE 1000

PRIVATE void
push(stack, sp, elem)
int *stack, *sp, elem;
{
    if ((*sp)++ >= STACKSIZE) {
	FatalError("nesting too deep in generic");
    }
    stack[*sp] = elem;
}

PRIVATE int
pop(stack, sp)
int *stack, *sp;
{
    return (*sp < 0) ? 0 : stack[(*sp)--];
}

#define EmptyBody() BufferFailure()

PUBLIC char *
ReadBody()
/* Return text between '{' and '}' in input. '{' is supposed to be read
 * already, the '}' will be left for the parser.
 */
{
    int pstack[STACKSIZE], bstack[STACKSIZE], bsp, psp, r;
    register int c;

    BufferStart();
    bsp = psp = -1;
    push(bstack, &bsp, file_info.f_line); /* starting '{' already read */
    LoadChar(c);
    while (c != EOI) {
	switch(c) {
	case '\n':
	    /* don't leave newlines in bodies, they confuse line numbering. */
	    BufferAdd(' ');
	    HandleNewLine();
	    break;
	case '#':
	    LoadChar(c);
	    while (c != '\n') {
		if (c == EOI) {
		    LexError("EOF in body");
		    return(EmptyBody());
		}
		LoadChar(c);
	    }
	    BufferAdd(' ');
	    HandleNewLine();
	    break;
	case '\'':
	    BufferAdd(c);
	    if (!GenericSkip('\'')) {
		LexError("in body: EOF in string constant");
		return(EmptyBody());
	    }
	    break;
	case '{':
	    push(bstack, &bsp, file_info.f_line);
	    BufferAdd(c);
	    break;
	case '(':
	    push(pstack, &psp, file_info.f_line);
	    BufferAdd(c);
	    break;
	case ')':
	    if (pop(pstack, &psp) == 0) {
		LexError("')' deleted");
	    } else {
		BufferAdd(c);
	    }
	    break;
	case '}':
	    r = pop(bstack, &bsp);
	    assert(r != 0);
	    if (bsp < 0) { /* closing '}' of generic construct, don't add */
		while (r = pop(pstack, &psp)) {
		    LexError("in body: missing ')' (line %d)", r);
		}
		PushBack(); /* let the parser have this one */
		return(BufferResult());
	    }
	    BufferAdd(c);
	    break;
	default:
	    BufferAdd(c);
	}
	LoadChar(c);
    }
    while (r = pop(pstack, &psp)) {
	LexError("in body: missing ')' (line %d)", r);
    }
    while (r = pop(bstack, &bsp)) {
	LexError("in body: missing '}' (line %d)", r);
    }
    return(EmptyBody());
}

PUBLIC void
MakeInstance(instance)
struct expr *instance;
{
    register struct cons *cons;
    register struct Generic *gen = instance->e_func->id_generic;

    DBUG_PRINT("generic", ("inserting `%s'", gen->gen_text));

    /* Generate assignments to the parameters. They will be referred to
     * by the generic body as if it were global variables.
     * Pretty sophisticated parameter mechanism, huh?
     */
    for (cons = Head(instance->e_args); cons != NULL; ) {
	register struct expr *arrow = ItemP(cons, expr);
	register struct cons *next = Next(cons);
	struct idf *var_idp = arrow->e_left->e_param->par_idp;

	DeclareIdent(&var_idp, I_VARIABLE);
	var_idp->id_flags |= F_QUIET; /* I know it is redefined, don't shout!*/
	EnterVarDef(var_idp, arrow->e_right);
	put_expr_node(arrow);
	Remove(&instance->e_args, cons);
	cons = next;
    }

    StartExecution((struct job *)NULL); /* do evaluate them */

    DoInsertText(gen->gen_text);
}

