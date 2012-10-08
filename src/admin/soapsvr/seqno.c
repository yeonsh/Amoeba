/*	@(#)seqno.c	1.1	96/02/27 10:22:45 */
#include "amoeba.h"
#include "seqno.h"

#ifdef SOAP_DIR_SEQNR

static sp_seqno_t Global_seqno;

void
incr_seq_no(s)
sp_seqno_t *s;
{
    if (++s->seq[1] == 0) { /* we had overflow */
	++s->seq[0];
    }
}

int
eq_seq_no(s1, s2)
sp_seqno_t *s1;
sp_seqno_t *s2;
{
    return (s1->seq[0] == s2->seq[0]) && (s1->seq[1] == s2->seq[1]);
}

int
less_seq_no(s1, s2)
sp_seqno_t *s1;
sp_seqno_t *s2;
{
    if (s1->seq[0] < s2->seq[0]) {
	return(1);
    } else if (s1->seq[0] > s2->seq[0]) {
	return(0);
    } else {
	return (s1->seq[1] < s2->seq[1]);
    }
}

void
zero_seq_no(s)
sp_seqno_t *s;
{
    s->seq[0] = s->seq[1] = 0;
}

void
set_global_seqno(s)
sp_seqno_t *s;
{
    Global_seqno = *s;
}

void
get_global_seqno(s)
sp_seqno_t *s;
{
    *s = Global_seqno;
}

void
incr_global_seqno()
{
    incr_seq_no(&Global_seqno);
}

void
sp_init_dirseqno(seq)
sp_seqno_t *seq;
{
    /* Called by sp_impl.c when a new directory is being created,
     * to initialise the starting sequence number for the directory.
     *
     * This is needed to correctly handle the situation where an existing
     * directory is destroyed and recreated during the absence of one
     * or more members.
     */
    get_global_seqno(seq);
}

#endif /* SOAP_DIR_SEQNR */
