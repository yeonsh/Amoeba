/*	@(#)fga002.c	1.3	94/04/06 09:05:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * These routine acces the Force Gate Array type 2
 *
 * Because all the registers are scattered within a certain space
 * we cannot define an simple struct, that's why we use pointers
 * as defined in fga002.h
 */

#include "fga002.h"
#include "sram.h"
#include "debug.h"

#define READ_FGA(p)	(*(char *)(p))
#define WRITE_FGA(p, x)	(*(char *)(p) = (x))

int slot;

void
fga_init() {
  char temp;
  /*
   * enable interrupt for DUARTS
   */
  WRITE_FGA(ICRLOCAL4, EN_DUART_INT);
  WRITE_FGA(ICRLOCAL5, EN_DUART_INT);
  /*
   * enable interrupt for TIMER
   */
  WRITE_FGA(ICRLOCAL2, EN_TIMER_INT);
#if PROFILE
  WRITE_FGA(ICRLOCAL3, EN_TIMER2_INT);
#endif
#ifndef NONET
  /*
   * enable interrupt for LANCE
   */
  WRITE_FGA(ICRLOCAL6, EN_LAN_INT);
#endif
  /*
   * enable interrupt of ABORT button
   */
  WRITE_FGA(ICRABORT, EN_ABORT_INT);
  /*
   * enable interrupts for mailboxes
   */
  WRITE_FGA(ICRMBOX0, IRQ_EN | LEVEL1);
  WRITE_FGA(ICRMBOX1, IRQ_EN | LEVEL1);
  WRITE_FGA(ICRMBOX2, IRQ_EN | LEVEL2);
  WRITE_FGA(ICRMBOX3, IRQ_EN | LEVEL3);
  WRITE_FGA(ICRMBOX4, IRQ_EN | LEVEL4);
  WRITE_FGA(ICRMBOX5, IRQ_EN | LEVEL5);
  WRITE_FGA(ICRMBOX6, IRQ_EN | LEVEL6);
  WRITE_FGA(ICRMBOX7, IRQ_EN | LEVEL7);
  slot = F30SRAM->f30sram_boot_setup.f3s4k_struct.f3s_slot_number;
  /*
   * enable access to FGA from VME side
   */
  WRITE_FGA(MYVMEPAGE, 0x80 + slot);
  temp = READ_FGA(CTL5);
  WRITE_FGA(CTL5, ((temp & 0x3) | 0x0C));
  temp = READ_FGA(MYVMEPAGE);
  DPRINTF(0,("MYVMEPAGE = 0x%x, CTL5 = 0x%x\n",temp & 0xFF ,READ_FGA(CTL5)));
  /*
   * enable reset call from VME side
   */
  temp = READ_FGA(CTL2);
  WRITE_FGA(CTL2, ((temp & 0xB) | 0x04));
  /*
   * print slot number
   */
  DPRINTF(0,("VME slot %d\n", slot));
}

int fga_slotnumber()
{
    return slot;
}
