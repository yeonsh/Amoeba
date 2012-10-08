/*	@(#)cache.c	1.2	94/04/07 15:04:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "module/prv.h"

#include "du.h"

/*
 * Capability cache module, needed to avoid counting objects more than once.
 *
 * Implementation: for each port, keep the object numbers in lists,
 * hashed on the lowest bits.
 */

typedef struct obj_node {
    objnum	     obj_num;
    struct obj_node *obj_next;
} obj_node;

#define NUM_ALLOC	(((8 * 1024) - 12) / sizeof(obj_node))

static obj_node *
new_obj_node()
{
    static int left_in_chunck = 0;
    static obj_node *obj_chunck = NULL;

    if (left_in_chunck == 0) {
	/* allocate new chunck; this avoids per struct malloc() overhead */
	obj_chunck = (obj_node *)malloc(NUM_ALLOC * sizeof(obj_node));
	if (obj_chunck == NULL) {
	    fatal("out of memory");
	}
	left_in_chunck = NUM_ALLOC;
    }

    return &obj_chunck[--left_in_chunck];
}

static int
list_insert(list, obj_num)
obj_node **list;
objnum obj_num;
{
    register struct obj_node **pre, *cur;

    for (pre = list, cur = *list;
	 cur != NULL && cur->obj_num < obj_num;
	 pre = &cur->obj_next, cur = cur->obj_next)
	;

    if (cur != NULL && cur->obj_num == obj_num) {
	return 0;	/* already in list */
    } else {		/* add to list */
	struct obj_node *obj = new_obj_node();

	obj->obj_next = cur;
	obj->obj_num = obj_num;
	*pre = obj;
	return 1;
    }
}

#define HASH_BITS	9
#define HASH_MASK	((1<<(HASH_BITS+1))-1)

struct port_obj {
    port	     pobj_port;
    struct port_obj *pobj_next;
    obj_node 	    *pobj_hash[HASH_MASK+1]; /* hash on last HASH_BITS bits */
} *pobj_first = NULL, *pobj_last = NULL;

int
cache_cap(cap)
capability *cap;
/*
 * Return 0 if it already was in the cache, 1 if we added it.
 */
{
    struct port_obj *pobj;
    objnum obj_num;

    for (pobj = pobj_first; pobj != NULL; pobj = pobj->pobj_next) {
	if (PORTCMP(&cap->cap_port, &pobj->pobj_port)) {
	    break;
	}
    }

    if (pobj == NULL) {
	register int i;

	/* no pobj for this server yet; add new one to port_first list */
	pobj = (struct port_obj *)malloc(sizeof(struct port_obj));
	if (pobj == NULL) {
	    fatal("out of memory");
	}

	pobj->pobj_port = cap->cap_port;
	pobj->pobj_next = NULL;
	for (i = 0; i <= HASH_MASK; i++) {
	    pobj->pobj_hash[i] = NULL;
	}

	/*
	 * Add them to the tail; rarely used ports will then end up
	 * at the end, most of the time.
	 */
	if (pobj_first == NULL) {
	    pobj_first = pobj_last = pobj;
	} else {
	    pobj_last->pobj_next = pobj;
	    pobj_last = pobj;
	}
    }

    obj_num = prv_number(&cap->cap_priv);
    return list_insert(&pobj->pobj_hash[obj_num & HASH_MASK], obj_num);
}

