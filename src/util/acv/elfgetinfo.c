/*	@(#)elfgetinfo.c	1.1	96/02/27 12:41:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NO_ELF_AOUT

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "acv.h"
#include <elf.h>
#include "namelist.h"

int
iselfaout(fd)
int fd;
{
    Elf32_Ehdr elfhdr;

    /* read and check a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	fprintf(stderr, "ELF: lseek failed");
	return 0;
    }

    if (read(fd, (char *) &elfhdr, sizeof(elfhdr)) != sizeof(elfhdr)) {
	fprintf(stderr, "ELF: cannot read header\n");
	return 0;
    }

    if (elfhdr.e_ident[EI_MAG0] != ELFMAG0 ||
	elfhdr.e_ident[EI_MAG1] != ELFMAG1 ||
	elfhdr.e_ident[EI_MAG2] != ELFMAG2 ||
	elfhdr.e_ident[EI_MAG3] != ELFMAG3)
    {
	return 0;
    }

    return 1;
}

static int
elf_getsection(fd, elfhdr, name, foundhdr, foundindx)
int	    fd;
Elf32_Ehdr *elfhdr;
char       *name;
Elf32_Shdr *foundhdr;
int	   *foundindx;
{
    /* Find the section specified */
    Elf32_Shdr strhdr;
    long off, stringoff;
    unsigned int secno;

    off = elfhdr->e_shoff + elfhdr->e_shstrndx * elfhdr->e_shentsize;
    if (lseek(fd, off, 0) != off || 
	read(fd, (char *) &strhdr, sizeof(strhdr)) != sizeof(strhdr))
    {
	fprintf(stderr, "ELF: cannot read strtab secthdr at 0x%lx\n", off);
	return 0;
    }
    stringoff = strhdr.sh_offset;
    
    for (secno = 0; secno < elfhdr->e_shnum; secno++) {
	char secname[80];
	Elf32_Shdr sechdr;
	
	off = elfhdr->e_shoff + secno * elfhdr->e_shentsize;
	if (lseek(fd, off, 0) != off) {
	    fprintf(stderr, "ELF: seek to section %d (0x%lx) failed\n",
		    secno, off);
	    return 0;
	}
	if (read(fd, (char *) &sechdr, sizeof(sechdr)) != sizeof(sechdr)) {
	    fprintf(stderr, "ELF: short read for section header %d\n",
		    secno);
	    return 0;
	}

	secname[sizeof(secname) - 1] = 0;
	off = stringoff + sechdr.sh_name;
	if (lseek(fd, off, 0) != off ||
	    read(fd, secname, sizeof(secname) - 1) <= 0)
	{
	    fprintf(stderr, "ELF: cannot get name of section %d (off 0x%lx)\n",
		    secno, off);
	    return 0;
	}

#ifdef DEBUG
	printf("section %s:\n", secname);
	printf("type:      %6lxX\n", sechdr.sh_type);
	printf("flags:     %6lxX\n", sechdr.sh_flags);
	printf("address:   %6lxX\n", sechdr.sh_addr);
	printf("fileoff:   %6ld\n",  sechdr.sh_offset);
	printf("size:      %6ld\n",  sechdr.sh_size);
	printf("link:      %6lxX\n", sechdr.sh_link);
	printf("info:      %6lxX\n", sechdr.sh_info);
	printf("align:     %6ld\n",  sechdr.sh_addralign);
	printf("entrysiz:  %6ld\n",  sechdr.sh_entsize);
#endif
	if (strcmp(secname, name) == 0) {
	    *foundhdr = sechdr;
	    *foundindx = secno;
	    return 1;
	}
    }

    fprintf(stderr, "ELF: could not find section %s\n", name);
    return 0;
}

