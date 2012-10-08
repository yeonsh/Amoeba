/*	@(#)dlcommon.c	1.1	96/02/27 11:49:41 */
/*
 * Common (shared) DLPI test routines.
 * Mostly pretty boring boilerplate sorta stuff.
 * These can be split into individual library routines later
 * but it's just convenient to keep them in a single file
 * while they're being developed.
 *
 * Not supported:
 *   Connection Oriented stuff
 *   QOS stuff
 */

/*
typedef	unsigned long	ulong;
*/


#include	<sys/types.h>
#include	<sys/stream.h>
#include	<sys/stropts.h>
#include	<sys/dlpi.h>
#include	<sys/signal.h>
#include	<stdio.h>
#include	<string.h>
#include	"dltest.h"


char	*dlprim();
char	*dlstate();
char	*dlerrno();
char	*dlpromisclevel();
char	*dlservicemode();
char	*dlstyle();
char	*dlmactype();


dlinforeq(fd)
int	fd;
{
	dl_info_req_t	info_req;
	struct	strbuf	ctl;
	int	flags;

	info_req.dl_primitive = DL_INFO_REQ;

	ctl.maxlen = 0;
	ctl.len = sizeof (info_req);
	ctl.buf = (char *) &info_req;

	flags = RS_HIPRI;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlinforeq:  putmsg");
}

dlinfoack(fd, bufp)
int	fd;
char	*bufp;
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlinfoack");

	dlp = (union DL_primitives *) ctl.buf;

	expecting(DL_INFO_ACK, dlp);

	if (ctl.len < sizeof (dl_info_ack_t))
		err("dlinfoack:  response ctl.len too short:  %d", ctl.len);

	if (flags != RS_HIPRI)
		err("dlinfoack:  DL_INFO_ACK was not M_PCPROTO");

	if (ctl.len < sizeof (dl_info_ack_t))
		err("dlinfoack:  short response ctl.len:  %d", ctl.len);
}

dlattachreq(fd, ppa)
int	fd;
u_long	ppa;
{
	dl_attach_req_t	attach_req;
	struct	strbuf	ctl;
	int	flags;

	attach_req.dl_primitive = DL_ATTACH_REQ;
	attach_req.dl_ppa = ppa;

	ctl.maxlen = 0;
	ctl.len = sizeof (attach_req);
	ctl.buf = (char *) &attach_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlattachreq:  putmsg");
}

dlenabmultireq(fd, addr, length)
int	fd;
char	*addr;
int	length;
{
	long	buf[MAXDLBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives*) buf;

	dlp->enabmulti_req.dl_primitive = DL_ENABMULTI_REQ;
	dlp->enabmulti_req.dl_addr_length = length;
	dlp->enabmulti_req.dl_addr_offset = sizeof (dl_enabmulti_req_t);

	(void) memcpy((char*)OFFADDR(buf, sizeof (dl_enabmulti_req_t)), addr, length);

	ctl.maxlen = 0;
	ctl.len = sizeof (dl_enabmulti_req_t) + length;
	ctl.buf = (char*) buf;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlenabmultireq:  putmsg");
}

dldisabmultireq(fd, addr, length)
int	fd;
char	*addr;
int	length;
{
	long	buf[MAXDLBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives*) buf;

	dlp->disabmulti_req.dl_primitive = DL_ENABMULTI_REQ;
	dlp->disabmulti_req.dl_addr_length = length;
	dlp->disabmulti_req.dl_addr_offset = sizeof (dl_disabmulti_req_t);

	(void) memcpy((char*)OFFADDR(buf, sizeof (dl_disabmulti_req_t)), addr, length);

	ctl.maxlen = 0;
	ctl.len = sizeof (dl_disabmulti_req_t) + length;
	ctl.buf = (char*) buf;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dldisabmultireq:  putmsg");
}

dlpromisconreq(fd, level)
int	fd;
u_long	level;
{
	dl_promiscon_req_t	promiscon_req;
	struct	strbuf	ctl;
	int	flags;

	promiscon_req.dl_primitive = DL_PROMISCON_REQ;
	promiscon_req.dl_level = level;

	ctl.maxlen = 0;
	ctl.len = sizeof (promiscon_req);
	ctl.buf = (char *) &promiscon_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlpromiscon:  putmsg");

}

dlpromiscoff(fd, level)
int	fd;
u_long	level;
{
	dl_promiscoff_req_t	promiscoff_req;
	struct	strbuf	ctl;
	int	flags;

	promiscoff_req.dl_primitive = DL_PROMISCOFF_REQ;
	promiscoff_req.dl_level = level;

	ctl.maxlen = 0;
	ctl.len = sizeof (promiscoff_req);
	ctl.buf = (char *) &promiscoff_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlpromiscoff:  putmsg");
}

dlphysaddrreq(fd, addrtype)
int	fd;
u_long	addrtype;
{
	dl_phys_addr_req_t	phys_addr_req;
	struct	strbuf	ctl;
	int	flags;

	phys_addr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	phys_addr_req.dl_addr_type = addrtype;

	ctl.maxlen = 0;
	ctl.len = sizeof (phys_addr_req);
	ctl.buf = (char *) &phys_addr_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlphysaddrreq:  putmsg");
}

