/*	@(#)cmdreg.h	1.6	94/04/06 15:40:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CMDREG_H__
#define __CMDREG_H__

/*
** cmdreg.h
**
**	This file contains the list of first and last command codes assigned
**	to each registered server.  Only registered servers are listed.
**	If you wish to register a standard server then mail
**		amoeba-support@cs.vu.nl
**	with your request for registration.
**	The set of error codes is the negative of the command codes.
**	Note that the RPC error codes are in the range RESERVED_FIRST to
**	RESERVED LAST.
**
**	Registered commands take numbers in the range
**		1000 to (NON_REGISTERED_FIRST - 1)
**	Developers may use command numbers in the range
**		NON_REGISTERED_FIRST to (NON_REGISTERED_LAST - 1)
**		You should make all your command numbers relative to these
**		constants in case they change in Amoeba 4.
**
**	Each server is assigned commands in units of 100.
**	If necessary a server may take more two or more consecutive quanta.
**	Command numbers 1 to 999 are reserved and may NOT be used.
**	The error codes that correspond to these command numbers are for
**	RPC errors.
**	Command numbers from 1000 to 1999 are reserved for standard commands
**	that all servers should implement where relevant.
**
**  Author:
**	Greg Sharp
*/

#define	RESERVED_FIRST			1
#define	RESERVED_LAST			999

/* The standard commands that all servers should support */
#define STD_FIRST_COM			1000
#define STD_LAST_COM			1999

#define STD_FIRST_ERR			(-(STD_FIRST_COM))
#define STD_LAST_ERR			(-(STD_LAST_COM))

/*
** A subset of the standard error codes is reserved for exec_file's errors
** which don't quite rate as standard server errors
*/
#define	AM_EXEC_FIRST_ERR		(STD_LAST_ERR + 100)
#define	AM_EXEC_LAST_ERR		(STD_LAST_ERR)

/*
**	Registered Servers
*/
/* Bullet Server */
#define	BULLET_FIRST_COM		2000
#define	BULLET_LAST_COM			2099

#define BULLET_FIRST_ERR		(-BULLET_FIRST_COM)
#define BULLET_LAST_ERR			(-BULLET_LAST_COM)

/* Directory Server */
#define SP_FIRST_COM			2100
#define SP_LAST_COM			2199

#define SP_FIRST_ERR			(-SP_FIRST_COM)
#define SP_LAST_ERR			(-SP_LAST_COM)

/* Virtual-Disk Thread */
#define	DISK_FIRST_COM			2200
#define	DISK_LAST_COM			2299

#define DISK_FIRST_ERR			(-DISK_FIRST_COM)
#define DISK_LAST_ERR			(-DISK_LAST_COM)

/* Kernel Process Server */
#define PS_FIRST_COM			2300
#define PS_LAST_COM			2399

#define PS_FIRST_ERR			(-PS_FIRST_COM)
#define PS_LAST_ERR			(-PS_LAST_COM)

/* Kernel Time of Day Server */
#define	TOD_FIRST_COM			2400
#define	TOD_LAST_COM			2499

#define TOD_FIRST_ERR			(-TOD_FIRST_COM)
#define TOD_LAST_ERR			(-TOD_LAST_COM)

/* Kernel Sys Server */
#define	SYS_FIRST_COM			2500
#define	SYS_LAST_COM			2599

#define	SYS_FIRST_ERR			(-SYS_FIRST_COM)
#define	SYS_LAST_ERR			(-SYS_LAST_COM)

/* Random Server */
#define	RND_FIRST_COM			2600
#define	RND_LAST_COM			2699

#define	RND_FIRST_ERR			(-RND_FIRST_COM)
#define	RND_LAST_ERR			(-RND_LAST_COM)

/* Old Directory Server */
#define	DIR_FIRST_COM			2700
#define	DIR_LAST_COM			2799

#define	DIR_FIRST_ERR			(-DIR_FIRST_COM)
#define	DIR_LAST_ERR			(-DIR_LAST_COM)

/* Session server (for Ajax) */
#define SES_FIRST_COM			2800
#define SES_LAST_COM			2899

#define SES_FIRST_ERR			(-SES_FIRST_COM)
#define SES_LAST_ERR			(-SES_LAST_COM)

