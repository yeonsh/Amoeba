/*	@(#)fga002.h	1.3	94/04/06 09:05:36 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Force gate array FGA002
 *
 * for a complete description see the Force gate array manual
 * for initialisation we need to disable the VME interrupts
 * enable the mailboxes. Setup the dualported ram acces
 * for all boards disable the busarbiter.
 * Select the FAIR arbitration scheme and alow the master CPU to
 * acces all the other gate arrays in the system.
 */

/*
 * The following registers should be left in the default state
 * for proper operation of the onboard memory
 * CTL10
 * CTL11
 * MAINUM
 * MAINUU
 * CTL16
 */

/*
 * We also keep our hands from the following registers
 * CTL3 (VSB bus)
 * CTL14 (eprom decoding)
 * CTL9  (eprom dsack)
 * CSLIO (local IO)
 * RDLIO (local IO)
 * WRLIO (local IO)
 * CTL6  (dsack control)
 * CTL3  (opt16)
 * CTL7  (bus release)
 */

/*
 * to disable the internal arbiter reset bit(2) in CTL1
 *
 * to enable the fair arbitration bit reset bit(1) in CTL8
 * 
 * to enable the dual ported ram
 *
 * VMEPAGE
 * BOTTOMPAGEU
 * BOTTOMPAGEL
 * TOPPAGEU
 * TOPPAGEL
 * ENAMCODE
 * CTL15
 *
 * to enable acces to the FGA002 registers from VME
 *
 * MYVMEPAGE
 * CTL5
 */

/*
 * interrupt management
 *
 * to disable interrupts from the VME bus
 *
 * do not touch CTL3
 *
 * ICRVME7	(reset bit(3))
 * ICRVME6	(reset bit(3))
 * ICRVME5	(reset bit(3))
 * ICRVME4	(reset bit(3))
 * ICRVME3	(reset bit(3))
 * ICRVME2	(reset bit(3))
 * ICRVME1	(reset bit(3))
 */

#define bit(n)	(1<<(n))

#define	ICRMBOX0	0xFFD00000
#define	ICRMBOX1	0xFFD00004
#define	ICRMBOX2	0xFFD00008
#define	ICRMBOX3	0xFFD0000C
#define	ICRMBOX4	0xFFD00010
#define	ICRMBOX5	0xFFD00014
#define	ICRMBOX6	0xFFD00018
#define	ICRMBOX7	0xFFD0001C

#define	ICRVME1		0xFFD00204
#define	ICRVME2		0xFFD00208
#define	ICRVME3		0xFFD0020C
#define	ICRVME4		0xFFD00210
#define	ICRVME5		0xFFD00214
#define	ICRVME6		0xFFD00218
#define	ICRVME7		0xFFD0021C
#define	ICRTIM0		0xFFD00220

#define	ICRDMANORM	0xFFD00230
#define	ICRDMAERR	0xFFD00234
#define	ICRFMB0REF	0xFFD00240
#define	ICRFMB1REF	0xFFD00244
#define	ICRFMB0MES	0xFFD00248
#define	ICRFMB1MES	0xFFD0024C
#define	ICRPARITY	0xFFD00258

#define IRQ_EN	bit(3)
#define LEVEL0	0
#define LEVEL1	1
#define LEVEL2	2
#define LEVEL3	3
#define LEVEL4	4
#define LEVEL5	5
#define LEVEL6	6
#define LEVEL7	7

#define EN_LAN_INT	0x1E
#define EN_DUART_INT	0x1C
#define EN_TIMER_INT	0x4D
#if PROFILE
#define EN_TIMER2_INT	0x4D
#endif
#define EN_ABORT_INT	0x4F

#define	ICRABORT	0xFFD00280
#define	ICRACFAIL	0xFFD00284
#define	ICRSYSFAIL	0xFFD00288
#define	ICRLOCAL0	0xFFD0028C
#define	ICRLOCAL1	0xFFD00290
#define	ICRLOCAL2	0xFFD00294
#define	ICRLOCAL3	0xFFD00298
#define	ICRLOCAL4	0xFFD0029C
#define	ICRLOCAL5	0xFFD002A0
#define	ICRLOCAL6	0xFFD002A4
#define	ICRLOCAL7	0xFFD002A8

#define EDGE_LEVEL	bit(6)
#define ACTIVITY	bit(5)
#define AUTOCLEAR	bit(4)


#define CTL1		0xFFD00238
#define CTL2		0xFFD0023C
#define CTL3		0xFFD00250
#define CTL4		0xFFD00254
#define	AUXPINCTL	0xFFD00260
#define	CTL5		0xFFD00264
#define	AUXFIFWEX	0xFFD00268
#define	AUXFIFREX	0xFFD0026C
#define	CTL6		0xFFD00270
#define	CTL7		0xFFD00274
#define	CTL8		0xFFD00278
#define	CTL9		0xFFD0027C
#define ENAMCODE	0xFFD002B4
#define	CTL10		0xFFD002C0
#define	CTL11		0xFFD002C4
#define	MAINUM		0xFFD002C8
#define	MAINUU		0xFFD002CC
#define	BOTTOMPAGEU	0xFFD002D0
#define	BOTTOMPAGEL	0xFFD002D4
#define	TOPPAGEU	0xFFD002D8
#define	TOPPAGEL	0xFFD002DC

#define	MYVMEPAGE	0xFFD002FC
#define	TIM0PRELOAD	0xFFD00300
#define	TIM0CTL		0xFFD00310
#define	DMASRCATT	0xFFD00320
#define	DMADSTATT	0xFFD00324
#define DMAGENERAL	0xFFD00328
#define CTL12		0xFFD0032C
#define LIOTIMING	0xFFD00330
#define LOCALIACK	0xFFD00334
#define FMBCTL		0xFFD00338
#define FMBAREA		0xFFD0033C
#define AUXSRCSTART	0xFFD00340
#define AUXDSTSTART	0xFFD00344
#define AUXSRCTERM	0xFFD00348
#define AUXDSTTERM	0xFFD0034C
#define	CTL13		0xFFD00350
#define CTL14		0xFFD00354
#define CTL15		0xFFD00358
#define CTL16		0xFFD0035C
#define PTYLL		0xFFD00400
#define PTYLM		0xFFD00404
#define PTYUM		0xFFD00408
#define PTYUU		0xFFD0040C
#define PTYATT		0xFFD00410
#define IDENT		0xFFD0041C
#define SPECIAL		0xFFD00420
#define SPECIALENA	0xFFD00424

#define ISLOCAL0	0xFFD00480
#define ISLOCAL1	0xFFD00484
#define ISLOCAL2	0xFFD00488
#define ISLOCAL3	0xFFD0048C
#define ISLOCAL4	0xFFD00490
#define ISLOCAL5	0xFFD00494
#define ISLOCAL6	0xFFD00498
#define ISLOCAL7	0xFFD0049C

#define ISTIM0		0xFFD004A0
#define ISDMANORM	0xFFD004B0
#define	ISDMAERR	0xFFD004B4
#define MBOX0		0xFFD80000
#define MBOX1		0xFFD80004
#define MBOX2		0xFFD80008
#define MBOX3		0xFFD8000C
#define MBOX4		0xFFD80010
#define MBOX5		0xFFD80014
#define MBOX6		0xFFD80018
#define MBOX7		0xFFD8001C
