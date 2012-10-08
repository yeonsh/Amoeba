/*	@(#)tables.c	1.7	94/04/06 08:56:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * tables.c
 */
#include <amoeba.h>
#include <i80386.h>
#include <trap.h>
#include <type.h>

/*
 * i80386 processor traps
 */
extern int aligntrap();
extern int div0trap();
extern int dbgtrap();
extern int brktrap();
extern int nmiint();
extern int ovflotrap();
extern int boundstrap();
extern int invoptrap();
extern int ndptrap();
extern int invaltrap();
extern int invtsstrap();
extern int segnptrap();
extern int stktrap();
extern int gptrap();
extern int pftrap();
extern int ndperr();
extern int dblfault();
extern int overrun();

/*
 * Hardware interrupts
 */
extern int irq_0_vect();
extern int irq_1_vect();
extern int irq_2_vect();
extern int irq_3_vect();
extern int irq_4_vect();
extern int irq_5_vect();
extern int irq_6_vect();
extern int irq_7_vect();
extern int irq_8_vect();
extern int irq_9_vect();
extern int irq_10_vect();
extern int irq_11_vect();
extern int irq_12_vect();
extern int irq_13_vect();
extern int irq_14_vect();
extern int irq_15_vect();

/*
 * System calls
 */
extern int ugetreq();
extern int uputrep();
extern int utrans();
extern int usuicide();
extern int utimeout();
extern int ucleanup();
extern int uawait();
extern int unewsys();
extern int ubadcall();
extern int urpc_trans();
extern int urpc_getreq();
extern int urpc_putrep();

extern void nullvect();

/*
 * Interrupt request level table
 */
int nintr = NIRQ;
void (*ivect[NIRQ])() = {
    nullvect,			/* IRQ 0 */
    nullvect,			/* IRQ 1 */
    nullvect,			/* IRQ 2 */
    nullvect,			/* IRQ 3 */
    nullvect,			/* IRQ 4 */
    nullvect,			/* IRQ 5 */
    nullvect,			/* IRQ 6 */
    nullvect,			/* IRQ 7 */
    nullvect,			/* IRQ 8 */
    nullvect,			/* IRQ 9 */
    nullvect,			/* IRQ 10 */
    nullvect,			/* IRQ 11 */
    nullvect,			/* IRQ 12 */
    nullvect,			/* IRQ 13 */
    nullvect,			/* IRQ 14 */
    nullvect,			/* IRQ 15 */
};

/*
 * Interrupt gate descriptors
 *
 * NOTE: this table is temporary, it is overwritten by the
 * proper IDT table in kstart/as.S.
 */
