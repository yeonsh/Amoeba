/*	@(#)scclgen.c	1.2	94/04/07 14:38:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifdef PYTHON
#if 0
#define STACK_TRACE
#endif
#include "stubcode.h"
#include "sc_global.h"

/*
 *	In this file the stubcode for the client is generated. The code
 *	generation is split into two parts, the marshal and unmarshcode.
 *	The main routine to gererate the code is x(Un)MarshalCode. The
 *	only thing it does is to start the code generator for a specific
 *	type. These routines take care that the item(s) that they want
 *	on the top of the stack will be there. And they cleanup there own
 *	garbage they make on the stack. For every Python type there is one
 *	Marshal and one UnMarshal function.
 */

#define DUMPSTK()	xDumpStack(apsStack, sp)

static void xMarshalCode(), xGetItemOnStack(), xUnMarshalClCode();

static TpsPythonTree apsStack[STKSIZE];
static int sp, iStop, bp;

static void xPop(iNumber)
int iNumber;
{

    if (sp-iNumber+1 > 0) {
#ifdef STACK_TRACE
    int i;
    
        for(i = sp-1; i >= sp-iNumber; i--) {
            mypf(OUT_HANDLE, "popped %s%n", apsStack[i]->psType->il_name);
        }
#endif
        sp -= iNumber;
    } else {
        mypf(ERR_HANDLE,"%rTrying to pop from an empty stack%n");
    }
}

static void xPush(psElem)
TpsPythonTree psElem;
{

    if (sp >= STKSIZE){
        mypf(ERR_HANDLE,"%rStack full%n");
        return;
    }
    apsStack[sp++] = psElem;
#ifdef STACK_TRACE
    mypf(OUT_HANDLE," pushed %s%n", psElem->psType->il_name);
#endif
}

static TpsPythonTree xpsTop()
{

    if (sp > 0) {
        return apsStack[sp-1];
    }
    mypf (ERR_HANDLE,"%rEmpty Stack%n");
    return NULL;
}

static xDup(n)
int n;
{

    apsStack[sp] = apsStack[sp-n];
    sp++;
#ifdef STACK_TRACE
    mypf(OUT_HANDLE, " dupped %s%n", apsStack[sp-1]->psType->il_name);
#endif
}

static long lLabelCount;

long xlNewLabel()
{

    return lLabelCount++;
}

/*
**	xlGetType returns the specifiers of an integer.
*/

long xlGetType(psTypeDesc)
struct typdesc *psTypeDesc;
{
long lRet = 0L;

    if (psTypeDesc == NULL) return 0L;
    switch (psTypeDesc->td_kind) {

    case tk_tdef:
            return xlGetType(psTypeDesc->td_prim);
            
    case tk_int:
            if (psTypeDesc->td_sign == 1) lRet |= NOSIGN;
            if (psTypeDesc->td_size >= 0) lRet |= INT32;
            break;
    
    default:
            printf("GetType is 0\n");
            break;
    
    }
    return lRet;
}

/*
**	xlSetFlags returns the flag to set for the use of a headerfield.
*/

long xlSetFlags(pc)
char *pc;
{
long lRet;

    if (pc != NULL) {
        lRet = (long)AmFind(pc) + 1;
    } else lRet = 0L;
    return lRet;
}

/*
**	xiNumberOfElem return the number of elements in a tuple that goes the
**	same way as the tuple and can not be calculated from an array size.
*/

int xiNumberOfElem(psTuple, iWay)
TpsPythonTree psTuple;
{
int iCount = 0, iCalc;

    iCalc = iWay & PF_IN ? PF_CALC_IN : PF_CALC_OUT;
    for (psTuple = psTuple->psPrim; psTuple != NULL; psTuple = psTuple->psNext) {
        if (!(psTuple->iFlags & iCalc) && (psTuple->iFlags & iWay)) iCount++;
    }
    return iCount;
}

static void xInitCodeGen(psTuple)
TpsPythonTree psTuple;
{

    /*
     *   Init stack
     */
    sp = bp = 0;
    iStop = No;
    xPush(psTuple);
    lLabelCount = 1L;
}