dlsetphysaddrreq(fd, addr, length)
int	fd;
char	*addr;
int	length;
{
	long	buf[MAXDLBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives*) buf;

	dlp->set_physaddr_req.dl_primitive = DL_ENABMULTI_REQ;
	dlp->set_physaddr_req.dl_addr_length = length;
	dlp->set_physaddr_req.dl_addr_offset = sizeof (dl_set_phys_addr_req_t);

	(void) memcpy((char*)OFFADDR(buf, sizeof (dl_set_phys_addr_req_t)), addr, length);

	ctl.maxlen = 0;
	ctl.len = sizeof (dl_set_phys_addr_req_t) + length;
	ctl.buf = (char*) buf;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlsetphysaddrreq:  putmsg");
}

dldetachreq(fd)
int	fd;
{
	dl_detach_req_t	detach_req;
	struct	strbuf	ctl;
	int	flags;

	detach_req.dl_primitive = DL_DETACH_REQ;

	ctl.maxlen = 0;
	ctl.len = sizeof (detach_req);
	ctl.buf = (char *) &detach_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dldetachreq:  putmsg");
}

dlbindreq(fd, sap, max_conind, service_mode, conn_mgmt, xidtest)
int	fd;
u_long	sap;
u_long	max_conind;
u_long	service_mode;
u_long	conn_mgmt;
u_long	xidtest;
{
	dl_bind_req_t	bind_req;
	struct	strbuf	ctl;
	int	flags;

	bind_req.dl_primitive = DL_BIND_REQ;
	bind_req.dl_sap = sap;
	bind_req.dl_max_conind = max_conind;
	bind_req.dl_service_mode = service_mode;
	bind_req.dl_conn_mgmt = conn_mgmt;
	bind_req.dl_xidtest_flg = xidtest;

	ctl.maxlen = 0;
	ctl.len = sizeof (bind_req);
	ctl.buf = (char *) &bind_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlbindreq:  putmsg");
}

dlunitdatareq(fd, addrp, addrlen, minpri, maxpri, datap, datalen)
int	fd;
u_char	*addrp;
int	addrlen;
u_long	minpri, maxpri;
u_char	*datap;
int	datalen;
{
	long	buf[MAXDLBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	data, ctl;

	dlp = (union DL_primitives*) buf;

	dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp->unitdata_req.dl_dest_addr_length = addrlen;
	dlp->unitdata_req.dl_dest_addr_offset = sizeof (dl_unitdata_req_t);
	dlp->unitdata_req.dl_priority.dl_min = minpri;
	dlp->unitdata_req.dl_priority.dl_max = maxpri;

	(void) memcpy(OFFADDR(dlp, sizeof (dl_unitdata_req_t)), addrp, addrlen);

	ctl.maxlen = 0;
	ctl.len = sizeof (dl_unitdata_req_t) + addrlen;
	ctl.buf = (char *) buf;

	data.maxlen = 0;
	data.len = datalen;
	data.buf = (char *) datap;

	if (putmsg(fd, &ctl, &data, 0) < 0)
		syserr("dlunitdatareq:  putmsg");
}

dlunbindreq(fd)
int	fd;
{
	dl_unbind_req_t	unbind_req;
	struct	strbuf	ctl;
	int	flags;

	unbind_req.dl_primitive = DL_UNBIND_REQ;

	ctl.maxlen = 0;
	ctl.len = sizeof (unbind_req);
	ctl.buf = (char *) &unbind_req;

	flags = 0;

	if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
		syserr("dlunbindreq:  putmsg");
}

dlokack(fd, bufp)
int	fd;
char	*bufp;
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlokack");

	dlp = (union DL_primitives *) ctl.buf;

	expecting(DL_OK_ACK, dlp);

	if (ctl.len < sizeof (dl_ok_ack_t))
		err("dlokack:  response ctl.len too short:  %d", ctl.len);

	if (flags != RS_HIPRI)
		err("dlokack:  DL_OK_ACK was not M_PCPROTO");

	if (ctl.len < sizeof (dl_ok_ack_t))
		err("dlokack:  short response ctl.len:  %d", ctl.len);
}

dlerrorack(fd, bufp)
int	fd;
char	*bufp;
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlerrorack");

	dlp = (union DL_primitives *) ctl.buf;

	expecting(DL_ERROR_ACK, dlp);

	if (ctl.len < sizeof (dl_error_ack_t))
		err("dlerrorack:  response ctl.len too short:  %d", ctl.len);

	if (flags != RS_HIPRI)
		err("dlerrorack:  DL_OK_ACK was not M_PCPROTO");

	if (ctl.len < sizeof (dl_error_ack_t))
		err("dlerrorack:  short response ctl.len:  %d", ctl.len);
}

