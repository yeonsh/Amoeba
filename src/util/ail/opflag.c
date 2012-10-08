/*	@(#)opflag.c	1.2	94/04/07 14:34:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

/*
 *	Operator flagging module, active while generating.
 *	Can keep up to sizeof(int) * 8 flags for each operator
 */

static struct op_flag {
    int of_opno;	/* Which operator	*/
    int of_flags;	/* Which flags		*/
} op_array[200];

static int n_ops;	/* How many are valid	*/
static int all_flags;	/* Any flag we saw	*/

/*
 *	Set a flag
 */
void
SetFlag(op, bit)
    int op, bit;
{
    register int i;
    all_flags |= bit;
    /* Do we keep flags on this operator yet?	*/
    for (i = n_ops; i--; ) if (op_array[i].of_opno == op) break;
    if (i < 0) { /* No we don't */
	if (n_ops >= sizeof(op_array)/sizeof(op_array[0])) {
	    mypf(ERR_HANDLE, "%r too many operators (SetFlag)\n");
	    return;
	}
	i = n_ops++;	/* Create a new entry */
	op_array[i].of_opno = op;
    }
    op_array[i].of_flags |= bit;
} /* SetFlag */

/*
 *	Get an operator's flags
 */
int
GetFlags(op)
    int op;
{
    register int i;
    for (i = n_ops; i--; ) if (op_array[i].of_opno == op) break;
    return (i < 0) ? 0 : op_array[i].of_flags;
} /* GetFlags */

/*
 *	Get any flag we set so far
 */
GetAnyFlag()
{
    return all_flags;
} /* GetAnyFlag */

/*
 *	Discard all flags
 */
void
UnFlagOp()
{
    all_flags = n_ops = 0;
} /* UnFlagOp */
