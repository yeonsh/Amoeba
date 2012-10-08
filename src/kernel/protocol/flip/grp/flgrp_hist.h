/*	@(#)flgrp_hist.h	1.5	94/04/06 08:37:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_HIST_H__
#define __FLGRP_HIST_H__

#define HST_MOD(g, s) 	(s & g->g_hstmask)
#define HST_FULL(g) 	(g->g_nexthistory - g->g_minhistory >= g->g_nhist)
#define HST_EMPTY(g) 	(g->g_minhistory == g->g_nextseqno)
#define HST_SYNCHRONIZE(g) (g->g_nextseqno - g->g_minhistory >= g->g_nhstfull-1)
#define HST_CHECK(g)    (g->g_nextseqno - g->g_minhistory <= g->g_nhstfull && \
			 g->g_nextseqno <= g->g_nexthistory)


#define HST_IN(g, s) ((g)->g_minhistory <= s && s < (g)->g_nextseqno)


typedef struct {
        bchdr_t 	h_bc;		/* broadcast header */
        char    	*h_data; 	/* user data including amoeba header */
        f_size_t	h_size; 	/* sizeof user data */
	int		h_bigendian;	/* endianess of data */
	int		h_nack;		/* number of acks received */
	uint32		h_mem_mask; 	/* keep track of who send an ack */
	int		h_accept;	/* can the msg be delivered? */
} hist_t, *hist_p;

#endif /* __FLGRP_HIST_H__ */