dlbindack(fd, bufp)
int	fd;
char	*bufp;
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlbindack");

	dlp = (union DL_primitives *) ctl.buf;

	expecting(DL_BIND_ACK, dlp);

	if (flags != RS_HIPRI)
		err("dlbindack:  DL_OK_ACK was not M_PCPROTO");

	if (ctl.len < sizeof (dl_bind_ack_t))
		err("dlbindack:  short response ctl.len:  %d", ctl.len);
}

dlphysaddrack(fd, bufp)
int	fd;
char	*bufp;
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlphysaddrack");

	dlp = (union DL_primitives *) ctl.buf;

	expecting(DL_PHYS_ADDR_ACK, dlp);

	if (flags != RS_HIPRI)
		err("dlbindack:  DL_OK_ACK was not M_PCPROTO");

	if (ctl.len < sizeof (dl_phys_addr_ack_t))
		err("dlphysaddrack:  short response ctl.len:  %d", ctl.len);
}

static void
sigalrm()
{
	(void) err("sigalrm:  TIMEOUT");
}

strgetmsg(fd, ctlp, datap, flagsp, caller)
int	fd;
struct	strbuf	*ctlp, *datap;
int	*flagsp;
char	*caller;
{
	int	rc;
	static	char	errmsg[80];

	/*
	 * Start timer.
	 */
	(void) signal(SIGALRM, sigalrm);
	if (alarm(MAXWAIT) < 0) {
		(void) sprintf(errmsg, "%s:  alarm", caller);
		syserr(errmsg);
	}

	/*
	 * Set flags argument and issue getmsg().
	 */
	*flagsp = 0;
	if ((rc = getmsg(fd, ctlp, datap, flagsp)) < 0) {
		(void) sprintf(errmsg, "%s:  getmsg", caller);
		syserr(errmsg);
	}

	/*
	 * Stop timer.
	 */
	if (alarm(0) < 0) {
		(void) sprintf(errmsg, "%s:  alarm", caller);
		syserr(errmsg);
	}

	/*
	 * Check for MOREDATA and/or MORECTL.
	 */
	if ((rc & (MORECTL | MOREDATA)) == (MORECTL | MOREDATA))
		err("%s:  MORECTL|MOREDATA", caller);
	if (rc & MORECTL)
		err("%s:  MORECTL", caller);
	if (rc & MOREDATA)
		err("%s:  MOREDATA", caller);

	/*
	 * Check for at least sizeof (long) control data portion.
	 */
	if (ctlp->len < sizeof (long))
		err("getmsg:  control portion length < sizeof (long):  %d", ctlp->len);
}

expecting(prim, dlp)
int	prim;
union	DL_primitives	*dlp;
{
	if (dlp->dl_primitive != (u_long)prim) {
		printdlprim(dlp);
		err("expected %s got %s", dlprim(prim),
			dlprim(dlp->dl_primitive));
		exit(1);
	}
}

/*
 * Print any DLPI msg in human readable format.
 */
printdlprim(dlp)
union	DL_primitives	*dlp;
{
	switch (dlp->dl_primitive) {
		case DL_INFO_REQ:
			printdlinforeq(dlp);
			break;

		case DL_INFO_ACK:
			printdlinfoack(dlp);
			break;

		case DL_ATTACH_REQ:
			printdlattachreq(dlp);
			break;

		case DL_OK_ACK:
			printdlokack(dlp);
			break;

		case DL_ERROR_ACK:
			printdlerrorack(dlp);
			break;

		case DL_DETACH_REQ:
			printdldetachreq(dlp);
			break;

		case DL_BIND_REQ:
			printdlbindreq(dlp);
			break;

		case DL_BIND_ACK:
			printdlbindack(dlp);
			break;

		case DL_UNBIND_REQ:
			printdlunbindreq(dlp);
			break;

		case DL_SUBS_BIND_REQ:
			printdlsubsbindreq(dlp);
			break;

		case DL_SUBS_BIND_ACK:
			printdlsubsbindack(dlp);
			break;

		case DL_SUBS_UNBIND_REQ:
			printdlsubsunbindreq(dlp);
			break;

		case DL_ENABMULTI_REQ:
			printdlenabmultireq(dlp);
			break;

		case DL_DISABMULTI_REQ:
			printdldisabmultireq(dlp);
			break;

		case DL_PROMISCON_REQ:
			printdlpromisconreq(dlp);
			break;

		case DL_PROMISCOFF_REQ:
			printdlpromiscoffreq(dlp);
			break;

		case DL_UNITDATA_REQ:
			printdlunitdatareq(dlp);
			break;

		case DL_UNITDATA_IND:
			printdlunitdataind(dlp);
			break;

		case DL_UDERROR_IND:
			printdluderrorind(dlp);
			break;

		case DL_UDQOS_REQ:
			printdludqosreq(dlp);
			break;

		case DL_PHYS_ADDR_REQ:
			printdlphysaddrreq(dlp);
			break;

		case DL_PHYS_ADDR_ACK:
			printdlphysaddrack(dlp);
			break;

		case DL_SET_PHYS_ADDR_REQ:
			printdlsetphysaddrreq(dlp);
			break;

		default:
			err("printdlprim:  unknown primitive type 0x%x",
				dlp->dl_primitive);
			break;
	}
}