static void xUnInitCodeGen(fp)
handle fp;
{

    /*
     *   Empty the stack.
     */
    if (sp > 0) {
        xPop(sp);
        mypf(fp, "%c%b", (TscOpcode)Pop, (TscOperand)sp);
    }
    sp = 0;
}

/*
**	Look if two Python tree items are equal.
*/

static int xiEqual(psItem1, psItem2)
TpsPythonTree psItem1, psItem2;
{

    if (psItem1->iType != psItem2->iType) return No;
    if (strcmp(psItem1->psType->il_name, psItem2->psType->il_name)) return No;
    return Yes;
}

/*
**	Get an item on the stack. The flags mean :
**
**	bForce	:	dup the item even if it is on top of the stack.
**	bOut	:	the item is an out parameter and if it can be
**			calculated on in it would not be dupped when this
**			flag is not set
*/

static void xGetItemOnStack(fp, psItem, bForce, bOut)
handle fp;
TpsPythonTree psItem;
Bool bForce;
Bool bOut;
{
int iCount = 1, iSp = sp;

    if ((!(psItem->iFlags & PF_CALC_IN) || bOut )&& psItem->psType != NULL) {
#ifdef STACK_TRACE
	mypf(OUT_HANDLE,"GetItem:%s%n", psItem->psType->il_name);
#endif
        while (iSp > 0) {
            if (xiEqual(apsStack[iSp-1], psItem)) break;
            iSp--;
            iCount++;
        }
        if (iSp > 0) {
            if (iCount > 1 || bForce) {
                mypf(fp, "%c%b", (TscOpcode)Dup, (TscOperand)iCount);
                xDup(iCount);
            }
        }
    }
}    

/*
**	xlGetListSize returns the fixed or maximum size of a list or string.
*/

static long xlGetListSize(psList)
TpsPythonTree psList;
{
struct etree *psBSize;

    if (psList->psType->il_type->td_kind == tk_char) return 1L;
    psBSize = psList->psType->il_type->td_bound;
    if (psBSize == NULL) {
        psBSize = psList->psType->il_type->td_act;
        if (psBSize == NULL) {
            mypf(ERR_HANDLE, "%rNo size found for string %s%n",
                             psList->psType->il_name);
            return 0L;
        }
    }
    if (psBSize->et_const) {
        return (long)psBSize->et_val;
    }
    /*
    **	The size is unknown, return the maximum buffer size.
    */
    return 30000L;
}

/*
**	xFindPar puts the integer that gives the size of an array on
**	top of the stack.
*/

static void xFindPar(fp, psList)
handle fp;
TpsPythonTree psList;
{
int iCount = 0;
TpsLink psWalk;

    for (psWalk = psList->psArrInfo; psWalk != NULL; psWalk = psWalk->psNext) {
        iCount++;
    }
    if (iCount != 1 || psList->iFlags & PF_EXPR) {
        mypf(ERR_HANDLE, "%wNo implemetation for bounderies with expressions%n");
        iStop = Yes;
        return;
    }
    xGetItemOnStack(fp, psList->psArrInfo->psLink, Yes, Yes);
}

/*
**	Set the alignment if necessary or possible
*/

static void xAlign(fp, psItList)
handle fp;
struct itlist *psItList;
{
struct typdesc *psTypeDesc;
int ret = 0;

    if (psItList == NULL) return;
    psTypeDesc = psItList->il_type;
    if (psTypeDesc == NULL) return;   /*  Some things aren't a real type */
    if ((ret = psTypeDesc->td_align) > 1) {
        mypf(fp, "%c%b", (TscOpcode)Align, (TscOperand)psTypeDesc->td_align);
    }
}

/*
**	Look it a (Un)Marshal function must be used.
*/

