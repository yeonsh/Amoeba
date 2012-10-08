/*	@(#)fault.h	1.2	94/04/06 16:00:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FAULT_H__
#define __FAULT_H__

/*
 * Fault frame related MC680x0 info
 */

/*
 * The trap vectors
 */

#define TRAPV_BUS_ERROR			2
#define TRAPV_ADDRESS_ERROR		3
#define TRAPV_ILL_INSTR			4
#define TRAPV_ZERO_DIVIDE		5
#define TRAPV_CHK			6
#define TRAPV_COND_TRAP			7
#define TRAPV_PRIV_VIOLATION		8
#define TRAPV_TRACE			9
#define TRAPV_LINE_E			10
#define TRAPV_LINE_F			11
#define TRAPV_COPROC_PROT_VIOLATION	13
#define TRAPV_FORMAT_ERROR		14
#define TRAPV_UNINITED_INTERRUPT	15
#define TRAPV_SPURIOUS			24
#define TRAPV_AUTOVEC(n)		(24+(n))
#define TRAPV_TRAP(n)			(32+(n))
#define TRAPV_FPCP_USE_UNORDERED	48
#define TRAPV_FPCP_INEXACT		49
#define TRAPV_FPCP_ZERO_DIVIDE		50
#define TRAPV_FPCP_UNDERFLOW		51
#define TRAPV_FPCP_OPERAND_ERROR	52
#define TRAPV_FPCP_OVERFLOW		53
#define TRAPV_FPCP_SIGNAL_NAN		54
#define TRAPV_MMU_CONFIG_ERROR		56

/*
 * Integer fault frame
 */
#define FUNCODE	0x0007		/* function code */

/* format $2 */

struct format2 {
	long	f2_iaddr;	/* instruction address */
};

/* format $8 */

#define F8_RW	0x0100		/* read/write flag */
#define F8_BY	0x0200		/* byte transfer */
#define F8_HB	0x0400		/* high byte transfer */
#define F8_RM	0x0800		/* read-modify-write cycle */
#define F8_DF	0x1000		/* data fetch */
#define F8_IF	0x2000		/* instruction fetch */
#define F8_RR	0x8000		/* re-run flag */

struct format8 {
	uint16	f8_ssw;		/* special status word */
	long	f8_faddr;	/* fault address */
	uint16	f8_u1;		/* unused/reserved */
	uint16	f8_dob;		/* data output buffer */
	uint16	f8_u2;		/* unused/reserved */
	uint16	f8_dib;		/* data input buffer */
	uint16	f8_u3;		/* unused/reserved */
	uint16	f8_iib;		/* instruction input buffer */
	uint16	f8_ii[16];	/* internal information */
};

/* format $9 */

struct format9 {
	struct format2 f9_f2;	/* same as for format $2 */
	uint16	f9_ir[4];	/* internal registers */
};

/* format $A */

#define FA_SIZ	0x0030	/* size code for data cycle */
#define FA_RW	0x0040	/* read/write for data cycle */
#define FA_RM	0x0080	/* read/modify/write on data cycle */
#define FA_DF	0x0100	/* fault/rerun flag for data cycle */
#define FA_RB	0x1000	/* rerun flag for stage B */
#define FA_RC	0x2000	/* rerun flag for stage C */
#define FA_FB	0x4000	/* fault on stage B of the instruction pipe */
#define FA_FC	0x8000	/* fault on stage C of the instruction pipe */

struct formatA {
	uint16	fA_ir1;		/* internal register */
	uint16 fA_ssw;		/* special status word */
	uint16 fA_ipsC;	/* instruction pipe stage C */
	uint16 fA_ipsB;	/* instruction pipe stage B */
	long	fA_faddr;	/* fault address */
	long	fA_ir2;		/* internal registers */
	long	fA_dob;		/* data output buffer */
	long	fA_ir3;		/* internal registers */
};

/* format $B */

struct formatB {
	struct formatA fB_fA;	/* same as for format $A */
	long	fB_ir4;		/* internal registers */
	long	fB_sbaddr;	/* stage B address */
	long	fB_ir5;		/* internal registers */
	long	fB_dib;		/* data input buffer */
	uint16	fB_ir[22];	/* internal registers */
};

/* format $F (pseudo format for 68000) */

#define FF_IN	0x0008		/* instruction/not flag */
#define FF_RW	0x0010		/* read/write flag */

struct formatF {
	uint16 fF_ssw;		/* special status word */
	long	fF_faddr;	/* fault address */
	uint16	fF_instr;	/* instruction register */
};

/* bits in status register */

