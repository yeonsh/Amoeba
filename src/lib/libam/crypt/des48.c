/*	@(#)des48.c	1.2	94/04/07 09:57:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * This program is an adaption to the
 * Proposed Federal Information Processing
 *  Data Encryption Standard.
 * See Federal Register, March 17, 1975 (40FR12134)
 */

#define HBS 24
#define BS (HBS<<1)

/*
 * Initial permutation,
 */
static	char	IP[] = {
    23, 27, 34, 44, 37, 17, 12, 42,
     3, 32, 41, 29, 20,  2,  1, 10,
     0, 28, 40,  6,  7, 11, 16,  8,
    25, 30, 14, 26, 47, 38, 19, 43,
    18,  5, 35, 39, 36, 21,  4, 45,
    24, 22, 13, 33, 31,  9, 15, 46, 
};

/*
 * Final permutation, FP = IP^(-1)
 */
static	char	FP[] = {
    16, 14, 13,  8, 38, 33, 19, 20,
    23, 45, 15, 21,  6, 42, 26, 46,
    22,  5, 32, 30, 12, 37, 41,  0,
    40, 24, 27,  1, 17, 11, 25, 44,
     9, 43,  2, 34, 36,  4, 29, 35,
    18, 10,  7, 31,  3, 39, 47, 28, 
};

/*
 * Permuted-choice 1 from the key bits
 * to yield C and D.
 * Note that bits 8,16... are left out:
 * They are intended for a parity check.
 */
static	char	PC1_C[] = {
	57,49,41,33,25,17, 9,
	 1,58,50,42,34,26,18,
	10, 2,59,51,43,35,27,
	19,11, 3,60,52,44,36,
};

static	char	PC1_D[] = {
	63,55,47,39,31,23,15,
	 7,62,54,46,38,30,22,
	14, 6,61,53,45,37,29,
	21,13, 5,28,20,12, 4,
};

/*
 * Sequence of shifts used for the key schedule.
*/
static	char	shifts[] = {
	1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1,
};

/*
 * Permuted-choice 2, to pick out the bits from
 * the CD array that generate the key schedule.
 */
static	char	PC2_C[] = {
	14,17,11,24, 1, 5,
	 3,28,15, 6,21,10,
	23,19,12, 4,26, 8,
	16, 7,27,20,13, 2,
};

static	char	PC2_D[] = {
	41,52,31,37,47,55,
	30,40,51,45,33,48,
	44,49,39,56,34,53,
	46,42,50,36,29,32,
};

/*
 * The C and D arrays used to calculate the key schedule.
 */

static	char	C[56];
#define D	(C+28)
/*
 * The key schedule.
 * Generated from the key.
 */
static	char	KS[16][48];

/*
 * Set up the key schedule from the key.
 */

OWsetkey(key)
char *key;
{
	register i, j, k;
	register char *ks;
	int t;

	/*
	 * First, generate C and D by permuting
	 * the key.  The low order bit of each
	 * 8-bit char is not used, so C and D are only 28
	 * bits apiece.
	 */
	for (i=0; i<28; i++) {
		C[i] = key[PC1_C[i]-1];
		D[i] = key[PC1_D[i]-1];
	}
	/*
	 * To generate Ki, rotate C and D according
	 * to schedule and pick up a permutation
	 * using PC2.
	 */
	for (i=0, ks=KS[0]; i<16; i++, ks += 48) {
		/*
		 * rotate.
		 */
		for (k=0; k<shifts[i]; k++) {
			t = C[0];
			for (j=0; j<28-1; j++)
				C[j] = C[j+1];
			C[27] = t;
			t = D[0];
			for (j=0; j<28-1; j++)
				D[j] = D[j+1];
			D[27] = t;
		}
		/*
		 * get Ki. Note C and D are concatenated.
		 */
		for (j=0; j<24; j++) {
			ks[j] = C[PC2_C[j]-1];
			ks[j+24] = D[PC2_D[j]-28-1];
		}
	}
}

/*
 * The E bit-selection table.
 */
static	char	E[] = {
    22, 15, 12,  3,  8,  2, 23, 16,
    14, 13,  9, 10,  0,  1, 21, 19,
    18,  6, 11,  7, 17,  4, 20,  5, 
     5, 17, 11, 13, 12, 14,  8,  7,
    19, 22, 18,  9,  3,  4,  1,  6,
    16,  2, 20, 15, 10, 23,  0, 21, 
};

/*
 * The 8 selection functions.
 * For some reason, they give a 0-origin
 * index, unlike everything else.
 */