/* Etherhook Server */
#define EH_FIRST_COM			2900
#define EH_LAST_COM			2999

#define EH_FIRST_ERR			(-EH_FIRST_COM)
#define EH_LAST_ERR			(-EH_LAST_COM)

/* Run Server */
#define RUN_FIRST_COM			3000
#define RUN_LAST_COM			3099

#define RUN_FIRST_ERR			(-RUN_FIRST_COM)
#define RUN_LAST_ERR			(-RUN_LAST_COM)

/* Virtual Circuit Server */
#define VC_FIRST_COM			3100
#define VC_LAST_COM			3199

#define VC_FIRST_ERR			(-VC_FIRST_COM)
#define VC_LAST_ERR			(-VC_LAST_COM)

/* Termios Interface */
#define TIOS_FIRST_COM			3200
#define TIOS_LAST_COM			3299

#define TIOS_FIRST_ERR			(-TIOS_FIRST_COM)
#define TIOS_LAST_ERR			(-TIOS_LAST_COM)

/* Tape Server */
#define TAPE_FIRST_COM			3300
#define TAPE_LAST_COM			3399

#define TAPE_FIRST_ERR			(-TAPE_FIRST_COM)
#define TAPE_LAST_ERR			(-TAPE_LAST_COM)

/* X Server(s) */
#define X_FIRST_COM			3400
#define X_LAST_COM			3499

#define X_FIRST_ERR			(-X_FIRST_COM)
#define X_LAST_ERR			(-X_LAST_COM)

/* IOP Server */
#define IOP_FIRST_COM			3500
#define IOP_LAST_COM			3599

#define IOP_FIRST_ERR			(-IOP_FIRST_COM)
#define IOP_LAST_ERR			(-IOP_LAST_COM)

/* Boot Server */
#define BOOT_FIRST_COM			3600
#define BOOT_LAST_COM			3699

#define BOOT_FIRST_ERR			(-BOOT_FIRST_COM)
#define BOOT_LAST_ERR			(-BOOT_LAST_COM)

/* MMDF Servers */
#define MMDF_FIRST_COM			3700
#define MMDF_LAST_COM			3799

#define MMDF_FIRST_ERR			(-MMDF_FIRST_COM)
#define MMDF_LAST_ERR			(-MMDF_LAST_COM)

/* TCPIP Server */
#define TCPIP_FIRST_COM			3800
#define TCPIP_LAST_COM			3899

#define TCPIP_FIRST_ERR			(-TCPIP_FIRST_COM)
#define TCPIP_LAST_ERR			(-TCPIP_LAST_COM)

/* Swiss Army Knife Server */
#define SAK_FIRST_COM			3900
#define SAK_LAST_COM			3999

#define SAK_FIRST_ERR			(-SAK_FIRST_COM)
#define SAK_LAST_ERR			(-SAK_LAST_COM)

/* TTY Server  and "file" interface */
#define TTY_FIRST_COM			4000
#define TTY_LAST_COM			4099

#define TTY_FIRST_ERR			(-TTY_FIRST_COM)
#define TTY_LAST_ERR			(-TTY_LAST_COM)

/* Kernel profile server */
#define PROFILE_FIRST_COM		4100
#define PROFILE_LAST_COM		4199

#define PROFILE_FIRST_ERR		(-PROFILE_FIRST_COM)
#define PROFILE_LAST_ERR		(-PROFILE_LAST_COM)

/* fifo server */
#define FIFO_FIRST_COM			4200
#define FIFO_LAST_COM			4299

#define FIFO_FIRST_ERR			(-FIFO_FIRST_COM)
#define FIFO_LAST_ERR			(-FIFO_LAST_COM)

/*
**	The First Number For Non-Registered Servers
*/

#define	UNREGISTERED_FIRST_COM		15000
#define	UNREGISTERED_FIRST_ERR		(-UNREGISTERED_FIRST_COM)

#ifndef AMOEBA_4
#	define UNREGISTERED_LAST_COM	31000
#else
#	define UNREGISTERED_LAST_COM	1000000000
#endif
#define	UNREGISTERED_LAST_ERR		(-UNREGISTERED_LAST_COM)

#endif /* __CMDREG_H__ */