static int xMarshSet(psTree)
TpsPythonTree psTree;
{
struct itlist *psType;

    psType = psTree->psType;
    if (psType == NULL)
        return No;
    if (psType->il_type == NULL)
        return No;
    if (psType->il_type->td_marsh == NULL)
        return No;
    /*
    **   The (un)marshal functions 'cm_cap' and 'cu_cap' should be ignored
    */
    if ((strcmp(psType->il_type->td_marsh->m_clm, "cm_cap") == 0) ||
           (strcmp(psType->il_type->td_marsh->m_clu, "cu_cap") == 0))
        return No;
    /*   Only things that get marshalled in the buffer can use a marshal
     *   routine for now.
     */
    if (psTree->iFlags & PF_INTUPLE)
        return Yes;
    if (psType->il_attr == NULL)
        return Yes;
    if (psType->il_attr->at_hmem == NULL)
        return Yes;
    return No;
}

struct MarshStubcodes {
        char		*ms_name;
        TscOpcode	ms_stubcode;
};

/*
**	In this stucture the (Un)Marshal functions that can be used are
**	defined. If you would want to add one just put the name of the
**	function in the ms_name field and the opcode in the ms_stubcode
**	field. You also have to define the opcode in the include file
**	"global_scdata.h".
*/

static struct MarshStubcodes MarshalCodes[] = {
	{	"tc_marshal",	MarshTC	},
	{	"tc_unmarshal",	UnMarshTC },
	{	NULL, 		0	}
};

/*
**	If we need to use a (Un)Marshal function this function determines
**	wich one and has to take care of the thing that must be done
**
**	Warning: The function works but it is not useable for now.
*/

static void xCallMarshFunc(fp, psMarshName, iMarsh)
handle fp;
struct m_name *psMarshName;
int iMarsh;	/* IN or OUT */
{
int i = 0;
char *pcMarshName;

    pcMarshName = iMarsh ? psMarshName->m_clm : psMarshName->m_clu;
    printf("Name : %s\n", pcMarshName);
    while(MarshalCodes[i].ms_name != NULL) {
        if (!strcmp(pcMarshName, MarshalCodes[i].ms_name)) break;
        i++;
    }
    if (MarshalCodes[i].ms_stubcode == 0) {
        mypf(ERR_HANDLE,"%r%s is not a marshal routine with a stubcode%n",
                         pcMarshName);
    } else {
        mypf(fp,"%c", (TscOpcode)MarshalCodes[i].ms_stubcode);
    }
}

static void xUnpackCode(fp, iSize)
int fp;
int iSize;
{
int i = 0;
TpsPythonTree psElem;

    psElem = xpsTop();
    psElem = psElem->psPrim;
    if (iSize > 1) {
    	mypf(fp,"%c%b", (TscOpcode)TTupleS, (TscOperand)iSize);
        mypf(fp,"%c%b", (TscOpcode)Unpack, (TscOperand)iSize);
    }
    if (iSize == 0) {
        mypf(fp, "%c", (TscOpcode)NoArgs);
        return;
    }
    if (iSize > 0) xPop(1);
    while (i < iSize) {
        if ((psElem->iFlags & PF_IN) && !(psElem->iFlags & PF_CALC_IN)) {
            xPush(psElem);
            i++;
        }
        psElem = psElem->psNext;
    }
}

xStackOrderGood(apsOrder, len)
TpsPythonTree *apsOrder;
int len;
{
register int i;

    if (len > (sp - 1)) return No;
    for (i = 0; i < len; i++) {
    	if (!xiEqual(apsOrder[i], apsStack[sp - i - 1])) return No;;
    }
    return Yes;
}

#define	MAXTUPLESIZE	64

