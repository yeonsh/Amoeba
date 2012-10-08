/*	@(#)mkptree.c	1.2	94/04/07 14:33:55 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifdef PYTHON

#include "stubcode.h"

/*
**	In this file the data structure needed for the generation of the
**	stubcode is made from the itlist that Ail makes.
**
**	For an explanation of the data structure see the file "stubcode.h"
*/

static TpsPythonTree xpsChType(), xpsCreateTuple();
static struct itlist *psStart;

/*
 *   Because some items don't have a name or a name on a strange place
 *   and for debuggging it nice to have a good name for an object this
 *   function creates a nice name.
 */

static char *xpcNiceName(psFor)
struct itlist *psFor;
{
static char *pcNoName = "No name",
            *pcArray  = "Array";

    switch (psFor->il_type->td_kind) {

    case tk_tdef:
            return psFor->il_type->td_tname;
    case tk_arr:
            return pcArray;
    case tk_struct:
            return psFor->il_type->td_name;
    case tk_int:
    case tk_ref:
            return psFor->il_name;
    default:
            return pcNoName;
    }
}

static void xRecursFree(psList)
struct stind *psList;
{
struct stind *psTmp;

    while (psList != NULL) {
        psTmp = psList;
        psList = psList->st_nxt;
        FREE(psTmp->st_str);
        FREE(psTmp);
    }
}

/*
 *   Determine wether an array is of fixed or variable length.
 */

static int xiArrKind(psArr)
struct typdesc *psArr;
{
int iBound, iAct;
struct etree *psETreeAct;

    iBound = iAct =  -1;    /*  The sizes are not kown yet. */
    if (psArr->td_bound->et_const) {
        /*
         *   The boundery size is constant and known.
         */
        iBound = psArr->td_bound->et_val;
    }
    psETreeAct = psArr->td_act;
    if (psETreeAct != NULL) {
        /*
         *   There is an actuel size.
         */
        if (psETreeAct->et_const) {
            /*
             *   And it is constant.
             */
            iAct = psETreeAct->et_val;
         }
     }
     if (iAct > -1 && iBound > -1) return PF_FIXARR;
     return PF_VARARR;
}

static int xiSetListFlags(psFor)
struct itlist *psFor;
{

    return xiArrKind(psFor->il_type);
}

static int xiSetStrFlags(psFor)
struct itlist *psFor;
{

    if (psFor->il_type->td_kind == tk_char)
        /*
         *   This is a string of length one, so it is fixed.
         */
        return PF_FIXARR;
    return xiArrKind(psFor->il_type);
}

/*
 *   Create an integer object in the tree.
 */

static TpsPythonTree xpsCreateInt(psFrom, iWay)
struct itlist *psFrom;
int iWay;
{
TpsPythonTree psRet;

    /*
     *   Create an integer object and return it.
     */
    psRet = new(TsPythonTree);
    psRet->iType = pt_integer;
    psRet->iFlags = iWay;
    psRet->psNext = psRet->psPrim = NULL;
    psRet->psType = psFrom;
    psRet->psArrInfo = NULL;
    return psRet;
}

/*
 *   Create a list object in the tree.
 */

static TpsPythonTree xpsCreateList(psFrom, iWay)
struct itlist *psFrom;
int iWay;
{
TpsPythonTree psRet;
struct itlist *psNewItList;

    psRet = new(TsPythonTree);
    psRet->iType = pt_list;
    psRet->psType = psFrom;
    psRet->psArrInfo = NULL;
    psRet->psNext = NULL;
    psRet->iFlags = iWay | xiSetListFlags(psFrom);
    psNewItList = new(struct itlist);
    psNewItList->il_type = psFrom->il_type->td_prim;
    psNewItList->il_next = NULL;
    psNewItList->il_attr = NULL;
    psNewItList->il_name = xpcNiceName(psFrom);
    psNewItList->il_bits = psFrom->il_bits | AT_PYTHON;

    switch (psFrom->il_type->td_prim->td_kind) {
    /*
     *   The switch seems useless, but when the types will be expanded
     *   this is easier to adjust.
     */
    case tk_int:
            {
            TpsPythonTree psTmp;
        
                psTmp = new(TsPythonTree);
                psTmp->iType = pt_integer;
                psTmp->psType = psNewItList;
                psTmp->psArrInfo = NULL;
                psTmp->psNext = NULL;
                psTmp->psPrim = NULL;
                psTmp->iFlags = iWay | PF_INLIST;
                psRet->psPrim = psTmp;
            }
            break;
    default:
            iWay |= PF_INLIST;
            psRet->psPrim = xpsChType(psNewItList, iWay);
            
    }
    return psRet;
}