/* ARGSUSED */
printdlinforeq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_INFO_REQ\n");
}

printdlinfoack(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];
	u_char	brdcst[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->info_ack.dl_addr_offset),
		dlp->info_ack.dl_addr_length, addr);
	addrtostring(OFFADDR(dlp, dlp->info_ack.dl_brdcst_addr_offset),
		dlp->info_ack.dl_brdcst_addr_length, brdcst);

	(void) printf("DL_INFO_ACK:  max_sdu %d min_sdu %d\n",
		dlp->info_ack.dl_max_sdu,
		dlp->info_ack.dl_min_sdu);
	(void) printf("addr_length %d mac_type %s current_state %s\n",
		dlp->info_ack.dl_addr_length,
		dlmactype(dlp->info_ack.dl_mac_type),
		dlstate(dlp->info_ack.dl_current_state));
	(void) printf("sap_length %d service_mode %s qos_length %d\n",
		dlp->info_ack.dl_sap_length,
		dlservicemode(dlp->info_ack.dl_service_mode),
		dlp->info_ack.dl_qos_length);
	(void) printf("qos_offset %d qos_range_length %d qos_range_offset %d\n",
		dlp->info_ack.dl_qos_offset,
		dlp->info_ack.dl_qos_range_length,
		dlp->info_ack.dl_qos_range_offset);
	(void) printf("provider_style %s addr_offset %d version %d\n",
		dlstyle(dlp->info_ack.dl_provider_style),
		dlp->info_ack.dl_addr_offset,
		dlp->info_ack.dl_version);
	(void) printf("brdcst_addr_length %d brdcst_addr_offset %d\n",
		dlp->info_ack.dl_brdcst_addr_length,
		dlp->info_ack.dl_brdcst_addr_offset);
	(void) printf("addr %s\n", addr);
	(void) printf("brdcst_addr %s\n", brdcst);
}

printdlattachreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_ATTACH_REQ:  ppa %d\n",
		dlp->attach_req.dl_ppa);
}

printdlokack(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_OK_ACK:  correct_primitive %s\n",
		dlprim(dlp->ok_ack.dl_correct_primitive));
}

printdlerrorack(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_ERROR_ACK:  error_primitive %s errno %s unix_errno %d\n",
		dlprim(dlp->error_ack.dl_error_primitive),
		dlerrno(dlp->error_ack.dl_errno),
		dlp->error_ack.dl_unix_errno);
}

printdlenabmultireq(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->enabmulti_req.dl_addr_offset),
		dlp->enabmulti_req.dl_addr_length, addr);

	(void) printf("DL_ENABMULTI_REQ:  addr_length %d addr_offset %d\n",
		dlp->enabmulti_req.dl_addr_length,
		dlp->enabmulti_req.dl_addr_offset);
	(void) printf("addr %s\n", addr);
}

printdldisabmultireq(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->disabmulti_req.dl_addr_offset),
		dlp->disabmulti_req.dl_addr_length, addr);

	(void) printf("DL_DISABMULTI_REQ:  addr_length %d addr_offset %d\n",
		dlp->disabmulti_req.dl_addr_length,
		dlp->disabmulti_req.dl_addr_offset);
	(void) printf("addr %s\n", addr);
}

printdlpromisconreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_PROMISCON_REQ:  level %s\n",
		dlpromisclevel(dlp->promiscon_req.dl_level));
}

printdlpromiscoffreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_PROMISCOFF_REQ:  level %s\n",
		dlpromisclevel(dlp->promiscoff_req.dl_level));
}

printdlphysaddrreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_PHYS_ADDR_REQ:  addr_type 0x%x\n",
		dlp->physaddr_req.dl_addr_type);
}

printdlphysaddrack(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->physaddr_ack.dl_addr_offset),
		dlp->physaddr_ack.dl_addr_length, addr);

	(void) printf("DL_PHYS_ADDR_ACK:  addr_length %d addr_offset %d\n",
		dlp->physaddr_ack.dl_addr_length,
		dlp->physaddr_ack.dl_addr_offset);
	(void) printf("addr %s\n", addr);
}

printdlsetphysaddrreq(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->set_physaddr_req.dl_addr_offset),
		dlp->set_physaddr_req.dl_addr_length, addr);

	(void) printf("DL_SET_PHYS_ADDR_REQ:  addr_length %d addr_offset %d\n",
		dlp->set_physaddr_req.dl_addr_length,
		dlp->set_physaddr_req.dl_addr_offset);
	(void) printf("addr %s\n", addr);
}

/* ARGSUSED */
printdldetachreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_DETACH_REQ\n");
}

printdlbindreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_BIND_REQ:  sap %d max_conind %d\n",
		dlp->bind_req.dl_sap,
		dlp->bind_req.dl_max_conind);
	(void) printf("service_mode %s conn_mgmt %d xidtest_flg 0x%x\n",
		dlservicemode(dlp->bind_req.dl_service_mode),
		dlp->bind_req.dl_conn_mgmt,
		dlp->bind_req.dl_xidtest_flg);
}

printdlbindack(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->bind_ack.dl_addr_offset),
		dlp->bind_ack.dl_addr_length, addr);

	(void) printf("DL_BIND_ACK:  sap %d addr_length %d addr_offset %d\n",
		dlp->bind_ack.dl_sap,
		dlp->bind_ack.dl_addr_length,
		dlp->bind_ack.dl_addr_offset);
	(void) printf("max_conind %d xidtest_flg 0x%x\n",
		dlp->bind_ack.dl_max_conind,
		dlp->bind_ack.dl_xidtest_flg);
	(void) printf("addr %s\n", addr);
}

/* ARGSUSED */
printdlunbindreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_UNBIND_REQ\n");
}

printdlsubsbindreq(dlp)
union	DL_primitives	*dlp;
{
	u_char	sap[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->subs_bind_req.dl_subs_sap_offset),
		dlp->subs_bind_req.dl_subs_sap_length, sap);

	(void) printf("DL_SUBS_BIND_REQ:  subs_sap_offset %d sub_sap_len %d\n",
		dlp->subs_bind_req.dl_subs_sap_offset,
		dlp->subs_bind_req.dl_subs_sap_length);
	(void) printf("sap %s\n", sap);
}

printdlsubsbindack(dlp)
union	DL_primitives	*dlp;
{
	u_char	sap[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->subs_bind_ack.dl_subs_sap_offset),
		dlp->subs_bind_ack.dl_subs_sap_length, sap);

	(void) printf("DL_SUBS_BIND_ACK:  subs_sap_offset %d sub_sap_length %d\n",
		dlp->subs_bind_ack.dl_subs_sap_offset,
		dlp->subs_bind_ack.dl_subs_sap_length);
	(void) printf("sap %s\n", sap);
}

printdlsubsunbindreq(dlp)
union	DL_primitives	*dlp;
{
	u_char	sap[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->subs_unbind_req.dl_subs_sap_offset),
		dlp->subs_unbind_req.dl_subs_sap_length, sap);

	(void) printf("DL_SUBS_UNBIND_REQ:  subs_sap_offset %d sub_sap_length %d\n",
		dlp->subs_unbind_req.dl_subs_sap_offset,
		dlp->subs_unbind_req.dl_subs_sap_length);
	(void) printf("sap %s\n", sap);
}

printdlunitdatareq(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->unitdata_req.dl_dest_addr_offset),
		dlp->unitdata_req.dl_dest_addr_length, addr);

	(void) printf("DL_UNITDATA_REQ:  dest_addr_length %d dest_addr_offset %d\n",
		dlp->unitdata_req.dl_dest_addr_length,
		dlp->unitdata_req.dl_dest_addr_offset);
	(void) printf("dl_priority.min %d dl_priority.max %d\n",
		dlp->unitdata_req.dl_priority.dl_min,
		dlp->unitdata_req.dl_priority.dl_max);
	(void) printf("addr %s\n", addr);
}

printdlunitdataind(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];
	u_char	src[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->unitdata_ind.dl_dest_addr_offset),
		dlp->unitdata_ind.dl_dest_addr_length, dest);
	addrtostring(OFFADDR(dlp, dlp->unitdata_ind.dl_src_addr_offset),
		dlp->unitdata_ind.dl_src_addr_length, src);

	(void) printf("DL_UNITDATA_IND:  dest_addr_length %d dest_addr_offset %d\n",
		dlp->unitdata_ind.dl_dest_addr_length,
		dlp->unitdata_ind.dl_dest_addr_offset);
	(void) printf("src_addr_length %d src_addr_offset %d\n",
		dlp->unitdata_ind.dl_src_addr_length,
		dlp->unitdata_ind.dl_src_addr_offset);
	(void) printf("group_address 0x%x\n",
		dlp->unitdata_ind.dl_group_address);
	(void) printf("dest %s\n", dest);
	(void) printf("src %s\n", src);
}

printdluderrorind(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->uderror_ind.dl_dest_addr_offset),
		dlp->uderror_ind.dl_dest_addr_length, addr);

	(void) printf("DL_UDERROR_IND:  dest_addr_length %d dest_addr_offset %d\n",
		dlp->uderror_ind.dl_dest_addr_length,
		dlp->uderror_ind.dl_dest_addr_offset);
	(void) printf("unix_errno %d errno %s\n",
		dlp->uderror_ind.dl_unix_errno,
		dlerrno(dlp->uderror_ind.dl_errno));
	(void) printf("addr %s\n", addr);
}

printdltestreq(dlp)
union	DL_primitives	*dlp;
{
	u_char	addr[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->test_req.dl_dest_addr_offset),
		dlp->test_req.dl_dest_addr_length, addr);

	(void) printf("DL_TEST_REQ:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->test_req.dl_flag,
		dlp->test_req.dl_dest_addr_length,
		dlp->test_req.dl_dest_addr_offset);
	(void) printf("dest_addr %s\n", addr);
}

