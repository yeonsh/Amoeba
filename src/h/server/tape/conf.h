/*	@(#)conf.h	1.3	94/04/06 17:16:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __CONF_H__
#define __CONF_H__

/*
** tape_contr_tab_type holds all information necessary to
** 'link' a tape driver to the tape server.
**
** Peter Bosch, 181289, peterb@cwi.nl
**
*/

typedef struct {
	char   *tape_driver_name;	/* Driver name */
	uint32	tape_devaddr;		/* Controller device address */
	uint32	tape_minunit;		/* Start looking at this unit */
	uint32	tape_maxunit;		/* Stop looking at this unit */
	int	tape_ivec;		/* Interrupt vector for controller */
	errstat	(*tape_ctlr_init)();	/* Controller init routine */
	errstat	(*tape_unit_init)();	/* Unit init routine */
	errstat	(*tape_info)();		/* Unit info routine */
	errstat	(*tape_status)();	/* Unit status routine */
	errstat (*tape_read)();		/* Unit read routine */
	errstat (*tape_write)();	/* Unit write routine */
	errstat	(*tape_write_eof)();	/* Unit write end-of-file routine */
	errstat	(*tape_file_skip)();	/* Unit file skip routine */
	errstat	(*tape_record_skip)();	/* Unit record skip routine */
	errstat	(*tape_rewind)();	/* Unit rewind routine */
	errstat	(*tape_erase)();	/* Unit erase routine */
	errstat	(*tape_load)();		/* Unit load routine */
	errstat	(*tape_unload)();	/* Unit unload routine */
	errstat	(*tape_file_pos)();	/* Unit file position */
	errstat	(*tape_rec_pos)();	/* Unit record position */
} tape_contr_tab_type;

#endif /* __CONF_H__ */
