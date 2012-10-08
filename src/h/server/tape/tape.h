/*	@(#)tape.h	1.1	96/02/27 10:38:42 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __TAPE_TAPE_H__
#define __TAPE_TAPE_H__

/*
**	tape.h
**
**	Constants and types related to the tape server.
*/

typedef	long	tape_addr;	/* tape address */

/*
** decode/encode an tape address from/to host architecture to/from big-endian
** format.  DEC_L_BE and ENC_L_BE are defined in byteorder.h
*/

#define	DEC_TAPE_ADDR_BE(x)	dec_l_be(x)
#define	ENC_TAPE_ADDR_BE(x)	enc_l_be(x)

/* size of tape thread's transaction buffer */
#define	T_REQBUFSZ		(29 * 1024)

/* tape server commands */
#define	TAPE_READ		(TAPE_FIRST_COM + 1)
#define	TAPE_SIZE		(TAPE_FIRST_COM + 2)
#define	TAPE_WRITE		(TAPE_FIRST_COM + 3)
#define	TAPE_WRITE_EOF		(TAPE_FIRST_COM + 4)
#define	TAPE_FILE_SKIP		(TAPE_FIRST_COM + 5)
#define	TAPE_REC_SKIP		(TAPE_FIRST_COM + 6)
#define	TAPE_REWIND		(TAPE_FIRST_COM + 7)
#define	TAPE_REWIMM		(TAPE_FIRST_COM + 8)
#define	TAPE_ERASE		(TAPE_FIRST_COM + 9)
#define	TAPE_LOAD		(TAPE_FIRST_COM + 10)
#define	TAPE_UNLOAD		(TAPE_FIRST_COM + 11)
#define	TAPE_FILE_POS		(TAPE_FIRST_COM + 12)
#define	TAPE_REC_POS		(TAPE_FIRST_COM + 13)

/* tape server error codes */
#define TAPE_NOTAPE		(TAPE_FIRST_ERR - 1)
#define TAPE_WRITE_PROT         (TAPE_FIRST_ERR - 2)
#define TAPE_CMD_ABORTED        (TAPE_FIRST_ERR - 3)
#define TAPE_BOT_ERR            (TAPE_FIRST_ERR - 4)
#define TAPE_EOF                (TAPE_FIRST_ERR - 5)
#define TAPE_REC_DAT_TRUNC      (TAPE_FIRST_ERR - 6)
#define TAPE_POS_LOST           (TAPE_FIRST_ERR - 7)
#define TAPE_EOT           	(TAPE_FIRST_ERR - 8)
#define TAPE_MEDIA_ERR         	(TAPE_FIRST_ERR - 9)

#endif /* __TAPE_TAPE_H__ */
