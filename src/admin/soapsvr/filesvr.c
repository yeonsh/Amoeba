/*	@(#)filesvr.c	1.3	96/02/27 10:22:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * filesvr.c: maintains the status of the (bullet) file servers used to
 * store directory files.
 */

#include "amoeba.h"
#include "stderr.h"
#include "module/stdcmd.h"
#include "module/ar.h"
#include "monitor.h"
#include "assert.h"

#include "filesvr.h"
#include "global.h"
#include "main.h"


static int bull_preferred = -1; /* currently preferred bullet server */
static int bull_highest = -1;	/* server with highest (static) preference */

/* information we keep about every (bullet) file server used to store
 * per-directory information:
 */
struct bull_info {
    int		b_valid;	/* set to 1 if the next fields are valid   */
    capability	b_cap;		/* super capability of the bullet server   */
    int		b_up;		/* set to 1 if it was up until recently	   */
    int		b_pref;		/* static preference of this bullet server */
    int		b_next;		/* next one in priority list		   */
};

static struct bull_info bullet[NBULLETS];
static int nbullets;
static int bull_navail = 0;

#define valid_svr(i)	((0 <= (i)) && ((i) < nbullets) && bullet[i].b_valid)

static void
determine_bull_preferred()
/* A bullet server was found to be up/down, or a new one was added.
 * Determine a new preferred bullet server.
 */
{
    int i, best;

    best = -1;
    for (i = bull_highest; i >= 0; i = bullet[i].b_next) {
	if (bullet[i].b_up) {
	    best = i;
	    break;
	}
    }

    bull_preferred = best;	/* i.e., possibly -1 */
}

static void
server_status(which, up)
int which;
int up;
{
    assert(valid_svr(which));

    /* bring the number of available bullet servers up to date */
    if (!bullet[which].b_up && up) {		/* down->up */
	bull_navail++;
    } else if (bullet[which].b_up && !up) {	/* up->down */
	bull_navail--;
    }

    bullet[which].b_up = up;
    determine_bull_preferred();
}

/*
 * Mark Bullet servers having a given port as unavailable,
 * with appropriate error messages.
 *
 * TODO: currently it only keeps status about the current bullet servers
 * mentioned in the superblock.  But because of the "newbullet" command,
 * more than NBULLETS Bullet servers may be in actual use!
 */
void
fsvr_set_unavail(svrport)
port *svrport;
{
    int which;

    for (which = 0; which < nbullets; which++) {
	if (PORTCMP(&bullet[which].b_cap.cap_port, svrport)) {
	    if (bullet[which].b_up) {
		switch (which) {
		case 0:	 MON_EVENT("Bullet 0 unavailable"); break;
		case 1:  MON_EVENT("Bullet 1 unavailable"); break;
		}

		message("Bullet %d unavailable (port %s)",
			which, ar_port(&bullet[which].b_cap.cap_port));

		server_status(which, 0);
	    }
	}
    }
}

int
fsvr_avail(svrport)
port *svrport;
{
    int which;

    if (NULLPORT(svrport)) {
	return 0;
    }

    for (which = 0; which < nbullets; which++) {
	if (PORTCMP(&bullet[which].b_cap.cap_port, svrport)) {
	    return bullet[which].b_up;
	}
    }

    /* I don't know about this one */
    return 1;
}


int
fsvr_preferred()
{
    return bull_preferred;
}

int
fsvr_nr_avail()
{
    return bull_navail;
}

void
fsvr_order_caps(caps, ncaps, order)
capability caps[];
int        ncaps;
int 	   order[];
{
    long add_vec = 0;
    int  added = 0;
    int  i;

    /* put the preferred caps in front */
    for (i = fsvr_preferred(); i >= 0; i = bullet[i].b_next) {
	if ((i < ncaps) &&
	    bullet[i].b_up &&
	    PORTCMP(&bullet[i].b_cap.cap_port, &caps[i].cap_port))
	{
	    order[added++] = i;
	    add_vec |= 1 << i;
	}
    }

    /* Make a pass over the capabilities adding the ones that
     * could be up as well, but were not added.
     */
    for (i = 0; i < ncaps; i++) {
	if ((add_vec & (1 << i)) == 0 && fsvr_avail(&caps[i].cap_port)) {
	    order[added++] = i;
	}
    }

    /* terminate with -1, in case not all caps were added */
    if (added < ncaps) {
	order[added++] = -1;
    }
}

