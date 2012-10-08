/*	@(#)vc.h	1.5	96/02/27 10:26:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __VC_H__
#define __VC_H__

/*
** virtual circuit module.
**
*/
#define VIRTCIRC	"virtual circuit"


#define CLIENT		0		/* Client parameters */
#define SERVER		1		/* Server parameters */

struct vc {
    mutex	    vc_mutex;		/* VC control block mutex */
    mutex	    vc_client_hup;	/* Client HUP mutex */
    mutex	    vc_client_dead;	/* Client won't access vc no more */
    mutex	    vc_iclosed;		/* Input closed */
    mutex	    vc_oclosed;		/* Output closed */
    mutex	    vc_done;		/* We're through */
    uint32 	    vc_status;		/* Vitual circuit status */
    long	    vc_sigs[2];		/* Client to srv/srv to client sig */
    /* XXX should be signum */
    struct circbuf *vc_cb[2];		/* In and output circular buffers */
    void  	  (*vc_warnf[2]) _ARGS(( int warnarg ));
					/* Warning functions */
    uint32   	    vc_warnarg[2];	/* Their arguments */
    port	    vc_port[2];		/* Remote/local ports */
};

#ifndef bit
# define bit(x)		(1<<(x))
#endif

/*
** vc_status bits.
**
*/
#define GOT_TRANS_SIG	bit(0)		/* Server received transaction signal */
#define GOT_SERVER_SIG	bit(1)		/* Client received server signal */
#define GOT_CLIENT_SIG	bit(2)		/* Server received client signal */
#define CLIENT_CB_ALIVE	bit(3)		/* Client circular not de-allocated */
#define SERVER_CB_ALIVE	bit(4)		/* Server circular not de-allocated */
#define ICLOSED		bit(5)		/* Input closed */
#define OCLOSED		bit(6)		/* Output closed */

/*
** packet types.
**
*/
#define VC_DATA		(VC_FIRST_COM+100)	/* Data requested, sent */
#define VC_HUP		(VC_FIRST_COM+101)	/* Hangup request, ack'd */
#define VC_EOF		(VC_FIRST_COM+102)	/* No more data, EOF detected */
#define VC_INTR		(VC_FIRST_COM+103)	/* Rem server got trans sig */
#define VC_EOF_HUP	(VC_FIRST_COM+104)	/* I and O cb closed */

/* 
** Argument to close.
**
*/
#define VC_IN		0x01		/* Close input virtual circuit */
#define VC_OUT		0x02		/* Close output virtual circuit */
#define VC_BOTH		0x03		/* Close virtual circuit completely */
#define VC_ASYNC	0x04		/* Don't wait for completion */

#define STACKSIZE 	8096
#define MAX_TRANS	30000		/* Maximum transaction size */

#include "_ARGS.h"

#define	vc_create 	_vc_create 
#define	vc_read  	_vc_read  
#define	vc_readall  	_vc_readall  
#define	vc_write 	_vc_write 
#define	vc_close 	_vc_close 
#define	vc_warn 	_vc_warn 
#define	vc_avail 	_vc_avail 
#define	vc_setsema 	_vc_setsema 
#define	vc_getp		_vc_getp
#define	vc_getpdone	_vc_getpdone
#define	vc_putp		_vc_putp
#define	vc_putpdone	_vc_putpdone

struct vc *vc_create 
	_ARGS((port *icap, port *ocap, int isize, int osize));
int vc_read  
	_ARGS((struct vc *vc, char *buf, int size));
int vc_readall  
	_ARGS((struct vc *vc, char *buf, int size));
int vc_write 
	_ARGS((struct vc *vc, char *buf, int size));
void vc_close 
	_ARGS((struct vc *vc, int which));
void vc_warn 
	_ARGS((struct vc *vc, int which, void (*rtn)(int warnarg), int arg));
int vc_avail 
	_ARGS((struct vc *vc, int which));
void vc_setsema 
	_ARGS((struct vc *vc, semaphore *sema));
int vc_getp
	_ARGS((struct vc *vc, bufptr *buf, int blocking));
void vc_getpdone
	_ARGS((struct vc *vc, int size));
int vc_putp
	_ARGS((struct vc *vc, bufptr *buf, int blocking));
void vc_putpdone
	_ARGS((struct vc *vc, int size));

#endif /* __VC_H__ */
