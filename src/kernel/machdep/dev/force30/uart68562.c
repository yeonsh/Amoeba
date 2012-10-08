/*	@(#)uart68562.c	1.4	94/04/06 09:07:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>

#include <machdep.h>
#include <arch_proto.h>

#include "global.h"
#include "map.h"
#include "ua68562info.h"

#ifndef NOTTY
#include "tty/term.h"
#endif

#define UART	ABSPTR(struct s68562 *, uart68562info.uai_devaddr)

/* magic constants for programming registers */

#define TxRESET			0x00
#define RxRESET			0x40
#define TxENB			0x02
#define RxENB			0x42
#define TxFIFA_RDY		0x02
#define RxFIFA_RDY		0x01
#define TxRDY			0x80
#define TBxRDY			0x20
#define RBxRDY			0x10

/**********************************************************/
#define TxFIFO_RDY		0x20
#define RxFIFO_RDY		0x10


struct s68562 {
			/* CHANNEL A registers */
	char da_cmr1;	/* channel mode register 1 */
	char da_cmr2;	/* channel mode register 2 */
	char da_s1r;	/* syn1 / secondary adr reg 1 */
	char da_s2r;	/* syn2 / secondary adr reg 2 */
	char da_tpr;	/* transmitter parameter reg */
	char da_ttr;	/* transmitter timing register */
	char da_rpr;	/* receiver parameter register */
	char da_rtr;	/* receiver timing register */
	char da_ctprh;	/* counter / timer preset reg H */
	char da_ctprl;	/* counter / timer preset reg L */
	char da_ctcr;	/* counter / timer control reg */
	char da_omr;	/* output and misc register */
	char da_cth;	/* counter / timer High */
	char da_ctl;	/* counter / timer Low */
	char da_pcr;	/* pin configuration register */
	char da_ccr;	/* channel command register */
	char da_txdata0;	/* transmitter fifo */
	char da_txdata1;	/* transmitter fifo */
	char da_txdata2;	/* transmitter fifo */
	char da_txdata3;	/* transmitter fifo */
	char da_rxdata0;	/* receiver fifo */
	char da_rxdata1;	/* receiver fifo */
	char da_rxdata2;	/* receiver fifo */
	char da_rxdata3;	/* receiver fifo */
	char da_rsr;	/* receiver status register */
	char da_trsr;	/* receiver / transmitter stat reg */
	char da_ictsr;	/* input + cntr / timer stat reg */
	char da_gsr;	/* general status register */
	char da_ier;	/* interrupt enable register */
	char notused0;
	char da_ivr;	/* interrupt vector register */
	char da_icr;	/* interrupt control register */
			/* CHANNEL B (console port) registers */
	char db_cmr1;	/* channel mode register 1 */
	char db_cmr2;	/* channel mode register 2 */
	char db_s1r;	/* syn1 / secondary adr reg 1 */
	char db_s2r;	/* syn2 / secondary adr reg 2 */
	char db_tpr;	/* transmitter parameter reg */
	char db_ttr;	/* transmitter timing register */
	char db_rpr;	/* receiver parameter register */
	char db_rtr;	/* receiver timing register */
	char db_ctprh;	/* counter / timer preset reg H */
	char db_ctprl;	/* counter / timer preset reg L */
	char db_ctcr;	/* counter / timer control reg */
	char db_omr;	/* output and misc register */
	char db_cth;	/* counter / timer High */
	char db_ctl;	/* counter / timer Low */
	char db_pcr;	/* pin configuration register */
	char db_ccr;	/* channel command register */
	char db_txdata0;	/* transmitter fifo */
	char db_txdata1;	/* transmitter fifo */
	char db_txdata2;	/* transmitter fifo */
	char db_txdata3;	/* transmitter fifo */
	char db_rxdata0;	/* receiver fifo */
	char db_rxdata1;	/* receiver fifo */
	char db_rxdata2;	/* receiver fifo */
	char db_rxdata3;	/* receiver fifo */
	char db_rsr;	/* receiver status register */
	char db_trsr;	/* receiver / transmitter stat reg */
	char db_ictsr;	/* input + cntr / timer stat reg */
	char db_gsr;	/* general status register */
	char db_ier;	/* interrupt enable register */
	char notused1;
	char db_ivrm;	/* interrupt vector register */
	char notused2;
};


