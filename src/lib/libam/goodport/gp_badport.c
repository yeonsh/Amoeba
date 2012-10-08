/*	@(#)gp_badport.c	1.2	94/04/07 10:02:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This is part of the module for avoiding timeout errors when doing repeated
 * transactions to bad ports.  Bad ports are defined as ones that have
 * previously given us RPC_NOTFOUND errors.
 *
 * If operation == GP_APPEND, add the port to a list of bad ports.  Remove it,
 * if operation == GP_DELETE.  If operation == GP_LOOKUP, just return Boolean
 * indicating whether it is in list.  GP_INIT clears the list.
 *
 * On GP_APPEND, if list is full, delete oldest entry from list before
 * adding the new entry.  This ensures that the most recently used
 * bad ports are in the list.
 */
#include <amoeba.h>
#include <module/mutex.h>
#include <module/goodport.h>

int
gp_badport(portp, operation)
    port *portp;
    int operation;
{
    int j, found_it;

    static int num_badports = 0;
    static port badports[10];
    static mutex mu;

    mu_lock(&mu);

    if (operation == GP_INIT) {
	j = 0;
	found_it = 1;
    } else {
	/* Search for the port in current list of bad ports: */
	for (j = 0; j < num_badports; j++)
	    if (PORTCMP(portp, &badports[j]))
		break;
	found_it = j < num_badports;
    }

    switch (operation) {
    	case GP_APPEND:
	    if (!found_it) {
		/* Ensure that there is room, deleting oldest if necessary: */
		if (num_badports >= sizeof(badports)/sizeof(*badports)) {
		    /* Remove oldest from list; shift following ports down: */
		    for (j = 0 ; j < num_badports - 1; j++)
			badports[j] = badports[j + 1];
		    --num_badports;
		}
		/* Add to list: */
		badports[num_badports++] = *portp;
	    }
	    break;
        case GP_DELETE:
	    if (found_it) {
		/* Remove from list, shifting following ports downward: */
		for ( ; j < num_badports - 1; j++)
		    badports[j] = badports[j + 1];
		--num_badports;
	    }
	    break;
        case GP_INIT:
	    num_badports = 0;
	    break;
    }

    mu_unlock(&mu);
    return found_it;
}
