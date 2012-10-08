/*	@(#)menu.c	1.4	96/02/27 10:12:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * MENU
 *
 *	This routine implements the menus for the disklabel program.
 *	They accept as input an array of structs defining the menu.
 *	The last entry in the array must contain NULL pointers.
 *	The struct contains the text to be printed in the menu and the name
 *	of the function to call if that menu item is selected.  The array
 *	index of the selected entry is returned.
 *
 * Author:  Gregory J. Sharp, June 1992
 */

#include "amoeba.h"
#include "stdio.h"
#include "stdlib.h"
#include "menu.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "proto.h"

int
execute_menu(headline, menu)
char *		headline;
struct menu *	menu;
{
    int			numitems;
    int			num;
    struct menu *	mp;

    /* Display the menu and count the number of items in the menu */
    printf("\n\n");
    printf(headline);
    for (numitems = 0, mp = menu;
	 mp->m_text != (char *) 0 && mp->m_func != (void (*)()) 0;
	 numitems++, mp++)
    {
	printf(" %2d. %s\n", numitems, mp->m_text);
    }

    /* Get the user's selection */
    do
    {
	num = getnumber("\nYour selection: ");
    } while (num >= numitems && printf("no such menu entry\n"));

    return num;
}
