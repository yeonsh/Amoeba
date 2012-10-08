/*	@(#)scope.h	1.2	94/04/07 14:53:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

struct param {
    struct idf       *par_idp;		/* parameter name */
    int		      par_flags;
    struct type      *par_type;
    struct expr      *par_default;	/* default value of this parameter */
    struct expr	     *par_value;	/* current value of this parameter */
    struct objclass  *par_class;	/* attribute class of object */
    union {
        struct idf   *par__conform;	/* links bool option and use of file */
        struct idf   *par__computed;	/* used in programs like `mkdep' */
    } par_conf_comp;
    struct expr      *par_expr;		/* expression pointing to this node */
} *new_param P(( void ));

#define par_conform  par_conf_comp.par__conform
#define par_computed par_conf_comp.par__computed

#define PAR_CONFORM  0x01
#define PAR_COMPUTED 0x02
#define PAR_REFCOMP  0x04 /* referred to by a PAR_COMPUTED parameter */

/* allocation definitions of struct param */
#include "structdef.h"
DEF_STRUCT(struct param, h_param, cnt_param)
#define new_param()   NEW_STRUCT(struct param, h_param, cnt_param, 10)
#define free_param(p) FREE_STRUCT(struct param, h_param, p)

/* Scopes are numbered from GLOBAL_SCOPE onward. */
#define GLOBAL_SCOPE	0

struct scope {
    struct slist *sc_scope;
    int		 sc_number;
};

/* allocation definitions of struct scope */
DEF_STRUCT(struct scope, h_scope, cnt_scope)
#define new_scope()   NEW_STRUCT(struct scope, h_scope, cnt_scope, 10)
#define free_scope(p) FREE_STRUCT(struct scope, h_scope, p)

EXTERN struct scope *CurrentScope;
/* CurrentScope is used to gather all parameters of a Tool or Cluster together.
 * When such an header is finished, Currentscope (the pointer, not all whats
 * in it) may be copied, for later use, when the Tool or Macro is called.
 */

void NewScope		P((void));
void DeclareIdent	P((struct idf **idp , int kind ));
void ClearScope		P((void));
void RemoveScope	P((void));
void AddScope		P((struct scope *scope ));
void ResetScopes	P((void));
void InitScope		P((void));
void PrintScope		P((struct scope *scope ));
/* Prints out all known information from 'scope' when DoPrintScopes from
 * main.h is TRUE.
 */