printdltestind(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];
	u_char	src[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->test_ind.dl_dest_addr_offset),
		dlp->test_ind.dl_dest_addr_length, dest);
	addrtostring(OFFADDR(dlp, dlp->test_ind.dl_src_addr_offset),
		dlp->test_ind.dl_src_addr_length, src);

	(void) printf("DL_TEST_IND:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->test_ind.dl_flag,
		dlp->test_ind.dl_dest_addr_length,
		dlp->test_ind.dl_dest_addr_offset);
	(void) printf("src_addr_length %d src_addr_offset %d\n",
		dlp->test_ind.dl_src_addr_length,
		dlp->test_ind.dl_src_addr_offset);
	(void) printf("dest_addr %s\n", dest);
	(void) printf("src_addr %s\n", src);
}

printdltestres(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->test_res.dl_dest_addr_offset),
		dlp->test_res.dl_dest_addr_length, dest);

	(void) printf("DL_TEST_RES:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->test_res.dl_flag,
		dlp->test_res.dl_dest_addr_length,
		dlp->test_res.dl_dest_addr_offset);
	(void) printf("dest_addr %s\n", dest);
}

printdltestcon(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];
	u_char	src[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->test_con.dl_dest_addr_offset),
		dlp->test_con.dl_dest_addr_length, dest);
	addrtostring(OFFADDR(dlp, dlp->test_con.dl_src_addr_offset),
		dlp->test_con.dl_src_addr_length, src);

	(void) printf("DL_TEST_CON:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->test_con.dl_flag,
		dlp->test_con.dl_dest_addr_length,
		dlp->test_con.dl_dest_addr_offset);
	(void) printf("src_addr_length %d src_addr_offset %d\n",
		dlp->test_con.dl_src_addr_length,
		dlp->test_con.dl_src_addr_offset);
	(void) printf("dest_addr %s\n", dest);
	(void) printf("src_addr %s\n", src);
}

printdlxidreq(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->xid_req.dl_dest_addr_offset),
		dlp->xid_req.dl_dest_addr_length, dest);

	(void) printf("DL_XID_REQ:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->xid_req.dl_flag,
		dlp->xid_req.dl_dest_addr_length,
		dlp->xid_req.dl_dest_addr_offset);
	(void) printf("dest_addr %s\n", dest);
}

printdlxidind(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];
	u_char	src[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->xid_ind.dl_dest_addr_offset),
		dlp->xid_ind.dl_dest_addr_length, dest);
	addrtostring(OFFADDR(dlp, dlp->xid_ind.dl_src_addr_offset),
		dlp->xid_ind.dl_src_addr_length, src);

	(void) printf("DL_XID_IND:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->xid_ind.dl_flag,
		dlp->xid_ind.dl_dest_addr_length,
		dlp->xid_ind.dl_dest_addr_offset);
	(void) printf("src_addr_length %d src_addr_offset %d\n",
		dlp->xid_ind.dl_src_addr_length,
		dlp->xid_ind.dl_src_addr_offset);
	(void) printf("dest_addr %s\n", dest);
	(void) printf("src_addr %s\n", src);
}

printdlxidres(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->xid_res.dl_dest_addr_offset),
		dlp->xid_res.dl_dest_addr_length, dest);

	(void) printf("DL_XID_RES:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->xid_res.dl_flag,
		dlp->xid_res.dl_dest_addr_length,
		dlp->xid_res.dl_dest_addr_offset);
	(void) printf("dest_addr %s\n", dest);
}

printdlxidcon(dlp)
union	DL_primitives	*dlp;
{
	u_char	dest[MAXDLADDR];
	u_char	src[MAXDLADDR];

	addrtostring(OFFADDR(dlp, dlp->xid_con.dl_dest_addr_offset),
		dlp->xid_con.dl_dest_addr_length, dest);
	addrtostring(OFFADDR(dlp, dlp->xid_con.dl_src_addr_offset),
		dlp->xid_con.dl_src_addr_length, src);

	(void) printf("DL_XID_CON:  flag 0x%x dest_addr_length %d dest_addr_offset %d\n",
		dlp->xid_con.dl_flag,
		dlp->xid_con.dl_dest_addr_length,
		dlp->xid_con.dl_dest_addr_offset);
	(void) printf("src_addr_length %d src_addr_offset %d\n",
		dlp->xid_con.dl_src_addr_length,
		dlp->xid_con.dl_src_addr_offset);
	(void) printf("dest_addr %s\n", dest);
	(void) printf("src_addr %s\n", src);
}

printdludqosreq(dlp)
union	DL_primitives	*dlp;
{
	(void) printf("DL_UDQOS_REQ:  qos_length %d qos_offset %d\n",
		dlp->udqos_req.dl_qos_length,
		dlp->udqos_req.dl_qos_offset);
}

/*
 * Return string.
 */