static void xPackCode(fp, psTuple, psRealTuple)
handle fp;
TpsPythonTree psTuple, psRealTuple;
{
int iSize, i = 0;
TpsPythonTree apsReverse[MAXTUPLESIZE];

    if ((iSize = xiNumberOfElem(psTuple, PF_OUT)) > 1) {
    	/*
    	**	Reverse the arguments in the tuple.
    	*/
    	for (psRealTuple = psRealTuple->psPrim; psRealTuple != NULL;
    			psRealTuple = psRealTuple->psNext) {
    	    apsReverse[i++] = psRealTuple;
    	    if (i >= MAXTUPLESIZE) assert(0);
    	}
	psRealTuple = apsReverse[0];
    	for (psRealTuple = psRealTuple->psPrim; psRealTuple != NULL;
    			psRealTuple = psRealTuple->psNext) {
    	    /*
    	    **	If the objects are already in the right order do nothing
    	    **	anymore.
    	    */
    	    if (xStackOrderGood(apsReverse, i)) return;
    	    /*
    	     *   Order the elements on the stack so they are in the
    	     *   same order as in the real tuple.
    	     */
    	    if (psRealTuple->iType == pt_tuple) {
    	        /*
    	         *   If a tuple has only one element then the element is
    	         *   the stack instead of the tuple.
    	         */
    	        if (xiNumberOfElem(psRealTuple, PF_OUT) == 1) {
    	        TpsPythonTree psTmp;
    	        
    	            /*
    	             *   Find the element that is on the stack.
    	             */
    	            for (psTmp = psRealTuple->psPrim; psTmp != NULL; psTmp = psTmp->psNext) {
    	                if (!(psTmp->iFlags & PF_CALC_OUT)) break;
    	                /*   Found it */
    	            }
    	            xGetItemOnStack(fp, psTmp, No, No);
    	        } else {
    	            xGetItemOnStack(fp, psRealTuple, No, No);
    	        }
    	    } else if (!(psRealTuple->iFlags & PF_CALC_OUT)) {
    	        /*
    	         *   Only put things in the tuple when they cann't
    	         *   be calculated.
    	         */
    	        xGetItemOnStack(fp, psRealTuple, No, No);
    	    }
    	}
    	mypf(fp, "%c%b", (TscOpcode)Pack, (TscOperand)iSize);
        xPop(iSize);
        xPush(psTuple);
    }
}

static void xMarshClTupleCode(fp, psTuple)
handle fp;
TpsPythonTree psTuple;
{
int iSize, iOldSp;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE,"Before Tuple%n");
	DUMPSTK();
#endif
    xGetItemOnStack(fp, psTuple, No, No);
    iSize = xiNumberOfElem(psTuple, PF_IN);
    iOldSp = sp -  1;   /*   sp points to the tuple and it will		*/
    			/*   removed by xUnpackCode.			*/
    xUnpackCode(fp, iSize);
    xMarshalCode(fp, psTuple->psPrim);
    xAlign(fp, psTuple->psType);
    if (ErrCnt) return;
    if ((iSize > 0) && ((sp - iOldSp) > 0)) {
        mypf(fp, "%c%b", (TscOpcode)Pop, (TscOperand)(sp - iOldSp));
        xPop(sp - iOldSp);
    }
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After Tuple%n");
	DUMPSTK();
#endif
}

static void xUnMarshClTupleCode(fp, psTuple, psReal)
handle fp;
TpsPythonTree psTuple, psReal;
{

    /*   Find the real tuple.   */
    for (; psReal != NULL; psReal = psReal->psNext) {
        if (xiEqual(psReal, psTuple)) break;
    }
    /*   There must be one      */
    assert(psReal != NULL);
    /*   Unmarshal the elements */
    xUnMarshalClCode(fp, psTuple->psPrim, psReal->psPrim);
    xAlign(fp, psTuple->psType);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before PackCode%n");
	DUMPSTK();
#endif
    xPackCode(fp, psTuple, psReal);
}

static void xMarshClStrCode(fp, psStr)
handle fp;
TpsPythonTree psStr;
{
long lSize;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before Str%n");
	DUMPSTK();
#endif
    xGetItemOnStack(fp, psStr, No, No);
    xAlign(fp, psStr->psType);
    if (psStr->iFlags & PF_FIXARR) {
        lSize = xlGetListSize(psStr);
        mypf(fp, "%c%b", PutFS, (TscOperand)lSize);
    } else if (psStr->iFlags & PF_VARARR) {
        lSize = xlGetListSize(psStr);
        mypf(fp, "%c%b", TStringSlt, (TscOperand)lSize);
        mypf(fp, "%c", PutVS);
    } else {
        mypf(ERR_HANDLE, "%rAn array with no fixed or variable size%n");
    }
    xPop(1);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After Str%n");
	DUMPSTK();
#endif
}