/**********************************************************/
void inituart _ARGS(( void ));
static void intuart _ARGS(( int sp ));
static int tty_putchar _ARGS(( int c ));


#ifndef NOTTY
static int queued;

static writestub(arg)
long arg;			/* arg = number of tty that was interrupted */
{
    extern writeint();

    queued &= ~(1 << arg);	/* switch off queued bit for this tty */
    writeint(arg);
}
#endif


static int
tty_putchar(c)
int c;
{
  while (!(UART->da_gsr & TxFIFO_RDY))
	;
  UART->db_txdata0 = c;
  return 0;
}


#ifndef NO_OUTPUT_ON_CONSOLE
/*ARGSUSED*/
putchar(iop, c)
struct io * iop;
{
    return tty_putchar(c);
}
#endif

/* uart interrupt routine */

/*ARGSUSED*/
static void intuart(sp)
int sp;
{
    int status;
#ifndef NOTTY
    int	readint();
    int	writestub();
#endif

    status = UART->da_gsr;
    if (status & RBxRDY) {
#ifndef NOTTY
	    enqueue(readint, (UART->db_rxdata0 & 0xffL) );
#else
	    {	register char c;
	    
		c = UART->db_rxdata0;
		(void) tty_putchar(c);
	    }
#endif
	    return;
    }
    if (status & TBxRDY) {
#ifndef NOTTY
	    if (!queued)
	    {
		queued = 1;
		enqueue(writestub, 0L);
	    }
#endif
	    return;
    }
    panic("bad UART interrupt");
}


void
inituart(){
  short vector;

  /* to understand these magic values read the 68562 manual */

  vector = uart68562info.uai_vector;

  UART->db_ccr = TxRESET;		/* reset transmitter */
  UART->db_ccr = RxRESET;		/* reset receiver */
  UART->db_cmr1 = 0x07;		/* async, no parity */
  UART->db_cmr2 = 0x38;		/* polled or IRQ */
  UART->db_tpr = 0x73;		/* 8 bits */
  UART->db_rpr = 0x03;		/* 8 bits */
  UART->db_omr = 0xE1;		/* same TPR + RTS */
  UART->db_pcr = 0x2F;		/* pin configuration */
  UART->db_rtr = 0x2D;		/* receiver clock from BRG 9600 baud */
  UART->db_ttr = 0x3D;		/* transmitter clock from BRG 9600 baud */
  UART->db_ier = 0x10;		/* interrupt on receive data */
				/* we write to channel a registers !! */
  UART->da_ivr = (char) vector;	/* interrupt vector */
  UART->da_icr = 0x81;		/* enable interrupt for channel B */
				/* write to channel B registers */
  UART->db_ccr = TxRESET;	/* reset transmitter */
  UART->db_ccr = TxENB;		/* enable transmitter */
  UART->db_ccr = RxRESET;	/* reset receiver */
  UART->db_ccr = RxENB;		/* enable receiver */

  (void) setvec(vector, intuart);
}

#ifndef NOTTY

/*ARGSUSED*/
static uart_stat(iop)
struct io *iop;
{
  printf("no status available\n");
}

/*ARGSUSED*/
uart_init(cdp, first)
struct cdevsw *cdp;
struct io *first;
{
  cdp->cd_write = putchar;
#if !VERY_SIMPLE && !NO_IOCTL
  cdp->cd_setDTR = 0;
  cdp->cd_setBRK = 0;
  cdp->cd_baud = 0;
#endif !VERY_SIMPLE && !NO_IOCTL
#if !VERY_SIMPLE || RTSCTS
  cdp->cd_isusp = 0;
  cdp->cd_iresume = 0;
#endif !VERY_SIMPLE || RTSCTS
  cdp->cd_status = uart_stat;
  return 3;
}

#endif /* NOTTY */
