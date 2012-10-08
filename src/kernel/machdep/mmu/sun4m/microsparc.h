/*	@(#)microsparc.h	1.1	96/02/27 14:00:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Defines for Tsunami (Microsparc) cpu
 */


/* ASI map */

#define ASI_MMU_FLPROBE		0x03
#define ASI_MMU_REGS		0x04
#define ASI_MMU_DIAG		0x06
#define ASI_USER_INSTR		0x08
#define ASI_SPVS_INSTR		0x09
#define ASI_USER_DATA		0x0A
#define ASI_SPVS_DATA		0x0B
#define ASI_INST_CACHE_TAG	0x0C
#define ASI_INST_CACHE_DATA	0x0D
#define ASI_DATA_CACHE_TAG	0x0E
#define ASI_DATA_CACHE_DATA	0x0F
#define	ASI_FLUSH_PAGE		0x10
#define	ASI_FLUSH_SEGMENT	0x11
#define	ASI_FLUSH_REGION	0x12
#define	ASI_FLUSH_CONTEXT	0x13
#define	ASI_FLUSH_USER		0x14
#define ASI_MMU_BYPASS		0x20
#define ASI_INST_CACHE_FLCLEAR	0x36
#define ASI_DATA_CACHE_FLCLEAR	0x37
#define ASI_DATA_CACHE_DIAGREG	0x39

/* Address map for MMU registers, at ASI 4 */

#define MMREG_PCR		0x0000	/* Processor Control Register         */
#define MMREG_CTPR		0x0100	/* Context Table Pointer Register     */
#define MMREG_CR		0x0200	/* Context Register		      */
#define MMREG_SFSR		0x0300	/* Synchronous Fault Status Register  */
#define MMREG_SFAR		0x0400	/* Synchronous Fault Address Register */
#define MMREG_TLBRCR		0x1000	/* TLB Replacement Control Register   */
#define MMREG_SFSR_DIAG		0x1300	/* Sync Fault Status Register, DIAG   */
#define MMREG_SFAR_DIAG		0x1400	/* Sync Fault Address Register, DIAG  */

/* Bits in Processor control register */

#define PCR_IMPL_MASK	0xF0000000	/* Implementation, 4 for Tsunami      */
#define PCR_VER_MASK	0x0F000000	/* Version, 1 for Tsunami	      */
#define PCR_STW		0x00800000	/* Software Table Walk enable         */
#define PCR_AV		0x00400000	/* Address View on SBUS lines         */
#define PCR_DV		0x00200000	/* Data View on SBUS lines            */
#define PCR_MV		0x00100000	/* Memory Data View                   */
#define PCR_PC		0x00020000	/* Parity Control                     */
#define PCR_ID		0x00010000	/* ITBR Disable                       */
#define PCR_AC		0x00008000	/* Alternate Cacheability	      */
#define PCR_BM		0x00004000	/* Boot mode                          */
#define PCR_PE		0x00001000	/* Parity Enable                      */
#define PCR_RC_MASK	0x00000C00	/* Refresh Control                    */
#define PCR_RC_128	0x00000000
#define PCR_RC_NONE	0x00000400
#define PCR_RC_512	0x00000800
#define PCR_RC_768	0x00000C00
#define PCR_IE		0x00000200	/* Instruction Cache Enable           */
#define PCR_DE		0x00000100	/* Data Cache Enable                  */
#define PCR_NF		0x00000002	/* No Fault bit                       */
#define PCR_EN		0x00000001	/* MMU Enable                         */

/* Identification codes from MMU control reg. for various CPU types */
#define	MICROSPARC_I		0x41	/* TSUNAMI */
#define	MICROSPARC_II		0x04