static void xUnMarshClStrCode(fp, psString)
handle fp;
TpsPythonTree psString;
{
long lSize;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before UStr%n");
	DUMPSTK();
#endif
    xAlign(fp, psString->psType);
    if (psString->iFlags & PF_FIXARR) {
        lSize = xlGetListSize(psString);
        mypf(fp, "%c%b", GetFS, (TscOperand)lSize);
        xPush(psString);
    } else if (psString->iFlags & PF_VARARR) {
        lSize = xlGetListSize(psString);
        xFindPar(fp, psString);
        mypf(fp, "%c", GetVS);
        mypf(fp, "%c%b", TStringSlt, (TscOperand)lSize);
        xPop(1);
        xPush(psString);
    } else {
        mypf(ERR_HANDLE, "%rAn array with no fixed or variable size%n");
    }
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After UStr%n");
	DUMPSTK();
#endif
}
static int xiInHeader(psInt)
TpsPythonTree psInt;
{

    if (psInt->iFlags & PF_INTUPLE) return No;
    if (psInt->psType->il_attr == NULL) return No;
    if (psInt->psType->il_attr->at_hmem == NULL) return No;
    return Yes;
}

static void xCalcArrSize(fp, psList)
handle fp;
TpsPythonTree psList;
{

    if (psList->iType == pt_list) {
      	mypf(fp, "%c", (TscOpcode)ListS);
    } else if (psList->iType == pt_string) {
       	mypf(fp, "%c", (TscOpcode)StringS);
    } else {
       	mypf(ERR_HANDLE, "%rCan not calculate the size of type %d.%n",
       				psList->iType);
    }
}

static void xMarshClIntCode(fp, psInt)
handle fp;
TpsPythonTree psInt;
{
long lFlags;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before Int%n");
	DUMPSTK();
#endif
    if (psInt->iFlags & PF_CALC_IN) {
    TpsPythonTree psList, psSave;
    TpsLink psWalk;
    int iCount = 0, isexpr = No;

        /*
         *   The integer can be calculated from an arry or string. Let's
         *   try and find this array or string.
         */
        for (psWalk = psInt->psArrInfo; psWalk != NULL; psWalk = psWalk->psNext) {
            psList = psWalk->psLink;
            if (psList->iFlags & PF_IN) {
            	iCount++;
            	isexpr |= (psList->iFlags & PF_EXPR);
                psSave = psList;
            }
        }
        if (iCount == 1) {
            /*
            **
            */
            xGetItemOnStack(fp, psSave, Yes, No);
            xCalcArrSize(fp, psSave);
        } else if (!isexpr) {
            register int i;

            /*
            **	The integer is used in more then one array but not in
            **	an expression, so we have to get all the sizes of the 
            **	array on top of the stack and check if they are equal.
            */
            for (psWalk = psInt->psArrInfo; psWalk != NULL;
            				psWalk = psWalk->psNext) {
            	psList = psWalk->psLink;
            	if (psList->iFlags & PF_IN) {
            	    xGetItemOnStack(fp, psList, Yes, No);
            	    xCalcArrSize(fp, psList);
            	}
            }
            for (i = 1; i < iCount; i++) {
            	mypf(fp, "%c", (TscOpcode)Equal);
            	mypf(fp, "%c%b", (TscOpcode)Pop, (TscOperand)1);
            	xPop(1);
            }
        } else {
            /*
            **	The integer is part of an expression. There is no
            **	implementation for that in stubcode right now.
            */
            mypf(ERR_HANDLE, "%rNo implementation for integers when they are part of an expression. Sorry.%n");
            iStop = Yes;
            return;
        }
    } else {
        xGetItemOnStack(fp, psInt, No, No);
    }
    if (psInt->psType) {
        if (!xiInHeader(psInt)) {
            /*
             *   Don't marshal in header. Only the type is important.
             */
            lFlags = xlGetType(psInt->psType->il_type);
            xAlign(fp, psInt->psType);
            mypf(fp, "%c%b", (TscOpcode)PutI, (TscOperand)lFlags);
        } else {
            lFlags = xlSetFlags(psInt->psType->il_attr->at_hmem);
            lFlags |= xlGetType(psInt->psType->il_type);
            mypf(fp, "%c%b", (TscOpcode)PutI, (TscOperand)lFlags);
        }
    } else {
        xAlign(fp, psInt->psType);
        mypf(fp, "%c%b", (TscOpcode)PutI, (TscOperand)0);
    }
    xPop(1);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After Int%n");
	DUMPSTK();
#endif
}

