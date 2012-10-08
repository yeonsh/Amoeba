/*	@(#)scdocgen.c	1.2	94/04/07 14:38:37 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifdef PYTHON
#include "stubcode.h"

static void xPrintType();

static void xPrintTypeOnly(fp, psType, iWay)
handle fp;
TpsPythonTree psType;
int iWay;
{

    switch (psType->iType) {
    
    case pt_integer:
            if (!(psType->iFlags & (iWay & PF_IN ? PF_CALC_IN : PF_CALC_OUT))) {
                mypf(fp, " Integer ");
            }
            break;
    
    case pt_cap:
            mypf(fp, " Capability ");
            break;
    
    case pt_tuple:
            mypf(fp, " %s ",psType->psType->il_name);
            if (xiNumberOfElem(psType, iWay) > 1) {
                mypf(fp, "(");
                for (psType = psType->psPrim; psType != NULL; psType = psType->psNext) {
                    xPrintType(fp, psType, iWay);
                }
                mypf(fp, ") ");
            }
            break;
    case pt_list:
            mypf(fp, "%s  [",psType->psType->il_name);
            for (psType = psType->psPrim; psType != NULL; psType = psType->psNext) {
                xPrintTypeOnly(fp, psType, iWay);
            }
            mypf(fp, " ] ");
            break;
    
    case pt_string:
            mypf(fp, "String ");
            break;
    }
}


static void xPrintType(fp, psType, iWay)
handle fp;
TpsPythonTree psType;
int iWay;
{

    if (psType->iFlags & iWay) {
        switch (psType->iType) {
    
        case pt_integer:
                if (!(psType->iFlags & (iWay & PF_IN ? PF_CALC_IN : PF_CALC_OUT))) {
                    mypf(fp, "Integer %s ", psType->psType->il_name);
                }
                break;
    
        case pt_cap:
                mypf(fp, "Capability %s ", psType->psType->il_name);
                break;

        case pt_arglist:
                mypf(fp, "%s (", psType->psType->il_name);
                for(psType = psType->psPrim; psType != NULL; psType = psType->psNext) {
                    xPrintType(fp, psType, iWay);
                }
                mypf(fp, ") ");
                break;
    
        case pt_tuple:
                mypf(fp, "%s ", psType->psType->il_name);
                if (xiNumberOfElem(psType, iWay) > 1) {
                    mypf(fp, "(");
                    for(psType = psType->psPrim; psType != NULL; psType = psType->psNext) {
                        xPrintType(fp, psType, iWay);
                    }
                    mypf(fp, ") ");
                } else {
                    xPrintType(fp, psType->psPrim, iWay, 0);
                }
                break;

        case pt_list:
                mypf(fp, "%s [", psType->psType->il_name);
                for (psType = psType->psPrim; psType != NULL; psType = psType->psNext) {
                    xPrintTypeOnly(fp, psType, iWay);
                }
                mypf(fp, " ] ");
                break;
    
        case pt_string:
                mypf(fp, "String %s ", psType->psType->il_name);
                break;
        }
    }
}

void xGenDoc(fp, psPTree)
handle fp;
TpsPythonTree psPTree;
{

    assert(psPTree != NULL);
    xPrintType(fp, psPTree, PF_IN);
    mypf(fp, "  -->  ");
    xPrintType(fp, psPTree, PF_OUT);
    mypf(fp, "%n");
}
#else
scdocgen_dummy() { }
#endif /* PYTHON */
