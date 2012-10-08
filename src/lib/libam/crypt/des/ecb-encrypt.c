/*	@(#)ecb-encrypt.c	1.2	92/05/13 14:30:40 */
#include "des-private.h"

extern des_u_long des_unexpand();

/* input data is not modified. output is written after input has been
   read */

/* The least significant bit of input->data[0] is bit # 1 in
   DES-sepcification etc. */

int
des_ecb_encrypt(input,output,schedule,mode)

C_Block		*input;
C_Block		*output;
Key_schedule	*schedule;
int		mode;

{
  C_Block	ibuf;
  des_u_long	L[2],R[2];
  des_u_long	Lnext[2];
  int	i;
  int	encrypt;

#if BIG_ENDIAN
  des_reverse(input->data, ibuf.data);
  if (!(mode & DES_NOIPERM)) {
    des_do_iperm((des_u_long *)&ibuf, (des_u_long *)&ibuf);
  }
#else
  if (!(mode & DES_NOIPERM)) {
    des_do_iperm((des_u_long *) input, (des_u_long *)&ibuf);
  } else {
    copy8(*input,ibuf);
  }
#endif
  encrypt = !(mode & DES_DECRYPT);
  des_expand(&ibuf.data[0], (des_u_char *) &L[0]);
  des_expand(&ibuf.data[4], (des_u_char *) &R[0]);
  for(i = 0; i < 16; i++) {
    copy8(*R,*Lnext);
    des_fun(R,schedule,encrypt ? i : 15 - i);
    R[0] ^= L[0];
    R[1] ^= L[1];
    copy8(*Lnext,*L);
  }
  
  val4(ibuf.data[0]) = des_unexpand((des_u_char *) R);
  val4(ibuf.data[4]) = des_unexpand((des_u_char *) L);
#if BIG_ENDIAN
  if (!(mode & DES_NOFPERM))
    des_do_fperm((des_u_long *)&ibuf, (des_u_long *)&ibuf);
  des_reverse(ibuf.data, output->data);
#else
  if (!(mode & DES_NOFPERM))
    des_do_fperm((des_u_long *)&ibuf, (des_u_long *)output);
  else
    copy8(ibuf,*output);
#endif
}