/*
 *   Create a string object in the tree.
 */

static TpsPythonTree xpsCreateString(psFrom, iWay)
struct itlist *psFrom;
int iWay;
{
TpsPythonTree psRet;

    psRet = new(TsPythonTree);
    psRet->iType = pt_string;
    psRet->psNext = NULL;
    psRet->iFlags = iWay | xiSetStrFlags(psFrom);
    psRet->psType = psFrom;
    psRet->psArrInfo = NULL;
    psRet->psPrim = NULL;
    return psRet;
}

/*
 *   Create a capability object in the tree.
 */

static TpsPythonTree xpsCreateCap(psFrom, iWay)
struct itlist *psFrom;
int iWay;
{
TpsPythonTree psRet;

    psRet = new(TsPythonTree);
    psRet->iType = pt_cap;
    psRet->psPrim = NULL;
    psRet->psType = psFrom;
    psRet->psArrInfo = NULL;
    psRet->iFlags = iWay;
    psRet->psNext = NULL;
    return psRet;
}

/*
 *   Create a tuple object in the tree if the tuple has more than
 *   one element. If it has exactly one element return the object of 
 *   that element, because there are no tuple of size one.
 */

static TpsPythonTree xpsCreateTuple(psFrom, iWay)
struct itlist *psFrom;
int iWay;
{
struct itlist *psFromStruct;

    psFromStruct = psFrom->il_type->td_meml;
    if (psFromStruct->il_next != NULL) {
    TpsPythonTree psRet;
    TpsPythonTree psTmp,psTmp2;
    
        psRet = new(TsPythonTree);
        psRet->iType = pt_tuple;
        psRet->iFlags = iWay;
        psRet->psType = psFrom;
        psRet->psArrInfo = NULL;
        psRet->psNext = NULL;
        iWay |= PF_INTUPLE;
        psTmp = psTmp2 = new(TsPythonTree);
        for (;psFromStruct != NULL;psFromStruct = psFromStruct->il_next) {
            if ((psTmp->psNext = xpsChType(psFromStruct, iWay)) != NULL) {
                psTmp = psTmp->psNext;
            }
        }
        psRet->psPrim = psTmp2->psNext;
        FREE(psTmp2);
        return psRet;
    }
    return xpsChType(psFromStruct, iWay);
}

/*
 *   Change the AIL type into the according Python type.
 */

static TpsPythonTree xpsChType(psType, iWay)
struct itlist *psType;
int iWay;
{

        switch (psType->il_type->td_kind) {
    
        case tk_int:
                return xpsCreateInt(psType, iWay);
        case tk_char:
                /*
                 *  There is no character object in python. A character
                 *  is a string with length one.
                 */
                return xpsCreateString(psType, iWay);
        case tk_struct:
                return xpsCreateTuple(psType, iWay);
        case tk_arr:
                /*
                 *   To determine which function to use ( Create list or 
                 *   Create string ) we have to find out if it's a
                 *   character array of not.
                 */
                if (psType->il_type->td_prim->td_kind == tk_char) {
                    return xpsCreateString(psType, iWay);
                }
                return xpsCreateList(psType, iWay);
         case tk_tdef:
                 /*
                  *   The capability typedef is a special object in Python,
                  *   so if it the typedef is a capability create a capability.
                  *   Otherwise change the primative type of the typedef
                  *   to the python type.
                  */
                 if (strcmp(psType->il_type->td_tname,"capability") == 0) {
                     return xpsCreateCap(psType, iWay);
                 }	
                 /* Fall through */
         case tk_ref:
                 /*
                  *   A reference does not exists as a Python type. 
                  *   For Python we use the type where the reference refers
                  *   to.
                  */
                 {
                 struct itlist *psTmp;
                 TpsPythonTree psTmp1;
             
                     psTmp = new(struct itlist);
                     psTmp->il_name = xpcNiceName(psType);
                     psTmp->il_next = NULL;
                     psTmp->il_attr = psType->il_attr;
                     psTmp->il_bits = psType->il_bits | AT_PYTHON;
                     psTmp->il_type = psType->il_type->td_prim;
                     psTmp1 = xpsChType(psTmp, iWay);
                     return psTmp1;
                 }
         default:
                mypf(ERR_HANDLE,"%wUnknown %s is an type %d for now.%n",
                                psType->il_name,
                                psType->il_type->td_kind);
                ShowType(psType->il_type);
                return NULL;
        }
}

