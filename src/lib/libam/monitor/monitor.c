/*	@(#)monitor.c	1.5	96/02/27 11:02:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define MONWHIZ
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "stdlib.h"
#include "string.h"

static MN_b_p mondata;		/* pointer to databuffer */
static bufsize monsize;		/* size of databuffer */
static MN_ev_p nxtev;		/* pointer to next unused event slot */
static int lsoff;		/* index of lowest used character slot */
static short newevflags;	/* flags for new event */
static header monhdr;		/* header to send transactions out */
static short *circbase;		/* base of circular buffer */
static int circnum;		/* number of entries in circular buffer */

void
_MN_ainit(buf,siz)
MN_b_p buf;
{
	register i;

	siz &= ~1;
	if (siz<0)
		return;
 	if (siz>MAXMONSIZE)
		siz=MAXMONSIZE;
	mondata = buf;
	for(i=0;i<siz;i++)
		mondata->MN_cbuf[i] = 0;
	nxtev = mondata->MN_mbuf;
	monsize = siz;
	lsoff = 3*siz/4;		/* 3/4 for structs and strings */
	lsoff &= ~1;
	circbase = mondata->MN_sbuf+lsoff/2;
	circnum = (siz-lsoff)/2-1;
	*circbase = 1;			/* starting index */
	circbase[circnum] = circnum;	/* last short gives size */
}

static
moninit() {
	char *buf;
	
	buf = (char *) malloc(DEFMONSIZE);
	if (buf==0)
		return 1;
	MON_SETBUF((MN_b_p) buf, DEFMONSIZE);
	return 0;
}

MN_ev_p
_MN_init(str)
char *str;
{
	char *strcpy();
	register MN_ev_p mp;
	int len;
	static MN_ev_t dummy;	/* used in case of overflow */
	static overflow;
	
	if (monsize==0 && moninit())
		return &dummy;
	for(mp=mondata->MN_mbuf;mp<nxtev;mp++)
		if (strcmp(str,mondata->MN_cbuf+mp->MN_soff)==0)
			return mp;
	if (overflow)
		return &dummy;
	len = strlen(str)+1;
	if (&mondata->MN_cbuf[lsoff-len] <= (char *)(mp+2) ) {
		overflow++;
		mp->MN_cnt = -1;
		return &dummy;
	}
	nxtev++;
	lsoff -= len;
	(void)strcpy(&mondata->MN_cbuf[lsoff],str);
	mp->MN_cnt = 0L;
	mp->MN_soff = lsoff;
	mp->MN_flags = newevflags;
	return mp;
}

static void
sendtrans(cmd) {
	header rephdr;
	register interval oldtm;

	oldtm = timeout((interval) 1000);
	monhdr.h_command = cmd;
	(void) trans(&monhdr, mondata->MN_cbuf, monsize, &rephdr, NILBUF, 0);
	(void) timeout(oldtm);
}

void
_MN_occurred(mp)
MN_ev_p mp;
{

	if (mp->MN_flags&MNF_CIRBUF) {
		circbase[*circbase] = mp->MN_soff;
		*circbase += 1;
		if (*circbase >= circnum)
			*circbase = 1;
	}
	if (mp->MN_flags&MNF_ITRACE)
		sendtrans(MNT_ITRACE);
}

void
_MN_xqt(hp, retbuf, retsiz)
header *hp;
bufptr *retbuf;
bufsize *retsiz;
{
	register i;
	register MN_ev_p mp;
	unsigned long mask;

	*retbuf = NILBUF;
	*retsiz = 0;
	switch(hp->MN_command) {
	default:
		hp->h_status = MNS_UNKNOWN;
		break;
	case MNC_ENQUIRY:
		hp->h_status = MNS_OK;
		*retbuf = mondata->MN_cbuf;
		*retsiz = monsize;
		break;
	case MNC_SETAFLAGS:
		for(mp=mondata->MN_mbuf;mp<nxtev;mp++)
			mp->MN_flags = hp->MN_arg;
		/* fall through */
	case MNC_SETNFLAGS:
		monhdr.h_port = hp->MN_clport;	/* for our own actions */
		newevflags = hp->MN_arg;
		hp->h_status = MNS_OK;
		break;
	case MNC_ITRACE:
		monhdr.h_port = hp->MN_clport;	/* for our own actions */
		for(i=0, mask=1, mp=mondata->MN_mbuf+hp->MN_arg;
		    i<32 && mp<nxtev;
		    i++, mask += mask, mp++)
			if (hp->h_offset&mask)
				mp->MN_flags |=   MNF_ITRACE;
			else
				mp->MN_flags &= ~ MNF_ITRACE;
		hp->h_status = MNS_OK;
		break;
	case MNC_RESET:
		for(mp=mondata->MN_mbuf;mp<nxtev;mp++)
			mp->MN_cnt = 0L;
		hp->h_status = MNS_OK;
		break;
	}
}