static void
fsvr_print_svrs()
{
#ifdef DEBUG
    int i;

    for (i = bull_highest; i >= 0; i = bullet[i].b_next) {
	message("bullet %d: pref=%d, up=%d, next=%d, cap=%s",
		i, bullet[i].b_pref, bullet[i].b_up, bullet[i].b_next,
		ar_cap(&bullet[i].b_cap));
    }
    message("bull_preferred=%d", bull_preferred);
#endif
}

void
fsvr_check_svrs()
{
    int j;

    /* Check whether any of the previously down file servers has become
     * available again, by doing a std_info on its super cap.
     * Returns the number of "up" file servers.
     */
    for (j = 0; j < nbullets; j++) {
	if (!bullet[j].b_up && !NULLPORT(&bullet[j].b_cap.cap_port)) {
	    /* Maybe it has been rebooted since the last time we checked */
	    char info[80];
	    int ignore;
	    errstat err;

	    err = std_info(&bullet[j].b_cap, info, (int)sizeof(info), &ignore);
	    if (err == STD_OK) {
		message("Bullet %d is up", j);
		server_status(j, 1);
	    }
	}
    }

    determine_bull_preferred();
}

void
fsvr_get_svrs(caplist, ncaps)
capability caplist[];
int	   ncaps;
{
    int i;

    for (i = 0; i < ncaps && i < nbullets; i++) {
	caplist[i] = bullet[i].b_cap;
    }

    /* zero the remaining ones requested */
    for (; i < ncaps; i++) {
	static capability NULLCAP;

	caplist[i] = NULLCAP;
    }
}

static void
insert(this, pref)
int this;
int pref;
{
    if (bull_highest < 0) {
	/* first one */
	bullet[this].b_next = -1;
	bull_highest = this;
    } else {
	int i, prev;

	for (prev = -1, i = bull_highest;
	     (bullet[i].b_pref > pref) && (bullet[i].b_next >= 0);
	     prev = i, i = bullet[i].b_next)
	{
	    /* skip this one */
	}

	if (bullet[i].b_pref <= pref) {
	    /* insert before i */
	    bullet[this].b_next = i;
	    if (prev >= 0) {
		bullet[prev].b_next = this;
	    } else {
		/* inserted in front */
		assert(i == bull_highest);
		bull_highest = this;
	    }
	} else {
	    /* add new last one after i */
	    bullet[this].b_next = -1;
	    bullet[i].b_next = this;
	}
    }

    bullet[this].b_pref = pref;
    bullet[this].b_valid = 1;
}

void
fsvr_replace_svr(index, cap)
int index;
capability *cap;
{
    /* replace an existing file server */
    assert(valid_svr(index));
    
    /* temporarily bring server down; check_svrs will poll the new one
     * and update its status accordingly
     */
    server_status(index, 0);
    bullet[index].b_cap = *cap;

    (void) fsvr_check_svrs();
    fsvr_print_svrs();
}

int
fsvr_add_svr(cap, pref)
capability *cap;
int pref;
{
    /* Add a file server, giving it the next-higher sequence number.
     * The sequence number is returned, thus giving a `handle' for
     * additional operations/request concerning this file server.
     */
    int this;

    if (nbullets >= NBULLETS) {
	panic("fsvr_add_svr: too many file servers");
    }

    this = nbullets;

    /* add it to the priority list */
    insert(this, pref);

    bullet[this].b_cap = *cap;
    bullet[this].b_up = 0;
    /* whether it is really up will be determined in a moment */

    nbullets++;

    (void) fsvr_check_svrs();
    fsvr_print_svrs();

    return this;
}

