/*	@(#)cbc-encrypt.c	1.1	91/11/18 16:24:25 */
#include "des-private.h"

C_Block		des_zero_block = {{0}};

int
des_cbc_encrypt(input,output,length,schedule,ivec,mode)

C_Block		*input;
C_Block		*output;
int		length;
Key_schedule	*schedule;
C_Block		*ivec;
int		mode;

{
  C_Block	vbuf;
  C_Block	ibuf;
  C_Block	V2;
  int		decrypt;
  int		i;

  decrypt = (mode & DES_DECRYPT);
  if (ivec) {
    vbuf = *ivec;
  } else {
    vbuf = des_zero_block;
  }
  for (; length > 0; length -= 8, input++,output++) {
    if (length < 8) {
      ibuf = *input;
      for(i = length; i < 8; i++)
	ibuf.data[i] = 0;
      input = &ibuf;
    }
    if (decrypt) {
      V2 = *input;
    } else {
      val4(input->data[0]) ^= val4(vbuf.data[0]);
      val4(input->data[4]) ^= val4(vbuf.data[4]);
    }
    des_ecb_encrypt(input,output,schedule,mode);
    if (decrypt) {
      val4(output->data[0]) ^= val4(vbuf.data[0]);
      val4(output->data[4]) ^= val4(vbuf.data[4]);
      vbuf = V2;
    } else {
      vbuf = *output;
    }
  }
  if (ivec) {
    *ivec = vbuf;
  }
}
