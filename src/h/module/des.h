/*	@(#)des.h	1.3	96/02/27 10:32:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_DES_H__
#define __MODULE_DES_H__

#include "_ARGS.h"

typedef unsigned char	des_u_char; /* This should be a 8-bit unsigned type */
typedef unsigned long	des_u_long; /* This should be a 32-bit unsigned type */

typedef struct {
  des_u_char	data[8];
} C_Block;

typedef struct {
  des_u_long	data[32];
} Key_schedule;

#define DES_ENCRYPT	0x0000
#define DES_DECRYPT	0x0001
#define DES_NOIPERM	0x0100
#define DES_NOFPERM	0x0200

#if 0
extern int		des_hash_inited;
extern Key_schedule	des_hash_key1;
extern Key_schedule	des_hash_key2;
#define DES_HASH_INIT() (des_hash_inited ? 0 : des_hash_init())
#endif

extern C_Block		des_zero_block;

#define	des_set_key	_des_set_key
#define	des_ecb_encrypt	_des_ecb_encrypt

int des_set_key  _ARGS(( C_Block * key, Key_schedule * sched));
int des_ecb_encrypt  _ARGS(( C_Block * inp, C_Block * outp,
					    Key_schedule * sched, int mode ));

#endif /* __MODULE_DES_H__ */