static void xUnMarshClIntCode(fp, psInt)
handle fp;
TpsPythonTree psInt;
{
long lFlags;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before UInt%n");
	DUMPSTK();
#endif
    if (psInt->psType) {
        if (!xiInHeader(psInt)) {
            lFlags = xlGetType(psInt->psType->il_type);
            xAlign(fp, psInt->psType);
            mypf(fp, "%c%b", (TscOpcode)GetI, (TscOperand)lFlags);
        } else {
            lFlags = xlSetFlags(psInt->psType->il_attr->at_hmem);
            lFlags |= xlGetType(psInt->psType->il_type);
            mypf(fp, "%c%b", (TscOpcode)GetI, (TscOperand)lFlags);
        }
    } else {
        xAlign(fp, psInt->psType);
        mypf(fp, "%c%b", (TscOpcode)GetI, (TscOperand)0);
    }
    xPush(psInt);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After UInt%n");
	DUMPSTK();
#endif
}

static void xMarshClCapCode(fp, psCap)
handle fp;
TpsPythonTree psCap;
{

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before Cap%n");
	DUMPSTK();
#endif
    xGetItemOnStack(fp, psCap, No, No);
    xAlign(fp, psCap->psType);
    mypf(fp, "%c%b", (TscOpcode)PutC, (TscOperand)0);
    xPop(1);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After Cap%n");
	DUMPSTK();
#endif
}

static void xUnMarshClCapCode(fp, psCap)
handle fp;
TpsPythonTree psCap;
{

    if (psCap->psType) {
    	if (psCap->psType->il_attr) {
            if ((psCap->psType->il_attr->at_hmem == NULL) || (psCap->iFlags & PF_INTUPLE)) {
               /*
                *   Not in header.
                */
               xAlign(fp, psCap->psType);
               mypf(fp, "%c%b", (TscOpcode)GetC, (TscOperand)0);
           } else {
               /*
                *   It is in the header.
                */
               mypf(fp, "%c%b", (TscOpcode)GetC, (TscOperand)xlSetFlags(psCap->psType->il_attr->at_hmem));
           }
       } else {
           mypf(fp, "%c%b", (TscOpcode)GetC, (TscOperand)0);
           xAlign(fp, psCap->psType);
       }
   } else {
       mypf(fp, "%c%b", (TscOpcode)GetC, (TscOperand)0);
       xAlign(fp, psCap->psType);
   }
   xPush(psCap);
}

static void xMarshClListCode(fp, psList)
handle fp;
TpsPythonTree psList;
{
long lScratch;

#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "Before List%n");
	DUMPSTK();
#endif
    lScratch = xlGetListSize(psList);
    /*
    **	Put the item that will be marshalled on top of the
    **	stack
    */
    xGetItemOnStack(fp, psList, No, No);
    if (psList->iFlags & PF_FIXARR) {
        mypf(fp, "%c", (TscOpcode)TListSeq);
    } else {
        /* Here code to see if the size is an expression. */
        mypf(fp, "%c", (TscOpcode)TListSlt);
    }
    mypf(fp, "%b", (TscOperand)lScratch);
    lScratch = xlNewLabel();
    xAlign(fp, psList->psType);
    mypf(fp, "%c%b", (TscOpcode)LoopPut, (TscOperand)lScratch);
    /*
    **	LoopPut gets the indexed element from the list and pushes it
    **	on the stack
    */
    xPush(psList->psPrim);
    xMarshalCode(fp, psList->psPrim);
    mypf(fp, "%c%b", (TscOpcode)EndLoop, (TscOperand)lScratch);
    xPop(1);
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After List%n");
	DUMPSTK();
#endif
}