/*
 *   xiGetWay is used to change the flags from the itlist structure
 *   to the according flag(s) for the PythonTree structure.
 */

static int xiGetWay(iBits)
int iBits;
{
int iRetBits = 0;

    if (iBits & AT_REQUEST)
        iRetBits |= PF_IN;
    if (iBits & AT_REPLY)
        iRetBits |= PF_OUT;
    return iRetBits;
}

/*
 *   GetFuncArgs gets the function arguments for the itlist and creates
 *   a PythonTree from them. Because parameters can go in as well as
 *   go out a flag is set for the way the parameter goes. This is done
 *   here because of a problem with structs, typedefs and lists. These types
 *   go in or out and not the members or the primative type. For the C
 *   generator this is no problem but for the stubcode generator it is
 *   because stubcode is generated for all members and primative types.
 *   So we need to known if a member or primative type goes in or out.
 */

static TpsPythonTree xpsGetFuncArgs(psFrom,psOp)
struct itlist *psFrom;
struct optr *psOp;
{
TpsPythonTree psRet;
TpsPythonTree psTmp,psTmp2;
struct itlist *psNew;

    psRet = new(TsPythonTree);
    psRet->iType = pt_arglist;
    psRet->iFlags = PF_IN | PF_OUT;    /*
                                        *   This looks silly but it is
                                        *   necessary for code generation.
                                        */
    psNew = new(struct itlist);
    psNew->il_name = psOp->op_name;
    psNew->il_type = psOp->op_in;
    psNew->il_next = NULL;
    psNew->il_attr = NULL;
    psNew->il_bits = AT_PYTHON;
    psRet->psType = psNew;
    psRet->psArrInfo = NULL;
    psRet->psNext = NULL;
    psTmp = psTmp2 = new(TsPythonTree);
    psTmp2->psNext = NULL;
    for (;psFrom != NULL;psFrom = psFrom->il_next) {
    int iWay;
    
            iWay = xiGetWay(psFrom->il_bits);
            if ((psTmp->psNext = xpsChType(psFrom, iWay)) != NULL) {
                psTmp = psTmp->psNext;
            }
    }
    psRet->psPrim = psTmp2->psNext;
    FREE(psTmp2);
    return psRet;
}

/*
 *   Check if the size of the array is given as an expression.
 */

static int xiIsExpression(psParList)
struct stind *psParList;
{

    if (psParList == NULL) {
        /*
         *   There are no parameters so it cann't be an expression.
         */
        return 0;
    }
    if (psParList->st_nxt == NULL) {
        /*
         *   There is only one parameter and this one can be calculated.
         */
        return 0;
    }
    /*
     *   To bad it must be an expression.
     */
    return PF_EXPR;
}

/*
 *   Make the link between to Python Tree entries.
 *   
 */

static void xMakeLink(psThis, psIn)
TpsPythonTree psThis, psIn;
{
TpsLink psNew;

    psNew = new(TsLink);
    psNew->psLink = psThis;
    if (psIn->psArrInfo == NULL) {
        psNew->psNext = NULL;
    } else {
        psNew->psNext = psIn->psArrInfo;
    }
    psIn->psArrInfo = psNew;
}

/*
 *   Check if this integer is used in this array. If this is the case
 *   then make a link, mark the integer and determine if it can be 
 *   calculated.
 */