static int
elf_convertsymtab(fd, aip, symtabhdr, strtabhdr, bssindx)
int fd;
struct aoutinfo *aip;
Elf32_Shdr *symtabhdr;
Elf32_Shdr *strtabhdr;
int bssindx;
{
    /* First read the string table */
    int nbytes, nsyms;
    Elf32_Sym *elfsyms;
    struct nlist *am_nlist;
    int i;

    aip->ai_str_tab = (char *) malloc(strtabhdr->sh_size + sizeof(long));
    if (aip->ai_str_tab == 0) {
	fprintf(stderr, "ELF: cannot allocate string table\n");
	return 0;
    }
    aip->ai_str_size = strtabhdr->sh_size;
    if (lseek(fd, strtabhdr->sh_offset, 0) != strtabhdr->sh_offset) {
	fprintf(stderr, "ELF: cannot seek to string table (0x%lx)\n",
		strtabhdr->sh_offset);
	return 0;
    }
    if ((nbytes = read(fd, aip->ai_str_tab + sizeof(long),
		       aip->ai_str_size)) != aip->ai_str_size)
    {
	fprintf(stderr, "ELF: read %ld instead of %ld bytes of string table\n",
		nbytes, aip->ai_str_size);
	return 0;
    }
    /* set the byte count */
    *(long *)aip->ai_str_tab = aip->ai_str_size;
	
    /* Read the symbol table. */
    elfsyms = (Elf32_Sym *) malloc(symtabhdr->sh_size);
    if (elfsyms == 0) {
	fprintf(stderr, "ELF: cannot allocate ELF symbol table (%d bytes)\n",
	       symtabhdr->sh_size);
	return 0;
    }
    if (lseek(fd, symtabhdr->sh_offset, 0) != symtabhdr->sh_offset) {
	fprintf(stderr, "ELF: cannot seek to symbol table (0x%lx)\n",
		symtabhdr->sh_offset);
	return 0;
    }
    if ((nbytes = read(fd, elfsyms, symtabhdr->sh_size)) != symtabhdr->sh_size)
    {
	fprintf(stderr, "ELF: read %ld instead of %ld bytes of symbol table\n",
	       nbytes, symtabhdr->sh_size);
	return 0;
    }
    
    /* Convert ELF symbol table to Amoeba (a.out) format */
    if (symtabhdr->sh_entsize != sizeof(Elf32_Sym)) {
	fprintf(stderr, "ELF: unexpected Elf symbol size (%d != %d)\n",
		symtabhdr->sh_entsize, sizeof(Elf32_Sym));
	return 0;
    }
    nsyms = symtabhdr->sh_size / symtabhdr->sh_entsize;
    am_nlist = (struct nlist *) calloc(nsyms, sizeof(struct nlist));
    if (am_nlist == 0) {
	fprintf(stderr, "ELF: cannot allocate symbol table\n");
	return 0;
    }
    for (i = 0; i < nsyms; i++) {
	Elf32_Sym *elfsym = &elfsyms[i];
	int type, external;
	
#ifdef FULL_DEBUG
	printf("%d: ", i);
	printf("name %5d ", elfsym->st_name);
	if (elfsym->st_name > 0 && elfsym->st_name < strtabhdr->sh_size) {
	    printf("(%12s) ", aip->ai_str_tab + 4 + elfsym->st_name);
	}
	printf("value %5lxX ", elfsym->st_value);
	printf("size %4d ", elfsym->st_size);
	printf("info %2d ", elfsym->st_info & 0xFF);
	printf("other %2d ", elfsym->st_other & 0xFF);
	printf("hindex %2d\n", elfsym->st_shndx & 0xFFFF);
#endif

	switch (ELF32_ST_TYPE(elfsym->st_info)) {
	case STT_OBJECT:
	    if (elfsym->st_shndx == bssindx) {
		type = N_BSS;
	    } else {
		type = N_DATA;
	    }
	    break;
	case STT_FUNC:
	    type = N_TEXT;
	    break;
	case STT_FILE:
	    type = N_FN;
	    break;
	default:
	    if (elfsym->st_shndx == 1) {
		/* probably assembly code without explicit type indication */
		type = N_TEXT;
	    } else {
		type = N_UNDF;
	    }
	    break;
	}
	switch (ELF32_ST_BIND(elfsym->st_info)) {
	case STB_GLOBAL:
	case STB_WEAK:
	    external = 1;
	    break;
	case STB_LOCAL:
	default:
	    external = 0;
	    break;
	}

	am_nlist[i].n_un.n_strx = elfsym->st_name + 4; /* we inserted a long */
	am_nlist[i].n_type = type | external;
	am_nlist[i].n_other = 0; /* ? */
	am_nlist[i].n_desc = 0; /* ? */
	am_nlist[i].n_value = elfsym->st_value;
    }

    aip->ai_sym_tab = (char *) am_nlist;
    aip->ai_sym_size = nsyms * sizeof(struct nlist);
    return 1;
}