static	char	S[8][64] = {
	14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7,
	 0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8,
	 4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0,
	15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13,

	15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10,
	 3,13, 4, 7,15, 2, 8,14,12, 0, 1,10, 6, 9,11, 5,
	 0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15,
	13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9,

	10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8,
	13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1,
	13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7,
	 1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12,

	 7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15,
	13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9,
	10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4,
	 3,15, 0, 6,10, 1,13, 8, 9, 4, 5,11,12, 7, 2,14,

	 2,12, 4, 1, 7,10,11, 6, 8, 5, 3,15,13, 0,14, 9,
	14,11, 2,12, 4, 7,13, 1, 5, 0,15,10, 3, 9, 8, 6,
	 4, 2, 1,11,10,13, 7, 8,15, 9,12, 5, 6, 3, 0,14,
	11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3,

	12, 1,10,15, 9, 2, 6, 8, 0,13, 3, 4,14, 7, 5,11,
	10,15, 4, 2, 7,12, 9, 5, 6, 1,13,14, 0,11, 3, 8,
	 9,14,15, 5, 2, 8,12, 3, 7, 0, 4,10, 1,13,11, 6,
	 4, 3, 2,12, 9, 5,15,10,11,14, 1, 7, 6, 0, 8,13,

	 4,11, 2,14,15, 0, 8,13, 3,12, 9, 7, 5,10, 6, 1,
	13, 0,11, 7, 4, 9, 1,10,14, 3, 5,12, 2,15, 8, 6,
	 1, 4,11,13,12, 3, 7,14,10,15, 6, 8, 0, 5, 9, 2,
	 6,11,13, 8, 1, 4,10, 7, 9, 5, 0,15,14, 2, 3,12,

	13, 2, 8, 4, 6,15,11, 1,10, 9, 3,14, 5, 0,12, 7,
	 1,15,13, 8,10, 3, 7, 4,12, 5, 6,11, 0,14, 9, 2,
	 7,11, 4, 1, 9,12,14, 2, 0, 6,10,13,15, 3, 5, 8,
	 2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11,
};

/*
 * P is a permutation on the selected combination
 * of the current L and key.
 */
static	char	P[] = {
     3, 13,  9, 12,  8, 20, 21,  7,
     5, 23, 16,  1, 14, 18,  4, 15,
    22, 10,  2,  0, 11, 19, 17,  6, 
};

/*
 * The current block, divided into 2 halves.
 */
static	char	L[BS];
#define R	(L+HBS)
static	char	tempL[HBS];
static	char	f[32];		/* read comments below */

/*
 * Warning!!
 *
 * f[] used to be HBS for some years.
 * 21/6/1990 cbo and sater discovered that inside the loop where f is computed
 * indices are used from 0 to 31. These overlapped the preS array which is
 * declared hereafter on all compilers upto that point, but only those
 * values that were not used anymore. But the values of f are only used
 * upto HBS. Makes you wonder about the one-way property.
 * Then came ACK, and reversed the order of the arrays in the image.
 * 
 * As a short term solution f[] was increased to 32, but in the long run
 * someone should have a good look at our "oneway" function
 */

/*
 * The combination of the key and the input, before selection.
 */
static	char	preS[48];

/*
 * The payoff: encrypt a block. (Now 48 bytes, 1 bit/byte)
 */

OWcrypt48(block)
char *block;
{
	register t1,t2, j, k;
	register int i;
	register char *ks;

	/*
	 * First, permute the bits in the input
	 */
	for (j=0; j<BS; j++)
		L[j] = block[IP[j]];
	/*
	 * Perform an encryption operation 16 times.
	 */
	for (i=0, ks=KS[0]; i<16; i++,ks += 48) {
		/*
		 * Save the R array,
		 * which will be the new L.
		 */
		for (j=0; j<HBS; j++)
			tempL[j] = R[j];
		/*
		 * Expand R to 48 bits using the E selector;
		 * exclusive-or with the current key bits.
		 */
		for (j=0; j<48; j++)
			preS[j] = R[E[j]] ^ ks[j];
		/*
		 * The pre-select bits are now considered
		 * in 8 groups of 6 bits each.
		 * The 8 selection functions map these
		 * 6-bit quantities into 4-bit quantities
		 * and the results permuted
		 * to make an f(R, K).
		 * The indexing into the selection functions
		 * is peculiar; it could be simplified by
		 * rewriting the tables.
		 */
		for (j=0,t1=0,t2=0; j<8; j++,t1+=6,t2+=4) {
			k = S[j][(preS[t1+0]<<5)+
				(preS[t1+1]<<3)+
				(preS[t1+2]<<2)+
				(preS[t1+3]<<1)+
				(preS[t1+4]<<0)+
				(preS[t1+5]<<4)];
			f[t2+0] = (k>>3)&01;
			f[t2+1] = (k>>2)&01;
			f[t2+2] = (k>>1)&01;
			f[t2+3] = (k>>0)&01;	/* 3 .. 31 !!! */
		}
		/*
		 * The new R is L ^ f(R, K).
		 * The f here has to be permuted first, though.
		 */
		for (j=0; j<HBS; j++)
			R[j] = L[j] ^ f[P[j]];
		/*
		 * Finally, the new L (the original R)
		 * is copied back.
		 */
		for (j=0; j<HBS; j++)
			L[j] = tempL[j];
	}
	/*
	 * The output L and R are reversed.
	 */
	for (j=0; j<HBS; j++) {
		t1 = L[j];
		L[j] = R[j];
		R[j] = t1;
	}
	/*
	 * The final output
	 * gets the inverse permutation of the very original.
	 */
	for (j=0; j<BS; j++)
		block[j] = L[FP[j]];
}
