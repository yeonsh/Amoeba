/*	@(#)measure.h	1.3	96/02/27 10:40:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef MEASURE

#define cat(a, b)	a##b

struct measure_info {
    long me_begin;
    long me_total;
    long me_cnt;
    long me_min;
    long me_max;
};

#define Fields(name) \
	struct measure_info cat(me_inf_,name)

typedef struct measure_kernel {
    long	nruns;
    long	factor;
    long	cost_gettime;		/* per nruns */
    long	(*gettime)();

    Fields(measure);			/* don't move this one */
    Fields(ether_snd);
    Fields(ether_rcv);
    Fields(flip_rcv);
    Fields(flip_snd);
    Fields(rpc_trans);
    Fields(rpc_getreq);
    Fields(rpc_doop);
    Fields(rpc_putrep);
    Fields(rpc_ack);
    Fields(rpc_rcv_putrep);
    Fields(rpc_snd_putrep);
    Fields(rpc_start_getreq);
    Fields(rpc_end_getreq);
    Fields(rpc_snd_trans);
    Fields(rpc_rcv_trans);
    Fields(rpc_getreq_reassem);
    Fields(rpc_trans_reassem);
    Fields(rpc_rcv);
    Fields(rpc_client);
    Fields(grp_snd_snd);
    Fields(grp_snd_rcv);
    Fields(grp_snd);
    Fields(grp_sequencer);
    Fields(grp_begin_rcv);
    Fields(grp_end_rcv);
    Fields(grp_member);
    Fields(raw_receive);
    Fields(raw_upcall);
    Fields(raw_send);
    Fields(raw_doop);
    Fields(mpx_start_scheduler);
    Fields(mpx_end_scheduler);
    Fields(map_ptr);
    Fields(mu_trylock);
    Fields(mu_unlock);
    Fields(mu_gotlock);
    Fields(sig_gotintr);
    Fields(sig_put);
    Fields(sig_call);
    Fields(sig_swtrap);
} measure_t, *measure_p;

extern measure_t measure_kernel;


#define measure_inf(name)	cat(measure_kernel.me_inf_,name)

#define BEGIN_MEASURE(name) \
    (measure_inf(name).me_begin = (*measure_kernel.gettime)())

extern void measure_end _ARGS((char *_name, struct measure_info *_inf));

#define END_MEASURE(name) \
    (measure_end(#name, &measure_inf(name)))

#else

#define BEGIN_MEASURE(name)

#define END_MEASURE(name)

#endif
