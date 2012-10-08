/*	@(#)pdbg.c	1.2	94/04/07 14:37:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifdef PYTHON
#include "stubcode.h"

void xDumpEntry(), xDumpTree();

/*
 *   In this file there are a few functions to diplay some debug
 *   information about the build PythonTree.
 */

void xDumpFlags(iFlags)
int iFlags;
{

    mypf(OUT_HANDLE,"with flags :");
    if (iFlags & PF_ARRPAR) mypf(OUT_HANDLE," PF_ARRPAR");
    if (iFlags & PF_FIXARR) mypf(OUT_HANDLE," PF_FIXARR");
    if (iFlags & PF_VARARR) mypf(OUT_HANDLE," PF_VARARR");
    if (iFlags & PF_IN) mypf(OUT_HANDLE," PF_IN");
    if (iFlags & PF_OUT) mypf(OUT_HANDLE," PF_OUT");
    if (iFlags & PF_EXPR) mypf(OUT_HANDLE," PF_EXPR");
    if (iFlags & PF_CALC_IN) mypf(OUT_HANDLE, " PF_CALC_IN");
    if (iFlags & PF_CALC_OUT) mypf(OUT_HANDLE," PF_CALC_OUT");
    if (iFlags & PF_INTUPLE) mypf(OUT_HANDLE," PF_INTUPLE");
    if (iFlags & PF_INLIST) mypf(OUT_HANDLE," PF_INLIST");
}

static void xDumpArrPars(psArr, pc)
TpsPythonTree psArr;
char *pc;
{

    if (psArr->psArrInfo != NULL) {
    TpsLink psWalk;
    
        psWalk = psArr->psArrInfo;
        mypf(OUT_HANDLE,"%n%s",pc);
        while (psWalk != NULL) {
            mypf(OUT_HANDLE," %s",psWalk->psLink->psType->il_name);
            psWalk = psWalk->psNext;
        }
    }
}

static void xDumpCap(psCap)
TpsPythonTree psCap;
{

    mypf(OUT_HANDLE,"%nCapability, name %s%n",psCap->psType->il_name);
    xDumpFlags(psCap->iFlags);
}

static void xDumpInt(psInt)
TpsPythonTree psInt;
{

    mypf(OUT_HANDLE,"%nInteger, name %s%n",psInt->psType->il_name);
    xDumpFlags(psInt->iFlags);
    mypf(OUT_HANDLE,"%n");
    if (psInt->iFlags & PF_ARRPAR)
        mypf(OUT_HANDLE,"and bound to a list/string");
    else
        mypf(OUT_HANDLE,"and NOT bound to a list/string");
}

static void xDumpString(psString)
TpsPythonTree psString;
{

    mypf(OUT_HANDLE,"%nString, name : %s%n",psString->psType->il_name);
    xDumpFlags(psString->iFlags);
    mypf(OUT_HANDLE,"%n");
    if (psString->iFlags & PF_FIXARR)
        mypf(OUT_HANDLE,"with fixed length");
    if (psString->iFlags & PF_VARARR)
        mypf(OUT_HANDLE,"with variable length");
    xDumpArrPars(psString, "with parameter(s) :");
}

static void xDumpList(psList)
TpsPythonTree psList;
{

    mypf(OUT_HANDLE,"%nList, name : %s%n",psList->psType->il_name);
    xDumpFlags(psList->iFlags);
    mypf(OUT_HANDLE,"%n");
    if (psList->iFlags & PF_FIXARR)
        mypf(OUT_HANDLE,"with fixed length");
    if (psList->iFlags & PF_VARARR)
        mypf(OUT_HANDLE,"with variable length");
    xDumpArrPars(psList, "with parameter(s) :");
    mypf(OUT_HANDLE,"%nof%>");
    if (psList->psPrim->iType == pt_integer)
        mypf(OUT_HANDLE,"%ninteger");
    else
        xDumpTree(psList->psPrim);
    mypf(OUT_HANDLE,"%<");
}

static void xDumpTuple(psTuple)
TpsPythonTree psTuple;
{

    mypf(OUT_HANDLE,"%nTuple, name : %s%n",psTuple->psType->il_name);
    xDumpFlags(psTuple->iFlags);
    mypf(OUT_HANDLE,"%n");
    mypf(OUT_HANDLE,"consists of :%>");
    xDumpTree(psTuple->psPrim);
    mypf(OUT_HANDLE,"%<");
}

static void xDumpArgList(psTuple)
TpsPythonTree psTuple;
{

    mypf(OUT_HANDLE,"%nTuple for an Argument list, name : %s%n",
                    psTuple->psType->il_name);
    xDumpFlags(psTuple->iFlags);
    mypf(OUT_HANDLE,"%n");
    mypf(OUT_HANDLE,"consists of :%>");
    xDumpTree(psTuple->psPrim);
    mypf(OUT_HANDLE,"%<");
}

void xDumpTree(psPTree)
TpsPythonTree psPTree;
{

    while (psPTree != NULL) {
        xDumpEntry(psPTree);
        psPTree = psPTree->psNext;
    }
    mypf(OUT_HANDLE,"%n");
}

void xDumpEntry(psPTree)
TpsPythonTree psPTree;
{

    switch (psPTree->iType) {
    
    case pt_tuple:
            xDumpTuple(psPTree);
            break;
    case pt_list:
            xDumpList(psPTree);
            break;
    case pt_string:
            xDumpString(psPTree);
            break;
    case pt_cap:
            xDumpCap(psPTree);
            break;
    case pt_integer:
            xDumpInt(psPTree);
            break;
    case pt_arglist:
            xDumpArgList(psPTree);
            break;
    default:
            mypf(OUT_HANDLE,"UnKnown type %d in Python Tree%n");
            break;
    }
    mypf(OUT_HANDLE,"%n");
}

void xDumpStack(apsStack, sp)
TpsPythonTree apsStack[];
int sp;
{
int i;

    mypf(OUT_HANDLE, "%nStack:%>%n");
    for(i=0; i < sp; i++) {
        mypf(OUT_HANDLE, "%<Item %d:%n%>", i);
        mypf(OUT_HANDLE, "%s%n", apsStack[i]->psType->il_name);
    }
    mypf(OUT_HANDLE, "%<");
}
#else
pdbg_dummy() {}
#endif /* PYTHON */
