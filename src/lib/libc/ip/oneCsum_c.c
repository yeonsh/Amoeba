/*	@(#)oneCsum_c.c	1.2	94/04/07 10:34:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
oneCsum_c.c

Part of the internet checksum (see RFC 1071 Computing the Internet checksum)
*/

#include <sys/types.h>

#include <_ARGS.h>

#include <server/ip/types.h>
#include <server/ip/gen/oneCsum.h>

u16_t oneC_sum (prev, data, size)
u16_t prev;
register u16_t *data;
size_t size;
{
	register unsigned long tmp;
	register u16_t *hi_data;
	u8_t word[2];

	tmp= prev;

	hi_data= data+(size/2);
	while (data<hi_data)
	{
		tmp += *data++;
	}

	if (size & 1)
	{
		word[0]= *(u8_t *)data;
		word[1]= 0;
		tmp += *(u16_t *)word;
	}
	tmp= (tmp & 0xffff) + (tmp >> 16);
	if (tmp > 0xffff)
		tmp++;
	return tmp;
}

