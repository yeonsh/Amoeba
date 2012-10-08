/*	@(#)acv.h	1.2	94/04/07 14:29:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * All there is to know about an a.out file to turn it into an
 * Amoeba process.
 */

struct aoutinfo {
    char *ai_architecture;	/* Architecture for binary */

    long  ai_txt_offset;	/* offset in file where text starts */
    long  ai_txt_virtual;	/* address of text in process */
    long  ai_txt_size;		/* length of text */

    long  ai_rom_offset;	/* Ditto for RO data */
    long  ai_rom_virtual;
    long  ai_rom_size;

    long  ai_dat_offset;	/* Ditto for RW data */
    long  ai_dat_virtual;
    long  ai_dat_size;

    long  ai_bss_virtual;	/* Ditto BSS, but no offset */
    long  ai_bss_size;

    char *ai_sym_tab;		/* buffer containing symbol table */
    long  ai_sym_size;		/* length of it */
    char *ai_str_tab;		/* buffer containing string table */
    long  ai_str_size;		/* length of it */

    long  ai_entrypoint;	/* Where to start running */
};