static int xInArray(psInt, psArr)
TpsPythonTree psInt, psArr;
{
struct stind *psAct, *psBound;
int iRet;

    psAct = UsedIdents(psArr->psType->il_type->td_act);
    iRet = xiIsExpression(psAct);
    psBound = UsedIdents(psArr->psType->il_type->td_bound);
    iRet |= xiIsExpression(psBound);
    for (;psAct != NULL; psAct = psAct->st_nxt) {
        if (!strcmp(psInt->psType->il_name, psAct->st_str)) {
        int iTmp;
        
            iTmp = psArr->iFlags & (PF_IN | PF_OUT);
            iTmp &= psInt->iFlags;
            if (iTmp) {
                xMakeLink(psArr, psInt);
                xMakeLink(psInt, psArr);
                psInt->iFlags |= PF_ARRPAR;
                if ((iTmp & PF_IN) && !iRet) {
                    psInt->iFlags |= PF_CALC_IN;
                }
                if ((iTmp & PF_OUT) && !iRet) {
                    psInt->iFlags |= PF_CALC_OUT;
                }
            } else {
                mypf(ERR_HANDLE,"%w%s and %s don't go in the same direction%n",
                                psArr->psType->il_name,
                                psInt->psType->il_name);
            }
        }
    }
    for (;psBound != NULL; psBound = psBound->st_nxt) {
        if (!strcmp(psInt->psType->il_name, psBound->st_str)) {
        int iTmp;
        
            iTmp = psArr->iFlags & (PF_IN | PF_OUT);
            iTmp &= psInt->iFlags;
            if (iTmp) {
            Bool bLinked = No;
            TpsLink psWalk = psInt->psArrInfo;
            
                /*
                 *   Check if there is already a link from the integer
                 *   to the array.
                 */
                for (; psWalk != NULL; psWalk = psWalk->psNext) {
                    if (psWalk->psLink == psArr) {
                        bLinked = Yes;
                        break;
                    }
                }
                if (!bLinked) {
                    /*
                     *   No link: make it.
                     */
                    xMakeLink(psInt, psArr);
                    xMakeLink(psArr, psInt);
                }
                psInt->iFlags |= PF_ARRPAR;
                if ((iTmp & PF_IN) && !iRet) {
                    psInt->iFlags |= PF_CALC_IN;
                }
                if ((iTmp & PF_OUT) && !iRet) {
                    psInt->iFlags |= PF_CALC_OUT;
                }
            } else {
                mypf(ERR_HANDLE,"%w%s and %s don't go in the same direction%n",
                                psArr->psType->il_name,
                                psInt->psType->il_name);
            }
        }
    }
    xRecursFree(psAct);
    xRecursFree(psBound);
}

/*
 *  Search for an array where the integer is used as a size.
 */

static void xFindArray(psFor, psWhere)
TpsPythonTree psFor, psWhere;
{

    for (;psWhere != NULL; psWhere = psWhere->psNext) {
        if ((psWhere->iType == pt_string) && (psWhere->iFlags & PF_VARARR)) {
            xInArray(psFor, psWhere);
        }
        if ((psWhere->iType == pt_list) && (psWhere->iFlags & PF_VARARR)) {
            xInArray(psFor, psWhere);
            /*
             *   This is needed because list can have lists or strings as
             *   elements.
             */
            xFindArray(psFor, psWhere->psPrim);
        }
    }
}

/*
 *   This function makes links between arrays and the integers
 *   that give the size.
 */

static void xSetArrayPar(psTree)
TpsPythonTree psTree;
{
TpsPythonTree psWalk;

    psWalk = psTree->psPrim;
    for (;psWalk != NULL; psWalk = psWalk->psNext) {
        if (psWalk->iType == pt_integer) {
            /*
             *   The parameter is an integer and is used as array size.
             */
            xFindArray(psWalk, psTree->psPrim);
        } else if (psWalk->iType == pt_tuple) {
            /*
             *   It is a tuple, repeat this procedure for the parameters in
             *   the tuple.
             */
            xSetArrayPar(psWalk);
        }
    }
}

/*
**	This function starts the generation of the Python tree and after this
**	it starts the function that makes the link between the arrays and
**	there parameters.
*/
TpsPythonTree xpsOrderArgs(psOp,psIt)
struct optr *psOp;
struct itlist *psIt;
{
TpsPythonTree psRet;

    psStart = psIt;
    psRet = xpsGetFuncArgs(psIt, psOp);
    psRet->psNext = NULL;
    xSetArrayPar(psRet);
    return psRet;
}
#else
mkptree_dummy() {}
#endif /* PYTHON */
