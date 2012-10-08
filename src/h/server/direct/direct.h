/*	@(#)direct.h	1.2	94/04/06 16:58:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DIRECT_H__
#define __DIRECT_H__

#define DIR_BUFFER	512

union dirbuf {
	char dir_name[DIR_BUFFER];
	struct dir_apnd {	/* for dir_append */
		capability da_cap;
		char da_name[DIR_BUFFER - sizeof(capability)];
	} dir_apnd;
	struct dir_tapnd {	/* for dir_tappend */
		capability dta_cap;
		capability dta_typecap;
		char dta_name[DIR_BUFFER - 2*sizeof(capability)];
	} dir_tapnd;
	capability dir_type;
};

#ifdef OLD_DIR_SERVER

/* directory server command codes */
#define DIR_CREATE	(DIR_FIRST_COM + 0)
#define DIR_REMOVE	(DIR_FIRST_COM + 1)
#define DIR_LIST	(DIR_FIRST_COM + 2)
#define DIR_APPEND	(DIR_FIRST_COM + 3)
#define DIR_LOOKUP	(DIR_FIRST_COM + 4)
#define DIR_DELETE	(DIR_FIRST_COM + 5)
#define DIR_RESTRICT	(DIR_FIRST_COM + 6)
#define DIR_TAPPEND	(DIR_FIRST_COM + 7)

/* error codes */
#define DIR_FAILED	STD_SYSERR
#define DIR_COMMAND	STD_COMBAD
#define DIR_BADCAP	STD_CAPBAD
#define DIR_DENIED	STD_DENIED
#define DIR_NOENT	STD_NOTFOUND
#define DIR_EXISTS	STD_EXISTS

#endif /* OLD_DIR_SERVER */

#endif /* __DIRECT_H__ */
