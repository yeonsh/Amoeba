/*	@(#)slist.h	1.3	96/02/27 13:06:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* generic "structure list" structure */

struct cons {
    generic      co_item;
    struct cons *co_next,
		*co_prev;
};

/* allocation definitions of struct cons */
#include "structdef.h"
DEF_STRUCT(struct cons, h_cons, cnt_cons)
#define new_cons()	NEW_STRUCT(struct cons, h_cons, cnt_cons, 80)
#define free_cons(p)	FREE_STRUCT(struct cons, h_cons, p)

struct slist {
    struct cons *sl_first,
		*sl_last;
};

/* allocation definitions of struct slist */
DEF_STRUCT(struct slist, h_slist, cnt_slist)
#define new_slist()	NEW_STRUCT(struct slist, h_slist, cnt_slist, 10)
#define free_slist(p)	FREE_STRUCT(struct slist, h_slist, p)

#define IsEmpty(slist)	 ((slist) == NULL)
#define Head(slist)	 (IsEmpty(slist) ? NULL : ((slist)->sl_first))
#define Tail(slist)	 (IsEmpty(slist) ? NULL : ((slist)->sl_last))
/* Use HeadOf/TailOf if it is certain that the list isn't empty, or when it
 * it is used as lvalue.
 */
#define HeadOf(slist)	 ((slist)->sl_first)
#define TailOf(slist)	 ((slist)->sl_last)
#define Item(cons)	 ((cons)->co_item)
#define ItemP(cons, rec) ((struct rec *)Item(cons))
#define Next(cons)	 ((cons)->co_next)
#define Prev(cons)	 ((cons)->co_prev)

#define Append(slistpp, itemp)	DoAppend(slistpp, (generic) itemp)

/* inline iteration (avoids n+1 function calls) : */
#define ITERATE(list, rec, item, statement)			\
{								\
    register struct cons *cons;					\
								\
    for (cons = Head(list); cons != NULL; cons = Next(cons)) {	\
	register struct rec *item = ItemP(cons, rec);		\
								\
	statement;						\
    }								\
}

#define RemoveHead(listp, rec) (struct rec *)DoRemoveHead(listp)
    
typedef void (*Iterator) P((generic item));

void DoAppend	P((struct slist **list , generic item ));
void Insert	P((struct slist **list , struct cons *before , generic item ));
void AppendList	P((struct slist **listp , struct slist *list ));
void Iterate	P((struct slist *list , Iterator fun ));
void Remove	P((struct slist **list , struct cons *cons ));
void Splice	P((struct slist **list, struct cons *c, struct slist *insert));
int  Length	P((struct slist *list ));

generic DoRemoveHead		P((struct slist **listp ));
struct slist *DestructiveAppend	P((struct slist *left , struct slist *right ));