char *
elfgetinfo(fd, aip)
int fd;
struct aoutinfo *aip;
{
    Elf32_Ehdr elfhdr;
    Elf32_Shdr strtabhdr, symtabhdr, bsshdr;
    int strtabindx, symtabindx, bssindx;
    Elf32_Phdr prghdr;
    unsigned int secno;
    unsigned long off, stringoff;
    int i;

    /* read and check a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	return "ELF: lseek failed";
    }

    if (read(fd, (char *) &elfhdr, sizeof(elfhdr)) != sizeof(elfhdr)) {
	return "ELF: short read for ELF header";
    }

    if (elfhdr.e_ident[EI_MAG0] != ELFMAG0 ||
	elfhdr.e_ident[EI_MAG1] != ELFMAG1 ||
	elfhdr.e_ident[EI_MAG2] != ELFMAG2 ||
	elfhdr.e_ident[EI_MAG3] != ELFMAG3)
    {
	return "not an ELF binary";
    }

    /* TODO: byteswaps on header entries if needed */

#ifdef DEBUG
    printf("file type:      0x%x\n",  elfhdr.e_type);
    printf("target machine: 0x%x\n",  elfhdr.e_machine);
    printf("file version:   0x%lx\n", elfhdr.e_version);
    printf("start address:  0x%lx\n", elfhdr.e_entry);
    printf("phdr off:         %ld\n", elfhdr.e_phoff);
    printf("shdr off:         %ld\n", elfhdr.e_shoff);
    printf("file flags:     0x%lx\n", elfhdr.e_flags);
    printf("ehdr size:        %d\n",  elfhdr.e_ehsize);
    printf("phdr size:        %d\n",  elfhdr.e_phentsize);
    printf("phdr num:         %ld\n", elfhdr.e_phnum);
    printf("shdr size:        %ld\n", elfhdr.e_shentsize);
    printf("shdr num:         %d\n",  elfhdr.e_shnum);
    printf("shdr stringind:   %d\n",  elfhdr.e_shstrndx);
#endif

    /* Process the program header table: */
    if (lseek(fd, elfhdr.e_phoff, 0) != elfhdr.e_phoff) {
	fprintf(stderr, "ELF: lseek to program header (0x%lx) failed\n",
		elfhdr.e_phoff);
	return "seek error";
    }

    if (elfhdr.e_type != ET_EXEC) {
	return "ELF binary: not executable";
    }

    switch (elfhdr.e_machine) {
    case EM_SPARC:
	aip->ai_architecture = "sparc";
	break;
    case EM_386:
	aip->ai_architecture = "i80386";
	break;
    case EM_68K:
	aip->ai_architecture = "mc68000";
	break;
    default:
	fprintf(stderr, "ELF: unknown machine type 0x%lx\n", elfhdr.e_machine);
	return "ELF: unsupported architecture";
    }