#define C_BIT		0x0001	/* carry */
#define V_BIT		0x0002	/* overflow */
#define Z_BIT		0x0004	/* zero */
#define N_BIT		0x0008	/* negative */
#define X_BIT		0x0010	/* extend */
#define PRI_MASK	0x0700	/* interrupt priority mask */
#define M_BIT		0x1000	/* master/interrupt state */
#define S_BIT		0x2000	/* supervisor/user state */
#define T0		0x4000	/* trace enable */
#define T1		0x8000	/* ditto */

#define SUPERVISOR	S_BIT
#define TRACE_BIT	T1

union format {
	struct format2 f_2;
	struct format8 f_8;
	struct format9 f_9;
	struct formatA f_A;
	struct formatB f_B;
	struct formatF f_F;
};

#define Format(f)	(((f)->f_offset >> 12) & 0xF)
#define Offset(f)	((f)->f_offset & 0xFFF)

struct fault {
	long	f_regs[16];
	uint16	f_sr;
	uint16	f_pc_high;	/* This field and the next actually one long */
	uint16	f_pc_low;	/* but that breaks alignment */
	uint16	f_offset;
	union format f_format;
};

#define REG_D0	0
#define REG_D1	1
#define REG_D2	2
#define REG_D3	3
#define REG_D4	4
#define REG_D5	5
#define REG_D6	6
#define REG_D7	7
#define REG_A0	8
#define REG_A1	9
#define REG_A2	10
#define REG_A3	11
#define REG_A4	12
#define REG_A5	13
#define REG_A6	14
#define REG_USP	15

#define F_PC(ffp)	((((long) (ffp)->f_pc_high)<<16)|(ffp)->f_pc_low)

#define f_f2 f_format.f_2
#define f_f8 f_format.f_8
#define f_f9 f_format.f_9
#define f_fA f_format.f_A
#define f_fB f_format.f_B
#define f_fF f_format.f_F

extern unsigned mc68000_fsize[];

#define FAULT_SIZE(f)	(64+mc68000_fsize[Format(f)])

/*
 * MC6888[12] fault frames
 */

struct fl_format_idle {
	uint16	fl_f_idle_coco;		/* Command/condition register */
	uint16	fl_f_idle_reserved;
	uint32	fl_f_idle_excop;	/* Exceptional operand */
	uint32	fl_f_idle_opreg;	/* Operand register */
	uint32	fl_f_idle_biuflags;	/* BIU flags */
};

struct fl_format_busy {
	uint32	fl_f_busy_internalregs[45];	/* Don't look */
};

struct fl_mc68882_extra {
	uint32	fl_m2_extra[8];		/* 32 bytes extra */
};

struct flfault {		/* Actually definition of worst case */
	uint8	flf_flag;		/* FP status present flag */
	uint8	flf_unused1;
	uint16	flf_unused2;
	uint32	flf_creg[3];		/* Control registers */
	uint32	flf_dreg[8][3];		/* 8 96 bit data registers */
	uint8	flf_version;		/* Probably type of chip */
	uint8	flf_size;		/* # of bytes size after boundary */
	uint16	flf_reserved;
	/* boundary */
	union {
		struct fl_format_idle	flfu_idle1;
		struct fl_format_busy	flfu_busy1;
		struct {
			struct	fl_mc68882_extra flfu2_extra1;
			struct	fl_format_idle	flfu2_idle;
		} flfu_idle2;
		struct {
			struct	fl_mc68882_extra flfu2_extra2;
			struct	fl_format_busy	flfu2_busy;
		} flfu_busy2;
	} flf_u;
};

#define FLFAULT_SIZE(f)	((f)->flf_flag == 0 ? sizeof(uint32) : \
			116+(f)->flf_size)

/*
 * All the macros to get around the variableness of the fault frame
 */

typedef struct ustate {
	struct flfault	m68us_flfault;
	struct fault	m68us_fault;
} thread_ustate;

#define USTATE_FAULT_FRAME(us) ((struct fault *) \
		(((char *) (us))+FLFAULT_SIZE(&(us)->m68us_flfault)) \
				)

/*
 * These are actually used by machine independent code
 */
#define USTATE_SIZE_MIN	(sizeof(long)+64+4*sizeof(uint16))
#define USTATE_SIZE_MAX (sizeof(struct ustate))
#define USTATE_SIZE(us) (FLFAULT_SIZE(&(us)->m68us_flfault) + \
			 FAULT_SIZE(USTATE_FAULT_FRAME(us)))

#define ARCHITECTURE "mc68000"

#endif /* __FAULT_H__ */
