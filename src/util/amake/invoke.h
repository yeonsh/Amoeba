/*	@(#)invoke.h	1.2	94/04/07 14:51:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

EXTERN struct idf *Id_alias, *Id_calias, *Id_failed, *Id_implicit;

enum InvState { INV_START, INV_CHECK, INV_CACHE,
		INV_TRIGGER, INV_INVOKE, INV_END, INV_PICKUP };	

struct invocation {
    enum InvState inv_state;
    struct expr  *inv_call;
    struct expr  *inv_trigger;
    struct expr  *inv_action;
    struct idf   *inv_tool_id;
    struct slist *inv_inputs;
    struct slist *inv_outputs;
    struct slist *inv_temps;
    struct job   *inv_job;
    struct cached *inv_cache;
    int		  inv_scope_nr;
    int		  inv_input_size; /* `large' commands have high priority */
};

/* allocation definitions of struct invocation */
#include "structdef.h"
DEF_STRUCT(struct invocation, h_invocation, cnt_invocation)
#define new_invocation()   NEW_STRUCT(struct invocation, h_invocation, cnt_invocation, 20)
#define free_invocation(p) FREE_STRUCT(struct invocation, h_invocation, p)


void AllocInputs	P((struct slist *inputs ));
void Taint		P((struct object *obj ));
void PutParams		P((struct expr *call ));
void FutureOutput	P((struct slist *objects , struct job *job ,
			   int scope_nr ));
void DetermineIO	P((struct expr *header , struct slist **inputs ,
			   struct slist **outputs ));
void Invoke		P((struct expr *header , struct slist *inputs ,
			   struct slist *outputs ));
void InvokeAfterwards	P((struct invocation *inv ));
void InitInvokations	P((void));
int  AddUsage		P((struct object *obj , struct job *job ));
int  MarkInputs		P((struct slist *inputs , struct job *job ));
long InputSize		P((struct job *job ));

struct job *InvokationJob   P((struct expr *header , struct slist *inputs ,
			     struct slist *outputs, struct invocation **invp));
struct expr *InvCall	    P((struct invocation *inv ));
struct slist **InvInputPtr  P((struct invocation *inv ));
struct slist **InvOutputPtr P((struct invocation *inv ));

