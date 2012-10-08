/*	@(#)caching.h	1.2	94/04/07 14:47:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


EXTERN struct idf *Id_cid;
EXTERN int CacheScopeNr;
EXTERN int CacheChanged;

EXTERN struct idf
    *Id_computed,	/* boolean attribute: object used as computed input */
    *Id_changed,	/* used to fake object change while F_no_exec */
    *Id_compare,	/* when set on output causes Amake to compare with
			 * previous version (if available)
			 */
    *Id_installed;	/* boolean attribute: don't remove this object */

/* It might be more convenient/efficient to also include in & outputs as a
 * seperate list in struct `cached'.
 */
struct cached {
    struct expr   *c_invocation;  /* instantiated tool header */
    struct expr   *c_result;
    struct job    *c_working;     /* running invocation, let equiv. wait */
    int 	   c_scope_nr;
    int		   c_flags;
    struct cached *c_same_input;  /* list of entries with same first input */
    struct cached *c_next;	  /* links all cached structs together */
};

/* allocation definitions of struct cached */
#include "structdef.h"
DEF_STRUCT(struct cached, h_cached, cnt_cached)
#define new_cached()        NEW_STRUCT(struct cached, h_cached, cnt_cached, 10)
#define free_cached(p)      FREE_STRUCT(struct cached, h_cached, p)

#define CH_WORKING 	0x01 /* result is being produced right now	    */
#define CH_CACHED  	0x02 /* fresh entry from statefile		    */
#define CH_NEW_ENTRY	0x04 /* invocation not in cache			    */
#define CH_RERUN	0x08 /* at least one of the inputs has changed	    */
#define CH_PICKUP	0x10 /* more than one invocation waiting for result */
#define CH_USED		0x20 /* cache entry used in this Amake run	    */
#define CH_PRESENTED	0x40 /* entry presented to statefile module         */
#define CH_DUMPED	0x80 /* entry dumped to statefile                   */

EXTERN struct cached *FirstCacheEntry; /* head of cached list */

void TouchFile		P((char *name ));
void CacheCommit	P((void));
int RetainCacheEntry	P((struct cached *c ));
int FileExists		P((struct object *obj ));
void EnterStateEntry	P((struct expr *inv_result ));
void RemoveOutputs	P((struct expr *inv ));
void LookInCache	P((struct invocation *inv ));
void HandleOutputs	P((struct expr *invocation , struct cached *cached ,
			   struct expr **result , int scope_nr ));
void InitCaching	P((void));

void RemoveIntermediates   P((void));
void InstallGeneratedInput P((struct object *wanted , struct object *cached ));
struct expr * QuickValueId P((struct object *obj ));
