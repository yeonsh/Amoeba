/*	@(#)sc2txt.c	1.2	94/04/07 14:37:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "sc2txt.h"
#include "sc_global.h"

void xPrintNum(fp, Num)
FILE *fp;
TscOperand Num;
{

    fprintf(fp, " %ld",(long)Num);
}

void xPrintFlags(fp, Flags)
FILE *fp;
TscOperand Flags;
{
long x;

    x = (long)Flags;
    x = x & 0x0000000F;
    if (x == H_EXTRA) fprintf(fp, " h_extra");
    if (x == H_SIZE) fprintf(fp, " h_size");
    if (x == H_OFFSET) fprintf(fp, " h_offset");
    if (x == H_PORT) fprintf(fp, " h_port");
    if (x == H_PRIV) fprintf(fp, " h_priv");
    if (x == PSEUDOFIELD) fprintf(fp, " h_port and h_priv");
    x = (long)Flags;
    if (x & NOSIGN) fprintf(fp, " unsigned");
    if (x & INT32) fprintf(fp, " int32");
}

void xPrintCode(fp, Opcode)
FILE *fp;
TscOpcode Opcode;
{

    switch (Opcode) {

    case NoArgs:	fprintf(fp, "NoArgs");
    			break;

    case BufSize:	fprintf(fp, "BufSize");
    			break;

    case Trans:		fprintf(fp, "Trans");
    			break;

    case TTupleS:	fprintf(fp, "TTupleS");
    			break;

    case Unpack:	fprintf(fp, "Unpack");
    			break;

    case AilWord:	fprintf(fp, "AilWord");
    			break;

    case StringS:	fprintf(fp, "StringS");
    			break;

    case ListS:		fprintf(fp, "ListS");
    			break;

    case PutFS:		fprintf(fp, "PutFS");
    			break;

    case TStringSeq:	fprintf(fp, "TStringSeq");
    			break;

    case TStringSlt:	fprintf(fp, "TStringSlt");
    			break;

    case PutVS:		fprintf(fp, "PutVS");
    			break;

    case TListSeq:	fprintf(fp, "TListSeq");
    			break;

    case TListSlt:	fprintf(fp, "TListSlt");
    			break;

    case LoopPut:	fprintf(fp, "LoopPut");
    			break;

    case EndLoop:	fprintf(fp, "EndLoop");
    			break;

    case PutI:		fprintf(fp, "PutI");
    			break;

    case PutC:		fprintf(fp, "PutC");
    			break;

    case Dup:		fprintf(fp, "Dup");
    			break;

    case Pop:		fprintf(fp, "Pop");
    			break;

    case Align:		fprintf(fp, "Align");
    			break;
    
    case Pack:		fprintf(fp, "Pack");
    			break;

    case GetVS:		fprintf(fp, "GetVS");
    			break;
    
    case LoopGet:	fprintf(fp, "LoopGet");
    			break;
    
    case GetFS:		fprintf(fp, "GetFS");
    			break;
    
    case GetI:		fprintf(fp, "GetI");
    			break;
    
    case GetC:		fprintf(fp, "GetC");
    			break;
    
    case PushI:		fprintf(fp, "PushI");
    			break;
    
    case MarshTC:	fprintf(fp, "MarshTC");
    			break;
    
    case UnMarshTC:	fprintf(fp, "UnMarshTC");
    			break;

    case Equal:		fprintf(fp, "Equal");
    			break;

    default:		fprintf(fp, "Unknown opcode %04x",(short)Opcode);
    			break;
    }
}

void swapcode(pcCode, iLen)
char *pcCode;
int iLen;
{
int i = sizeof(TscOperand);
TscOpcode opcode;
TscOperand operand;

    while (i < iLen) {
        memcpy(&opcode, &pcCode[i], sizeof(TscOpcode));
        SwapOpcode(opcode);
        memcpy(&pcCode[i], &opcode, sizeof(TscOpcode));
        i += sizeof(TscOpcode);
        if (opcode & OPERAND) {
            memcpy(&operand, &pcCode[i], sizeof(TscOperand));
            SwapOperand(operand);
            memcpy(&pcCode[i], &operand, sizeof(TscOperand));
            i += sizeof(TscOperand);
        }
    }
}

void xTransCode(fp, pcCode, iLen)
FILE *fp;
char *pcCode;
int iLen;
{
int i = 0;
TscOpcode Opcode;
TscOperand Operand, magic;

   /*
    *   Read the header.
    */
   memcpy(&magic, &pcCode[i], sizeof(TscOperand));
   if (magic != SC_MAGIC) {
   	SwapOperand(magic);
       if (magic != SC_MAGIC) {
           fprintf(stderr, "No stubcode file\n");
           return;
       } else {
           swapcode(pcCode, iLen);
       }
   }
   i += sizeof(TscOperand);
   while (i < iLen) {
        memcpy(&Opcode, &pcCode[i], sizeof(TscOpcode));
        xPrintCode(fp, Opcode);
        i += sizeof(TscOpcode);
        if(Opcode & OPERAND) {
            memcpy(&Operand, &pcCode[i], sizeof(TscOperand));
            i += sizeof(TscOperand);
            if (Opcode & FLAGS) {
                xPrintFlags(fp, Operand);
            } else {
                xPrintNum(fp, Operand);
            }
        }
        fprintf(fp, "\n");
    }
}

int xiGetSize(file)
int file;
{
struct stat sFStat;

    fstat(file, &sFStat);
    return (int)sFStat.st_size;
}

main(argc, argv)
int argc;
char *argv[];
{
int iFSize, fd, i;
char *pcBuffer, acOutFile[1024];
FILE *fp;

    if (argc == 1) {
        fprintf(stderr, "Usage:%s file ...\n",argv[0]);
        exit(1);
    }
    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) == -1) {
            perror(argv[i]);
        } else {
            iFSize = xiGetSize(fd);
            pcBuffer = (char *)malloc(iFSize);
            if (read(fd, pcBuffer, (unsigned)iFSize) != iFSize) {
                perror(argv[i]);
            } else {
                close(fd);
                sprintf(acOutFile,"%s.txt",argv[i]);
                if ((fp = fopen(acOutFile,"w")) == NULL) {
                    perror(acOutFile);
                } else {
                    xTransCode(fp, pcBuffer, iFSize);
                    fclose(fp);
                }
            }
        }
    }
    return 0;
}
