/*	@(#)des-perms.c	1.2	92/05/13 14:30:38 */
#include	"des-private.h"

extern unsigned char des_Iperm[], des_Fperm[];

static void
des_do_perm(ibuf, obuf, table)
register des_u_long *ibuf, *obuf;
register unsigned char *table;
{
  register word,i,bitno;
  register des_u_long mask,result;
  des_u_long tobuf[2];

  for (word=0;word<=1;word++) {
	result = 0;
	for (i=0, mask=1;i<32;i++, mask <<= 1) {
		bitno = *table++;
		if (ibuf[bitno>>5] & (1<<(bitno&31)))
			result |= mask;
	}
	tobuf[word] = result;
  }
  obuf[0] = tobuf[0]; obuf[1] = tobuf[1];
}

des_do_iperm(ibuf,obuf)
des_u_long	*ibuf,*obuf;
{

  des_do_perm(ibuf, obuf, des_Iperm);
}

des_do_fperm(ibuf,obuf)
des_u_long	*ibuf,*obuf;
{

  des_do_perm(ibuf, obuf, des_Fperm);
}