    for (secno = 0; secno < elfhdr.e_phnum; secno++) {
	if (read(fd, (char *) &prghdr, sizeof(prghdr)) != sizeof(prghdr)) {
	    fprintf(stderr, "ELF: short read for program header %d\n", secno);
	    return "ELF read error";
	}

#ifdef DEBUG
	printf("\nprogram section %d:\n", secno);
	printf("type:   %6lxX\n", prghdr.p_type);
	printf("off:    %6ld\n",  prghdr.p_offset);
	printf("vaddr:  %6lxX\n", prghdr.p_vaddr);
	printf("paddr:  %6lxX\n", prghdr.p_paddr);
	printf("filesiz:%6ld\n",  prghdr.p_filesz);
	printf("memsiz: %6ld\n",  prghdr.p_memsz);
	printf("flags:  %6lxX\n", prghdr.p_flags);
	printf("align:  %6ld\n",  prghdr.p_align);
#endif

	if (prghdr.p_type != PT_LOAD) {
	    /* We only deal with ELF binary statically linked with the
	     * Amoeba libaries.  Maybe the user is accidentally trying
	     * to convert a Unix binary?
	     */
	    fprintf(stderr,"ELF: wrong section type %d; not an Amoeba binary\n",
		    prghdr.p_type);
	    return "ELF binary format error";
	}

	switch (prghdr.p_flags) {
	case (PF_R|PF_X):
	    /* text */
	    if (aip->ai_txt_size != 0) {
		return "ELF: multiple text segments";
	    }
	    aip->ai_txt_offset = prghdr.p_offset;
	    aip->ai_txt_virtual = prghdr.p_vaddr;
	    aip->ai_txt_size = prghdr.p_filesz;
	    break;
	case (PF_R|PF_W|PF_X):
	    /* data or bss */
	    if (prghdr.p_filesz == 0) {
		/* bss */
	        if (aip->ai_bss_size != 0) {
		   return "ELF: multiple bss segments";
	        }
		aip->ai_bss_virtual = prghdr.p_vaddr;
		aip->ai_bss_size = prghdr.p_memsz;
	    } else {
		if (prghdr.p_filesz < prghdr.p_memsz) {
		    /* data and bss combined */
		    if (aip->ai_dat_size != 0 || aip->ai_bss_size != 0) {
			return "ELF: multiple data/bss segments";
		    }
		    aip->ai_dat_offset = prghdr.p_offset;
		    aip->ai_dat_virtual = prghdr.p_vaddr;
		    aip->ai_dat_size = prghdr.p_filesz;
		    /* remainder is bss */
		    aip->ai_bss_virtual = prghdr.p_vaddr + prghdr.p_filesz;
		    aip->ai_bss_size = prghdr.p_memsz - prghdr.p_filesz;
		} else {
		    /* just data */
		    if (aip->ai_dat_size != 0) {
			return "ELF: multiple data segments";
		    }
		    aip->ai_dat_offset = prghdr.p_offset;
		    aip->ai_dat_virtual = prghdr.p_vaddr;
		    aip->ai_dat_size = prghdr.p_filesz;
		}
	    }
	    break;
	default:
	    fprintf(stderr, "ELF: unsupported segment type with flags 0x%lx\n",
		    prghdr.p_flags);
	    return "ELF: unsupported segment type";
	}
    }

    aip->ai_entrypoint = elfhdr.e_entry;

    /* sanity check to make sure all segments were present: */
    if (aip->ai_txt_size == 0) {
	return "ELF: missing text segment";
    }
    if (aip->ai_dat_size == 0) {
	return "ELF: missing data segment";
    }
    if (aip->ai_bss_size == 0) {
	return "ELF: missing bss segment";
    }

    if (!elf_getsection(fd, &elfhdr, ".strtab", &strtabhdr, &strtabindx) ||
	!elf_getsection(fd, &elfhdr, ".symtab", &symtabhdr, &symtabindx))
    {
	/* no symbol table */
	aip->ai_sym_tab = 0;
	aip->ai_sym_size = 0;
	aip->ai_str_tab = 0;
	aip->ai_str_size = 0;
    }
    else
    {
	if (!elf_getsection(fd, &elfhdr, ".bss", &bsshdr, &bssindx)) {
	    return "ELF: cannot find bss section";
	}
	if (!elf_convertsymtab(fd, aip, &symtabhdr, &strtabhdr, bssindx)) {
	    return "ELF: cannot convert symbol table";
	}
    }

    /* OK, succesful conversion */
    return 0;
}

#else
extern int main(); /* avoid 'empty translation unit' */
#endif /* NO_ELF_AOUT */
