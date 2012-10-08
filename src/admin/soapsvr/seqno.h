/*	@(#)seqno.h	1.1	96/02/27 10:22:56 */
#ifndef SEQNO_H
#define SEQNO_H

#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"

void zero_seq_no   _ARGS((sp_seqno_t *s));
int  eq_seq_no	   _ARGS((sp_seqno_t *s1, sp_seqno_t *s2));
int  less_seq_no   _ARGS((sp_seqno_t *s1, sp_seqno_t *s2));
void incr_seq_no   _ARGS((sp_seqno_t *s));

void set_global_seqno  _ARGS((sp_seqno_t *s));
void get_global_seqno  _ARGS((sp_seqno_t *s));
void incr_global_seqno _ARGS((void));

#define lseq(seqno)	((long) ((seqno)->seq[1]))
#define hseq(seqno)	((long) ((seqno)->seq[0]))

void sp_init_dirseqno _ARGS((sp_seqno_t *s));

#endif
