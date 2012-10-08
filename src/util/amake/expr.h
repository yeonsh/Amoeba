/*	@(#)expr.h	1.3	94/04/07 14:50:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

EXTERN struct expr *NullExpr, *DiagExpr, *IgnoreExpr;
EXTERN struct expr *EmptyListStringExpr, *EmptyListFileExpr;
EXTERN struct expr *TrueExpr, *FalseExpr, *UnKnown;

typedef struct expr *((*CODE) P(( struct expr *expr )));

#define IsTrue(bool) ((bool)->e_number == TTRUE)
#define IsFalse(bool) ((bool)->e_number == FFALSE)

/*
 * Een expressie wordt gerepresenteerd door een boom bestaande uit nodes van
 * het volgende type:
 */

struct expr {
    short e_number;			/* token nummer		*/
    short e_type;			/* type v.d. expressie	*/
    union {
	struct {			/* operator node 	*/
	    struct expr *e__left,	/* left argument	*/
	    		*e__right;	/* right argument(NULL if unary oper)*/
	} e_operator;
	struct idf *e__idp;		/* ID in tool/function calls */
	char *e__string;		/* STRING */
	struct variable *e__variable;	/* REFID */
	struct param *e__param;		/* ID as parameter of tool/cluster */
	struct object *e__object;	/* Internal Amake object */
	struct job *e__job;		/* job computing value for this node */
	struct {			/* function/tool call */
	    struct idf	 *e__func;
	    struct slist *e__args;
	} e_call;
	struct slist *e__list;
	generic e__pointer;		/* POINTER */
	long e__integer;		/* INTEGER */
    } e_inf;
};
#define e_left		e_inf.e_operator.e__left
#define e_right		e_inf.e_operator.e__right
#define e_idp		e_inf.e__idp
#define e_string	e_inf.e__string
#define e_param		e_inf.e__param
#define e_code		e_inf.e__code
#define e_object	e_inf.e__object
#define e_variable	e_inf.e__variable
#define e_job		e_inf.e__job
#define e_func		e_inf.e_call.e__func
#define e_args		e_inf.e_call.e__args
#define e_list		e_inf.e__list
#define e_pointer	e_inf.e__pointer
#define e_integer	e_inf.e__integer

#define MIN_PRIO	1
#define MAX_PRIO	7

EXTERN struct expr *MakeCallExpr P(( struct idf *func ));

/* optimised get_id: */
EXTERN struct idf *do_get_id P(( struct expr *id_expr ));
#define get_id(e)	(((e)->e_number == ID) ? (e)->e_idp : do_get_id(e))

EXTERN char *do_get_string P(( struct expr *str_expr ));

EXTERN short priority[];
/*
 * priority[tok_num] levert de prioriteit van het token met nummer tok_num.
 */

#ifdef DEBUG
#define DBUG_Expr(e) PrExpr(e)
#else
#define DBUG_Expr(e)
#endif

#define IsShared(e)	((e)->e_number == '}')
#define IsList(e)	((e)->e_number == '{' || (e)->e_number == '}')

void expr_init		P((void));
void put_expr_list	P((struct slist *list ));
void put_expr		P((struct expr *e ));
void AddParam		P((struct expr *call , struct param *param ));
void put_expr_node	P((struct expr *node ));
void ReportExprs	P((void));
void MakeShared		P((struct expr *list ));
void PrList		P((struct slist *list ));
void PrExpr		P((struct expr *e ));
int  PrintExpres 	P((struct expr *e ));
char *do_get_string	P((struct expr *str_expr ));

struct expr *new_expr		P((int tok ));
struct expr *empty_list		P((void));
struct expr *MakeCallExpr	P((struct idf *func ));
struct expr *StringExpr		P((char *s ));
struct expr *IntExpr		P((long l ));
struct expr *CopyExpr		P((struct expr *e ));
struct expr *CopySharedExpr	P((struct expr *e ));
struct idf  *do_get_id		P((struct expr *id_expr ));
struct param *dummy_param	P((void));

