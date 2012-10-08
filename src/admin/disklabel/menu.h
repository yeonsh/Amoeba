/*	@(#)menu.h	1.2	94/04/06 11:43:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#if !defined(__MENU_H__)

#define	__MENU_H__
#include "_ARGS.h"

/*
 * MENU.H
 *
 *	Menus are just arrays of struct menu with the last entry in the
 *	array containing NULL pointers.
 */

struct menu {
	char *	m_text;
	void	(*m_func) _ARGS(( void ));
};

int execute_menu _ARGS(( char * headline, struct menu * menu ));

#endif /* __MENU_H__ */