addrtostring(addr, length, s)
u_char	*addr;
u_long	length;
u_char	*s;
{
	int	i;

	for (i = 0; i < length; i++) {
		(void) sprintf((char*) s, "%x:", addr[i] & 0xff);
		s = s + strlen((char*)s);
	}
	if (length)
		*(--s) = '\0';
}

/*
 * Return length
 */
stringtoaddr(sp, addr)
char	*sp;
char	*addr;
{
	int	n = 0;
	char	*p;
	int	val;

	p = sp;
	while (p = strtok(p, ":")) {
		if (sscanf(p, "%x", &val) != 1)
			err("stringtoaddr:  invalid input string:  %s", sp);
		if (val > 0xff)
			err("stringtoaddr:  invalid input string:  %s", sp);
		*addr++ = val;
		n++;
		p = NULL;
	}
	
	return (n);
}


static char
hexnibble(c)
char	c;
{
	static	char	hextab[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f'
	};

	return (hextab[c & 0x0f]);
}

char*
dlprim(prim)
u_long	prim;
{
	static	char	primbuf[80];

	switch ((int)prim) {
		case DL_INFO_REQ: return("DL_INFO_REQ");
		case DL_INFO_ACK: return("DL_INFO_ACK");
		case DL_ATTACH_REQ: return("DL_ATTACH_REQ");
		case DL_DETACH_REQ: return("DL_DETACH_REQ");
		case DL_BIND_REQ: return("DL_BIND_REQ");
		case DL_BIND_ACK: return("DL_BIND_ACK");
		case DL_UNBIND_REQ: return("DL_UNBIND_REQ");
		case DL_OK_ACK: return("DL_OK_ACK");
		case DL_ERROR_ACK: return("DL_ERROR_ACK");
		case DL_SUBS_BIND_REQ: return("DL_SUBS_BIND_REQ");
		case DL_SUBS_BIND_ACK: return("DL_SUBS_BIND_ACK");
		case DL_UNITDATA_REQ: return("DL_UNITDATA_REQ");
		case DL_UNITDATA_IND: return("DL_UNITDATA_IND");
		case DL_UDERROR_IND: return("DL_UDERROR_IND");
		case DL_UDQOS_REQ: return("DL_UDQOS_REQ");
		case DL_CONNECT_REQ: return("DL_CONNECT_REQ");
		case DL_CONNECT_IND: return("DL_CONNECT_IND");
		case DL_CONNECT_RES: return("DL_CONNECT_RES");
		case DL_CONNECT_CON: return("DL_CONNECT_CON");
		case DL_TOKEN_REQ: return("DL_TOKEN_REQ");
		case DL_TOKEN_ACK: return("DL_TOKEN_ACK");
		case DL_DISCONNECT_REQ: return("DL_DISCONNECT_REQ");
		case DL_DISCONNECT_IND: return("DL_DISCONNECT_IND");
		case DL_RESET_REQ: return("DL_RESET_REQ");
		case DL_RESET_IND: return("DL_RESET_IND");
		case DL_RESET_RES: return("DL_RESET_RES");
		case DL_RESET_CON: return("DL_RESET_CON");
		default:
			(void) sprintf(primbuf, "unknown primitive 0x%x", prim);
			return (primbuf);
	}
}


char*
dlstate(state)
u_long	state;
{
	static	char	statebuf[80];

	switch (state) {
		case DL_UNATTACHED: return("DL_UNATTACHED");
		case DL_ATTACH_PENDING: return("DL_ATTACH_PENDING");
		case DL_DETACH_PENDING: return("DL_DETACH_PENDING");
		case DL_UNBOUND: return("DL_UNBOUND");
		case DL_BIND_PENDING: return("DL_BIND_PENDING");
		case DL_UNBIND_PENDING: return("DL_UNBIND_PENDING");
		case DL_IDLE: return("DL_IDLE");
		case DL_UDQOS_PENDING: return("DL_UDQOS_PENDING");
		case DL_OUTCON_PENDING: return("DL_OUTCON_PENDING");
		case DL_INCON_PENDING: return("DL_INCON_PENDING");
		case DL_CONN_RES_PENDING: return("DL_CONN_RES_PENDING");
		case DL_DATAXFER: return("DL_DATAXFER");
		case DL_USER_RESET_PENDING: return("DL_USER_RESET_PENDING");
		case DL_PROV_RESET_PENDING: return("DL_PROV_RESET_PENDING");
		case DL_RESET_RES_PENDING: return("DL_RESET_RES_PENDING");
		case DL_DISCON8_PENDING: return("DL_DISCON8_PENDING");
		case DL_DISCON9_PENDING: return("DL_DISCON9_PENDING");
		case DL_DISCON11_PENDING: return("DL_DISCON11_PENDING");
		case DL_DISCON12_PENDING: return("DL_DISCON12_PENDING");
		case DL_DISCON13_PENDING: return("DL_DISCON13_PENDING");
		case DL_SUBS_BIND_PND: return("DL_SUBS_BIND_PND");
		default:
			(void) sprintf(statebuf, "unknown state 0x%x", state);
			return (statebuf);
	}
}