static void xUnMarshListCode(fp, psList, psReal)
handle fp;
TpsPythonTree psList, psReal;
{
long lSize, lLabel;

#ifdef STACK_TRACE
    mypf(OUT_HANDLE, "Before UList%n");
#endif
    lSize = xlGetListSize(psList);
    lLabel = xlNewLabel();
    xAlign(fp, psList->psType);
    if (psList->iFlags & PF_FIXARR) {
        mypf(fp, "%c%b", (TscOpcode)PushI, (TscOperand)lSize);
    } else {
        xFindPar(fp, psList);
    }
    mypf(fp, "%c%b", (TscOpcode)LoopGet, (TscOperand)lLabel);
    for(;psReal != NULL; psReal = psReal->psNext) {
        if (xiEqual(psReal, psList)) break;
    }
    assert(psReal != NULL);
    xPop(1); /* The size of the array is poped by LoopGet */
    xUnMarshalClCode(fp, psList->psPrim, psReal->psPrim);
    mypf(fp, "%c%b", (TscOpcode)EndLoop, (TscOperand)lLabel);
    xPop(1);
    xPush(psList);
#ifdef STACK_TRACE
    mypf(OUT_HANDLE, "After UList%n");
#endif
}

static void xMarshalCode(fp, psOnWire)
int fp;
TpsPythonTree psOnWire;
{

    while(psOnWire != NULL) {
        if (psOnWire->iFlags & PF_IN) {
            if (xMarshSet(psOnWire)) {
            	xCallMarshFunc(fp, psOnWire->psType->il_type->td_marsh, Yes);
            } else {
                switch(psOnWire->iType) {
    
                case pt_tuple:
                case pt_arglist:
                        xMarshClTupleCode(fp, psOnWire);
                        break;

                case pt_list:
                        xMarshClListCode(fp, psOnWire);
                        break;
    
                case pt_string:
                        xMarshClStrCode(fp, psOnWire);
                        break;
    
                case pt_cap:
                        xMarshClCapCode(fp, psOnWire);
                        break;
    
                case pt_integer:
                        xMarshClIntCode(fp, psOnWire);
                        break;
    
                default:
                        mypf(ERR_HANDLE, "%rUnknow type %d%n", psOnWire->iType);
                        return;
                }
            }
        }
        if(ErrCnt != 0) return;
        psOnWire = psOnWire->psNext;
    }
}

static void xUnMarshalClCode(fp, psOnWire, psReal)
handle fp;
TpsPythonTree psOnWire, psReal;
{

    while (psOnWire != NULL) {
        if (psOnWire->iFlags & PF_OUT) {
            if (xMarshSet(psOnWire)) {
                xCallMarshFunc(fp, psOnWire->psType->il_type->td_marsh, No);
            } else {
                switch (psOnWire->iType) {
                
                case pt_arglist:
                case pt_tuple:
                        xUnMarshClTupleCode(fp, psOnWire, psReal);
                        break;
                        
                case pt_cap:
                        xUnMarshClCapCode(fp, psOnWire);
                        break;
                
                case pt_list:
                        xUnMarshListCode(fp, psOnWire, psReal);
                        break;
                
                case pt_integer:
                        xUnMarshClIntCode(fp, psOnWire);
                        break;
                        
                case pt_string:
                        xUnMarshClStrCode(fp, psOnWire);
                        break;
                }
            }
        }
        if (ErrCnt != 0) return;
        psOnWire = psOnWire->psNext;
    }
}

void xGenClCode(fp, psTuple, psOnWire, iCommand, pcAilword)
int fp;
TpsPythonTree psTuple, psOnWire;
int iCommand;
char *pcAilword;
{

    xInitCodeGen(psTuple);
    xMarshalCode(fp, psOnWire);
    if (ErrCnt != 0 || iStop) return;
#ifdef STACK_TRACE
	mypf(OUT_HANDLE, "After generation%n");
	DUMPSTK();
#endif
    if (pcAilword != NULL) {
    	mypf(fp, "%c%b", (TscOpcode)AilWord, 
    	               (TscOperand)xlSetFlags(pcAilword));
    }
    mypf(fp, "%c%b", (TscOpcode)Trans, (TscOperand)iCommand);
    bp = 0;
    Mreinit(NULL);	/*   Fix   */
    xUnMarshalClCode(fp, psOnWire, psTuple);
}
#else
scclgen_dummy() {}
#endif /* PYTHON */
