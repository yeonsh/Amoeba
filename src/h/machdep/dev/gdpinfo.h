/*	@(#)gdpinfo.h	1.3	94/04/06 16:44:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Generic dp8390 driver information.
 */
typedef struct dpinfo {
    int		dpi_reg;		/* board's I/O register */
    int		dpi_irq;		/* board's IRQ level */
    int		dpi_16bit;		/* has 16-bit access capabilities */
    phys_bytes	dpi_basemem;		/* base of onboard memory */
    int		(*dpi_init)();		/* initialize ethernet board */
    dphead_p	(*dpi_gethead)();	/* get message header */
    int		(*dpi_getpkt)();	/* get packet from board */
    int		(*dpi_putpkt)();	/* put packet on board */
    void	(*dpi_stop)();		/* stop ethernet board */
} dpi_t, *dpi_p;
