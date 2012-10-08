/*	@(#)i386_proto.h	1.3	94/04/06 09:20:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __I386_PROTO_H__
#define __I386_PROTO_H__

#include <amoeba.h>
#include <_ARGS.h>
#include <machdep.h>
#include <fault.h>

void disable	_ARGS((void));
void enable	_ARGS((void));

void out_byte	_ARGS((int _port, int _val));
void out_word	_ARGS((int _port, int _val));
void out_long	_ARGS((int _port, long _val));
int  in_byte	_ARGS((int _port));
int  in_word	_ARGS((int _port));
long in_long	_ARGS((int _port));

/* NB. always a bytecount, even for word version! */
void outs_byte	_ARGS((int _port, char * _ptr, int _bytecnt));
void outs_word	_ARGS((int _port, char * _ptr, int _bytecnt));
void ins_byte	_ARGS((int _port, char * _ptr, int _bytecnt));
void ins_word	_ARGS((int _port, char * _ptr, int _bytecnt));

int  get_flags	_ARGS((void));
void set_flags	_ARGS((int _flags));

int  getCR0	_ARGS((void));
int  getCR2	_ARGS((void));
int  getCR3	_ARGS((void));
void setCR0	_ARGS((int));
void setCR3	_ARGS((phys_bytes));

void setirq	_ARGS((int _irq, void (*_handler)()));
void unsetirq	_ARGS((int _irq));

void flushTLB	_ARGS((void));

void pic_enable	 _ARGS((int _irq));
void pic_disable _ARGS((int _irq));
void pic_start	 _ARGS((void));
void pic_stop	 _ARGS((void));

void pit_delay	_ARGS((int _msec));

int  dma_setup	_ARGS((int _mode, int _channel, char *_addr, int _count));
void dma_done	_ARGS((int _channel));
void dma_start	_ARGS((int _channel));

void framedump	_ARGS((struct fault *_frame, uint32 ss, uint32 esp));

void page_mapin	_ARGS((phys_bytes addr, phys_bytes size));

void crt_enabledisplay _ARGS((void));

void bleep	_ARGS((void));

void newevent	_ARGS((int, int, int, int));

#endif /* __SYS__PROTO_H__ */
