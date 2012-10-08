/*	@(#)des-fun.c	1.1	91/11/18 16:25:11 */
#include "des-private.h"

#if BIG_ENDIAN
#define P_IND(x) (((x)&04) | (3-((x)&03)))
#else
#define P_IND(x) (x)
#endif

/* This function performs f()-function in a slightly modified form. It
   starts with data which has gone through E-permutation and produces
   data which is already E-permuted for the next iteration. */

des_fun(R,schedule,ki)

des_u_long	*R;
Key_schedule	*schedule;
int		ki;

{
  des_u_long	*keyptr = &schedule->data[ki*2];
  register des_u_char	*p;
  des_u_long	F[2];
  register des_u_long	R0,R1;
  extern unsigned char spe_nano_index[];
  extern unsigned long spe_nano_store[];
  
  copy8(R[0],F[0]);
  F[0] ^= keyptr[0];
  F[1] ^= keyptr[1];
  p = (des_u_char*)F;
  R0 = R1 = 0;
  R0 ^= spe_nano_store[spe_nano_index[ 0*64 + p[P_IND(0)]]];
  R1 ^= spe_nano_store[spe_nano_index[ 1*64 + p[P_IND(0)]]];
  R0 ^= spe_nano_store[spe_nano_index[ 2*64 + p[P_IND(1)]]];
  R1 ^= spe_nano_store[spe_nano_index[ 3*64 + p[P_IND(1)]]];
  R0 ^= spe_nano_store[spe_nano_index[ 4*64 + p[P_IND(2)]]];
  R1 ^= spe_nano_store[spe_nano_index[ 5*64 + p[P_IND(2)]]];
  R0 ^= spe_nano_store[spe_nano_index[ 6*64 + p[P_IND(3)]]];
  R1 ^= spe_nano_store[spe_nano_index[ 7*64 + p[P_IND(3)]]];
  R0 ^= spe_nano_store[spe_nano_index[ 8*64 + p[P_IND(4)]]];
  R1 ^= spe_nano_store[spe_nano_index[ 9*64 + p[P_IND(4)]]];
  R0 ^= spe_nano_store[spe_nano_index[10*64 + p[P_IND(5)]]];
  R1 ^= spe_nano_store[spe_nano_index[11*64 + p[P_IND(5)]]];
  R0 ^= spe_nano_store[spe_nano_index[12*64 + p[P_IND(6)]]];
  R1 ^= spe_nano_store[spe_nano_index[13*64 + p[P_IND(6)]]];
  R0 ^= spe_nano_store[spe_nano_index[14*64 + p[P_IND(7)]]];
  R1 ^= spe_nano_store[spe_nano_index[15*64 + p[P_IND(7)]]];
  R[0] = R0;
  R[1] = R1;
}
