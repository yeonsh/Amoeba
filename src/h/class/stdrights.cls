/*	@(#)stdrights.cls	1.3	96/02/27 10:29:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Definition of the rights several servers use.
 *
 *	In the past, people have been defining right as a
 *	property of a server, which does not match ail's
 *	way of doing it. (Frankly, I don't believe it matches
 *	anything useful at all.)
 *
 *	Inheritance of rights is the most valuable kind of
 *	inheritance, since we don't have as many bits in an
 *	integer as integers in 0..2^32 - and we allow only 8 bits now...
 *
 *		Siebren
 */

class process_rights {
    /* Do NOT include "<proc.h>" */

    rights
	PSR_READ = 0x01,
	PSR_WRITE = 0x02,
	PSR_CREATE = 0x04,
	PSR_DELETE = 0x08,
	PSR_EXEC = 0x10,
	PSR_KILL = 0x20,
	PSR_DUMMY1 = 0x40,
	PSR_DUMMY2 = 0x80;
};

class bullet_rgts {
    /* Do NOT include "<server/bullet/bullet.h>" */

    rights
	BS_RGT_CREATE = 0x01,
	BS_RGT_READ = 0x02,
	BS_RGT_MODIFY = 0x04,
	BS_RGT_DELETE = 0x08,
	BS_RGT_ADMIN = 0x10;
};

class soap_rights {
    /* Do NOT include "<server/soap/soap.h>" */

    rights
	/* Define a macro to construct certain rights: */
#define SP_COLN(c)	(1 << (c-1))
	SP_COL1 = SP_COLN(1),	/* The '1' in /dw1234 */
	SP_COL2 = SP_COLN(2),
	SP_COL3 = SP_COLN(3),
	SP_COL4 = SP_COLN(4),
	SP_MODRGT = 0x40,	/* The 'w' in your /dw1234 */
	SP_DELRGT = 0x80;	/* The 'd' */
};

class stdrights {
    /*
     *	There should be something like this, but we'd better
     *	clean out our garbage before adding yet another set
     *	of rights. Anyway, here is a proposed set of names,
     *	with a list of servers that already use the same value.
     */
    rights
	RGT_CREATE = 0x01,	/* bullet */
	RGT_READ = 0x02,	/* bullet */
	RGT_MODIFY = 0x04,	/* soap, bullet */
	RGT_DELETE = 0x08;	/* soap, bullet */
};
