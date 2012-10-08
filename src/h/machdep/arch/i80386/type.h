/*	@(#)type.h	1.3	94/04/06 15:59:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __TYPE_H__
#define __TYPE_H__

/*
 * Generic descriptor table layout pointer
 */
struct desctableptr_s {
    uint16 limit;
    uint16 base_lo;
    uint16 base_hi;
    uint16 junk;
};

/*
 * Gate descriptor layout
 */
struct gatedesc_s {
    uint16 offset_low;
    uint16 selector;
    uint8 pad;			/* |000|XXXXX| ig & trpg, |XXXXXXXX| thread g */
    uint8 access;		/* |P|dpl|0|type| */
    uint16 offset_high;
};

/*
 * Temporary gate descriptor layout used by idt table
 */
typedef struct gate_desc {
    uint32 offset;
    uint16 selector;
    uint8 pad;
    uint8 type;
} gate_desc;

/*
 * Segment descriptor layout for DS, CS, and TSS segments
 */
struct segdesc_s {
    uint16 limit_low;
    uint16 base_low;
    uint8 base_mid;
    uint8 access;               /* |P|dpl|1|X|E|R|A| */
    uint8 gran;			/* |G|X|0|A|limit| */
    uint8 base_high;
};

/*
 * Task state segment layout
 */
struct tss_s {
    uint32 backlink;
    uint32 sp0;
    uint32 ss0;
    uint32 sp1;
    uint32 ss1;
    uint32 sp2;
    uint32 ss2;
    uint32 cr3;
    uint32 ip;
    uint32 flags;
    uint32 ax;
    uint32 cx;
    uint32 dx;
    uint32 bx;
    uint32 sp;
    uint32 bp;
    uint32 si;
    uint32 di;
    uint32 es;
    uint32 cs;
    uint32 ss;
    uint32 ds;
    uint32 fs;
    uint32 gs;
    uint32 ldt;
    uint16 trap;
    uint16 iobase;
};

#endif /* __TYPE_H__ */