gate_desc idt[] = {
    /* processor traps */
    MKKTRPG(div0trap),		/* 000 */
    MKKTRPG(dbgtrap), 		/* 001 */
    MKINTG(nmiint),		/* 002 */
    MKUTRPG(brktrap), 		/* 003 */
    MKUTRPG(ovflotrap),		/* 004 */
    MKKTRPG(boundstrap),	/* 005 */
    MKKTRPG(invoptrap),		/* 006 */
    MKKTRPG(ndptrap),		/* 007 */
    MKKTRPG(dblfault),		/* 008 */
    MKKTRPG(overrun),		/* 009 */
    MKKTRPG(invtsstrap),	/* 010 */
    MKKTRPG(segnptrap),		/* 011 */
    MKKTRPG(stktrap),		/* 012 */
    MKKTRPG(gptrap),		/* 013 */
    MKKTRPG(pftrap),		/* 014 */
    MKKTRPG(invaltrap),		/* 015 */
    MKKTRPG(ndperr),		/* 016 */
    MKKTRPG(aligntrap),		/* 017 */
    MKKTRPG(invaltrap),		/* 018 */
    MKKTRPG(invaltrap),		/* 019 */
    MKKTRPG(invaltrap),		/* 020 */
    MKKTRPG(invaltrap),		/* 021 */
    MKKTRPG(invaltrap),		/* 022 */
    MKKTRPG(invaltrap),		/* 023 */
    MKKTRPG(invaltrap),		/* 024 */
    MKKTRPG(invaltrap),		/* 025 */
    MKKTRPG(invaltrap),		/* 026 */
    MKKTRPG(invaltrap),		/* 027 */
    MKKTRPG(invaltrap),		/* 028 */
    MKKTRPG(invaltrap),		/* 029 */
    MKKTRPG(invaltrap),		/* 030 */
    MKKTRPG(invaltrap),		/* 031 */

    /* hardware interrupts */
    MKINTG(irq_0_vect),		/* 032 */
    MKINTG(irq_1_vect),		/* 033 */
    MKINTG(irq_2_vect),		/* 034 */
    MKINTG(irq_3_vect),		/* 035 */
    MKINTG(irq_4_vect),		/* 036 */
    MKINTG(irq_5_vect),		/* 037 */
    MKINTG(irq_6_vect),		/* 038 */
    MKINTG(irq_7_vect),		/* 039 */
    MKINTG(irq_8_vect),		/* 040 */
    MKINTG(irq_9_vect),		/* 041 */
    MKINTG(irq_10_vect),	/* 042 */
    MKINTG(irq_11_vect),	/* 043 */
    MKINTG(irq_12_vect),	/* 044 */
    MKINTG(irq_13_vect),	/* 045 */
    MKINTG(irq_14_vect),	/* 046 */
    MKINTG(irq_15_vect),	/* 047 */

    MKKTRPG(invaltrap),		/* 048 */
    MKKTRPG(invaltrap),		/* 049 */

    /* system calls */
#ifndef NOPROC
    MKUTRPG(ugetreq),		/* 050 */
    MKUTRPG(uputrep),		/* 051 */
    MKUTRPG(utrans),		/* 052 */
    MKUTRPG(usuicide),		/* 053 */
    MKUTRPG(utimeout),		/* 054 */
    MKUTRPG(ucleanup),		/* 055 */
    MKUTRPG(uawait),		/* 056 */
    MKUTRPG(unewsys),		/* 057 */
    MKUTRPG(urpc_getreq),	/* 058 */
    MKUTRPG(urpc_putrep),	/* 059 */
    MKUTRPG(urpc_trans),	/* 060 */
    MKUTRPG(ubadcall),		/* 061 */
    MKUTRPG(ubadcall),		/* 062 */
    MKUTRPG(ubadcall),		/* 063 */
#else
    MKUTRPG(invaltrap),		/* 050 */
    MKUTRPG(invaltrap),		/* 051 */
    MKUTRPG(invaltrap),		/* 052 */
    MKUTRPG(invaltrap),		/* 053 */
    MKUTRPG(invaltrap),		/* 054 */
    MKUTRPG(invaltrap),		/* 055 */
    MKUTRPG(invaltrap),		/* 056 */
    MKUTRPG(invaltrap),		/* 057 */
    MKUTRPG(invaltrap),		/* 058 */
    MKUTRPG(invaltrap),		/* 059 */
    MKUTRPG(invaltrap),		/* 060 */
    MKUTRPG(invaltrap),		/* 061 */
    MKUTRPG(invaltrap),		/* 062 */
    MKUTRPG(invaltrap),		/* 063 */
#endif /* NOPROC */
    MKKTRPG(invaltrap),		/* 064 */
    MKKTRPG(invaltrap),		/* 065 */
    MKKTRPG(invaltrap),		/* 066 */
    MKKTRPG(invaltrap),		/* 067 */
    MKKTRPG(invaltrap),		/* 068 */
    MKKTRPG(invaltrap),		/* 069 */
    MKKTRPG(invaltrap),		/* 070 */
    MKKTRPG(invaltrap),		/* 071 */
    MKKTRPG(invaltrap),		/* 072 */
    MKKTRPG(invaltrap),		/* 073 */
    MKKTRPG(invaltrap),		/* 074 */
    MKKTRPG(invaltrap),		/* 075 */
    MKKTRPG(invaltrap),		/* 076 */
    MKKTRPG(invaltrap),		/* 077 */
    MKKTRPG(invaltrap),		/* 078 */
    MKKTRPG(invaltrap),		/* 079 */
    MKKTRPG(invaltrap),		/* 080 */
    MKKTRPG(invaltrap),		/* 081 */
    MKKTRPG(invaltrap),		/* 082 */
    MKKTRPG(invaltrap),		/* 083 */
    MKKTRPG(invaltrap),		/* 084 */
    MKKTRPG(invaltrap),		/* 085 */
    MKKTRPG(invaltrap),		/* 086 */
    MKKTRPG(invaltrap),		/* 087 */
    MKKTRPG(invaltrap),		/* 088 */
    MKKTRPG(invaltrap),		/* 089 */
    MKKTRPG(invaltrap),		/* 090 */
    MKKTRPG(invaltrap),		/* 091 */
    MKKTRPG(invaltrap),		/* 092 */
    MKKTRPG(invaltrap),		/* 093 */
    MKKTRPG(invaltrap),		/* 094 */
    MKKTRPG(invaltrap),		/* 095 */
    MKKTRPG(invaltrap),		/* 096 */
    MKKTRPG(invaltrap),		/* 097 */
    MKKTRPG(invaltrap),		/* 098 */
    MKKTRPG(invaltrap),		/* 099 */
    MKKTRPG(invaltrap),		/* 100 */
    MKKTRPG(invaltrap),		/* 101 */
    MKKTRPG(invaltrap),		/* 102 */
    MKKTRPG(invaltrap),		/* 103 */
    MKKTRPG(invaltrap),		/* 104 */
    MKKTRPG(invaltrap),		/* 105 */
    MKKTRPG(invaltrap),		/* 106 */
    MKKTRPG(invaltrap),		/* 107 */
    MKKTRPG(invaltrap),		/* 108 */
    MKKTRPG(invaltrap),		/* 109 */
    MKKTRPG(invaltrap),		/* 110 */
    MKKTRPG(invaltrap),		/* 111 */
    MKKTRPG(invaltrap),		/* 112 */
    MKKTRPG(invaltrap),		/* 113 */
    MKKTRPG(invaltrap),		/* 114 */
    MKKTRPG(invaltrap),		/* 115 */
    MKKTRPG(invaltrap),		/* 116 */
    MKKTRPG(invaltrap),		/* 117 */
    MKKTRPG(invaltrap),		/* 118 */
    MKKTRPG(invaltrap),		/* 119 */
    MKKTRPG(invaltrap),		/* 120 */
    MKKTRPG(invaltrap),		/* 121 */
    MKKTRPG(invaltrap),		/* 122 */
    MKKTRPG(invaltrap),		/* 123 */
    MKKTRPG(invaltrap),		/* 124 */
    MKKTRPG(invaltrap),		/* 125 */
    MKKTRPG(invaltrap),		/* 126 */
    MKKTRPG(invaltrap),		/* 127 */
    MKKTRPG(invaltrap),		/* 128 */
    MKKTRPG(invaltrap),		/* 129 */
    MKKTRPG(invaltrap),		/* 130 */
    MKKTRPG(invaltrap),		/* 131 */
    MKKTRPG(invaltrap),		/* 132 */
    MKKTRPG(invaltrap),		/* 133 */
    MKKTRPG(invaltrap),		/* 134 */
    MKKTRPG(invaltrap),		/* 135 */
    MKKTRPG(invaltrap),		/* 136 */
    MKKTRPG(invaltrap),		/* 137 */
    MKKTRPG(invaltrap),		/* 138 */
    MKKTRPG(invaltrap),		/* 139 */
    MKKTRPG(invaltrap),		/* 140 */
    MKKTRPG(invaltrap),		/* 141 */
    MKKTRPG(invaltrap),		/* 142 */
    MKKTRPG(invaltrap),		/* 143 */
    MKKTRPG(invaltrap),		/* 144 */
    MKKTRPG(invaltrap),		/* 145 */
    MKKTRPG(invaltrap),		/* 146 */
    MKKTRPG(invaltrap),		/* 147 */
    MKKTRPG(invaltrap),		/* 148 */
    MKKTRPG(invaltrap),		/* 149 */
    MKKTRPG(invaltrap),		/* 150 */
    MKKTRPG(invaltrap),		/* 151 */
    MKKTRPG(invaltrap),		/* 152 */
    MKKTRPG(invaltrap),		/* 153 */
    MKKTRPG(invaltrap),		/* 154 */
    MKKTRPG(invaltrap),		/* 155 */
    MKKTRPG(invaltrap),		/* 156 */
    MKKTRPG(invaltrap),		/* 157 */
    MKKTRPG(invaltrap),		/* 158 */
    MKKTRPG(invaltrap),		/* 159 */
    MKKTRPG(invaltrap),		/* 160 */
    MKKTRPG(invaltrap),		/* 161 */
    MKKTRPG(invaltrap),		/* 162 */
    MKKTRPG(invaltrap),		/* 163 */
    MKKTRPG(invaltrap),		/* 164 */
    MKKTRPG(invaltrap),		/* 165 */
    MKKTRPG(invaltrap),		/* 166 */
    MKKTRPG(invaltrap),		/* 167 */
    MKKTRPG(invaltrap),		/* 168 */
    MKKTRPG(invaltrap),		/* 169 */
    MKKTRPG(invaltrap),		/* 170 */
    MKKTRPG(invaltrap),		/* 171 */
    MKKTRPG(invaltrap),		/* 172 */
    MKKTRPG(invaltrap),		/* 173 */
    MKKTRPG(invaltrap),		/* 174 */
    MKKTRPG(invaltrap),		/* 175 */
    MKKTRPG(invaltrap),		/* 176 */
    MKKTRPG(invaltrap),		/* 177 */
    MKKTRPG(invaltrap),		/* 178 */
    MKKTRPG(invaltrap),		/* 179 */
    MKKTRPG(invaltrap),		/* 180 */
    MKKTRPG(invaltrap),		/* 181 */
    MKKTRPG(invaltrap),		/* 182 */
    MKKTRPG(invaltrap),		/* 183 */
    MKKTRPG(invaltrap),		/* 184 */
    MKKTRPG(invaltrap),		/* 185 */
    MKKTRPG(invaltrap),		/* 186 */
    MKKTRPG(invaltrap),		/* 187 */
    MKKTRPG(invaltrap),		/* 188 */
    MKKTRPG(invaltrap),		/* 189 */
    MKKTRPG(invaltrap),		/* 190 */
    MKKTRPG(invaltrap),		/* 191 */
    MKKTRPG(invaltrap),		/* 192 */
    MKKTRPG(invaltrap),		/* 193 */
    MKKTRPG(invaltrap),		/* 194 */
    MKKTRPG(invaltrap),		/* 195 */
    MKKTRPG(invaltrap),		/* 196 */
    MKKTRPG(invaltrap),		/* 197 */
    MKKTRPG(invaltrap),		/* 198 */
    MKKTRPG(invaltrap),		/* 199 */
    MKKTRPG(invaltrap),		/* 200 */
    MKKTRPG(invaltrap),		/* 201 */
    MKKTRPG(invaltrap),		/* 202 */
    MKKTRPG(invaltrap),		/* 203 */
    MKKTRPG(invaltrap),		/* 204 */
    MKKTRPG(invaltrap),		/* 205 */
    MKKTRPG(invaltrap),		/* 206 */
    MKKTRPG(invaltrap),		/* 207 */
    MKKTRPG(invaltrap),		/* 208 */
    MKKTRPG(invaltrap),		/* 209 */
    MKKTRPG(invaltrap),		/* 210 */
    MKKTRPG(invaltrap),		/* 211 */
    MKKTRPG(invaltrap),		/* 212 */
    MKKTRPG(invaltrap),		/* 213 */
    MKKTRPG(invaltrap),		/* 214 */
    MKKTRPG(invaltrap),		/* 215 */
    MKKTRPG(invaltrap),		/* 216 */
    MKKTRPG(invaltrap),		/* 217 */
    MKKTRPG(invaltrap),		/* 218 */
    MKKTRPG(invaltrap),		/* 219 */
    MKKTRPG(invaltrap),		/* 220 */
    MKKTRPG(invaltrap),		/* 221 */
    MKKTRPG(invaltrap),		/* 222 */
    MKKTRPG(invaltrap),		/* 223 */
    MKKTRPG(invaltrap),		/* 224 */
    MKKTRPG(invaltrap),		/* 225 */
    MKKTRPG(invaltrap),		/* 226 */
    MKKTRPG(invaltrap),		/* 227 */
    MKKTRPG(invaltrap),		/* 228 */
    MKKTRPG(invaltrap),		/* 229 */
    MKKTRPG(invaltrap),		/* 230 */
    MKKTRPG(invaltrap),		/* 231 */
    MKKTRPG(invaltrap),		/* 232 */
    MKKTRPG(invaltrap),		/* 233 */
    MKKTRPG(invaltrap),		/* 234 */
    MKKTRPG(invaltrap),		/* 235 */
    MKKTRPG(invaltrap),		/* 236 */
    MKKTRPG(invaltrap),		/* 237 */
    MKKTRPG(invaltrap),		/* 238 */
    MKKTRPG(invaltrap),		/* 239 */
    MKKTRPG(invaltrap),		/* 240 */
    MKKTRPG(invaltrap),		/* 241 */
    MKKTRPG(invaltrap),		/* 242 */
    MKKTRPG(invaltrap),		/* 243 */
    MKKTRPG(invaltrap),		/* 244 */
    MKKTRPG(invaltrap),		/* 245 */
    MKKTRPG(invaltrap),		/* 246 */
    MKKTRPG(invaltrap),		/* 247 */
    MKKTRPG(invaltrap),		/* 248 */
    MKKTRPG(invaltrap),		/* 249 */
    MKKTRPG(invaltrap),		/* 250 */
    MKKTRPG(invaltrap),		/* 251 */
    MKKTRPG(invaltrap),		/* 252 */
    MKKTRPG(invaltrap),		/* 253 */
    MKKTRPG(invaltrap),		/* 254 */
    MKKTRPG(invaltrap),		/* 255 */
};