char*
dlerrno(errno)
u_long	errno;
{
	static	char	errnobuf[80];

	switch (errno) {
		case DL_ACCESS: return("DL_ACCESS");
		case DL_BADADDR: return("DL_BADADDR");
		case DL_BADCORR: return("DL_BADCORR");
		case DL_BADDATA: return("DL_BADDATA");
		case DL_BADPPA: return("DL_BADPPA");
		case DL_BADPRIM: return("DL_BADPRIM");
		case DL_BADQOSPARAM: return("DL_BADQOSPARAM");
		case DL_BADQOSTYPE: return("DL_BADQOSTYPE");
		case DL_BADSAP: return("DL_BADSAP");
		case DL_BADTOKEN: return("DL_BADTOKEN");
		case DL_BOUND: return("DL_BOUND");
		case DL_INITFAILED: return("DL_INITFAILED");
		case DL_NOADDR: return("DL_NOADDR");
		case DL_NOTINIT: return("DL_NOTINIT");
		case DL_OUTSTATE: return("DL_OUTSTATE");
		case DL_SYSERR: return("DL_SYSERR");
		case DL_UNSUPPORTED: return("DL_UNSUPPORTED");
		case DL_UNDELIVERABLE: return("DL_UNDELIVERABLE");
		case DL_NOTSUPPORTED: return("DL_NOTSUPPORTED");
		case DL_TOOMANY: return("DL_TOOMANY");
		case DL_NOTENAB: return("DL_NOTENAB");
		case DL_BUSY: return("DL_BUSY");
		case DL_NOAUTO: return("DL_NOAUTO");
		case DL_NOXIDAUTO: return("DL_NOXIDAUTO");
		case DL_NOTESTAUTO: return("DL_NOTESTAUTO");
		case DL_XIDAUTO: return("DL_XIDAUTO");
		case DL_TESTAUTO: return("DL_TESTAUTO");
		case DL_PENDING: return("DL_PENDING");

		default:
			(void) sprintf(errnobuf, "unknown dlpi errno 0x%x", errno);
			return (errnobuf);
	}
}

char*
dlpromisclevel(level)
u_long	level;
{
	static	char	levelbuf[80];

	switch (level) {
		case DL_PROMISC_PHYS: return("DL_PROMISC_PHYS");
		case DL_PROMISC_SAP: return("DL_PROMISC_SAP");
		case DL_PROMISC_MULTI: return("DL_PROMISC_MULTI");
		default:
			(void) sprintf(levelbuf, "unknown promisc level 0x%x", level);
			return (levelbuf);
	}
}

char*
dlservicemode(servicemode)
u_long	servicemode;
{
	static	char	servicemodebuf[80];

	switch (servicemode) {
		case DL_CODLS: return("DL_CODLS");
		case DL_CLDLS: return("DL_CLDLS");
		case DL_CODLS|DL_CLDLS: return("DL_CODLS|DL_CLDLS");
		default:
			(void) sprintf(servicemodebuf,
				"unknown provider service mode 0x%x", servicemode);
			return (servicemodebuf);
	}
}

char*
dlstyle(style)
long	style;
{
	static	char	stylebuf[80];

	switch (style) {
		case DL_STYLE1: return("DL_STYLE1");
		case DL_STYLE2: return("DL_STYLE2");
		default:
			(void) sprintf(stylebuf, "unknown provider style 0x%x", style);
			return (stylebuf);
	}
}

char*
dlmactype(media)
u_long	media;
{
	static	char	mediabuf[80];

	switch (media) {
		case DL_CSMACD: return("DL_CSMACD");
		case DL_TPB: return("DL_TPB");
		case DL_TPR: return("DL_TPR");
		case DL_METRO: return("DL_METRO");
		case DL_ETHER: return("DL_ETHER");
		case DL_HDLC: return("DL_HDLC");
		case DL_CHAR: return("DL_CHAR");
		case DL_CTCA: return("DL_CTCA");
		default:
			(void) sprintf(mediabuf, "unknown media type 0x%x", media);
			return (mediabuf);
	}
}

/*VARARGS1*/
err(fmt, a1, a2, a3, a4)
char	*fmt;
char	*a1, *a2, *a3, *a4;
{
	(void) fprintf(stderr, fmt, a1, a2, a3, a4);
	(void) fprintf(stderr, "\n");
	(void) exit(1);
}

syserr(s)
char	*s;
{
	(void) perror(s);
	exit(1);
}

strioctl(fd, cmd, timout, len, dp)
int	fd;
int	cmd;
int	timout;
int	len;
char	*dp;
{
	struct	strioctl	sioc;
	int	rc;

	sioc.ic_cmd = cmd;
	sioc.ic_timout = timout;
	sioc.ic_len = len;
	sioc.ic_dp = dp;
	rc = ioctl(fd, I_STR, &sioc);

	if (rc < 0)
		return (rc);
	else
		return (sioc.ic_len);
}
