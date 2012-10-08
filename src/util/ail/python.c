/*	@(#)python.c	1.2	94/04/07 14:37:52 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifdef PYTHON
/*
**	In this file the generators for the stubcode are defined.
*/

#include "stubcode.h"
#include "sc_global.h"

static Bool bReqAddr, bReplAddr;
static struct etree *psReqAlloc, *psReplAlloc;
static AllocWay;

/*
 *   The buffersize for the buffer needed for the RPC. This code is
 *   nearly the same as the code for the other generators.
 */

static void xDeclStcBuf(fp, psOp)
int fp;
struct optr *psOp;
{
struct itlist *psArgs;
int iScratch;

    psArgs=psOp->op_client;
    if(psOp->op_in->td_esize==NULL){
mypf(ERR_HANDLE,"%r Actual in size for %s unknown; too bad\n",psOp->op_name);
        return;
    }
    if(psOp->op_out->td_esize==NULL){
mypf(ERR_HANDLE,"%r Actual out size for %s unknown; too bad\n",psOp->op_name);
        return;
    }

#ifdef NOTDEF
    if(psOp->op_in->td_msize==NULL){
mypf(ERR_HANDLE,"%r Max in size for %s unknown; too bad\n",psOp->op_name);
        return;
    }
    if(psOp->op_out->td_msize==NULL){
mypf(ERR_HANDLE,"%r Max out size for %s unknown; too bad\n",psOp->op_name);
        return;
    }
#endif

    bReplAddr = bReqAddr = No;	/* Default: no & needed */
    /*
     *	For the reply message:
     */
    
    if (psOp->op_ob == 0) {	/* No out-parameters in the buffer	*/
	psReplAlloc = Cleaf((long) 0);
    } else {
	psReplAlloc = psOp->op_out->td_msize;
	if (psReplAlloc == NULL) psReplAlloc = psOp->op_out->td_esize;
    }

    if (psOp->op_ib == 0) {	/* No in-parameters in the buffer	*/
	psReqAlloc = Cleaf((long) 0);
    } else {
	if (psReplAlloc->et_const) {
	    /*
	     *	Replysize is constant; try to use a
	     *	constant requestsize as well
	     */
	    if (psOp->op_in->td_msize != NULL
	    && psOp->op_in->td_msize->et_const)	/* Max is constant	*/
		psReqAlloc = psOp->op_in->td_msize;
	    else
		psReqAlloc = psOp->op_in->td_esize;	/* Use the actual size for in */
	} else {
	    /* Replysize is dynamic; can as well use actual size for alloc */
	    psReqAlloc = psOp->op_in->td_esize;
	}
    }

    /*
     *	At this point psReplAlloc and psReqAlloc are zero (!= NULL!!) if
     *	the buffer can be "borrowed" from the caller, or there
     *	are no buffer parameters that travel the associated way.
     *	This is (and that's no coincidence) exactly the information
     *	that VarBuf() expects.
     *	Now it's time to determine the way we allocate a buffer.
     */
    AllocWay = B_NOBUF;			/* Be optimistic		*/
    if (!IsZero(psReqAlloc)) {		/* No buffer for request	*/
	if (psReqAlloc->et_const) AllocWay = B_STACK;
	else AllocWay = dynamic;	/* Either malloc or alloca	*/
    }
    if (!IsZero(psReplAlloc) && AllocWay != dynamic) {
	if (psReplAlloc->et_const) AllocWay = B_STACK;
	else AllocWay = dynamic;
    }

    /* Arrange a buffer, declare _adv	*/
    switch (AllocWay) {
        int ReqCon, ReplCon;	/* -1: not constant, 0: not there	*/

    case B_NOBUF:
	/* No other declarations need to be made. */
	assert(IsZero(psReqAlloc));
	assert(IsZero(psReplAlloc));
	mypf(fp, "%c%b", (TscOpcode)BufSize, (TscOperand)0);
	break;
    case B_STACK:
	/* Compute max(request, reply)	*/
	if (!IsZero(psReqAlloc)) {
	    assert(psReqAlloc->et_const);
	    iScratch = psReqAlloc->et_val;
	} else {
	    iScratch = 0;
	}
	if (!IsZero(psReplAlloc)) {
	    assert(psReplAlloc->et_const);
	    if (psReplAlloc->et_val > iScratch) iScratch = psReplAlloc->et_val;
	}
	mypf(fp, "%c%b", (TscOpcode)BufSize, (TscOperand)iScratch);
	break;
    case B_MALLOC:
    case B_ALLOCA:
	assert(psReqAlloc != NULL);
        assert(psReplAlloc != NULL);
        if (psReqAlloc->et_const) ReqCon = psReqAlloc->et_const;
        else ReqCon = -1;
        if (psReplAlloc->et_const) ReplCon = psReplAlloc->et_const;
        else ReplCon = -1;
        assert(!(ReqCon>0 && ReplCon>0));	/* At least 1 dynamic	*/
	/*
	 *   In stubcode it is impossible to compute the dynamic size of 
	 *   the buffer. So if this happens use the maximum allowed
	 *   size of the buffer and give a warning.
	 */
	mypf(fp, "%c%b", (TscOpcode)BufSize, (TscOperand)30000);
	mypf(ERR_HANDLE,"%w Can't compute buffersize. Using 30000%n");
	break;
    case B_ERROR:
	mypf(ERR_HANDLE, "%r no memory allocator defined\n");
	break;
    default:
	assert(0);
    }
    assert(psReplAlloc != NULL);
    assert(psReqAlloc != NULL);
} /* DeclBuf */

/*
**	The next two functions free the memory allocated for the python
**	tree
*/

static void xFreeLink(psLink)
TpsLink psLink;
{

    if (psLink->psNext != NULL) {
        xFreeLink(psLink->psNext);
    }
    FREE(psLink);
}

static void xFreeTree(psTree)
TpsPythonTree psTree;
{

    if (psTree->psNext != NULL) xFreeTree(psTree->psNext);
    if (psTree->psPrim != NULL) xFreeTree(psTree->psPrim);
    if (psTree->psArrInfo != NULL) xFreeLink(psTree->psArrInfo);
    if (psTree->psType->il_bits & AT_PYTHON) FREE(psTree->psType);
    FREE(psTree);
}

/*
 *   Make a header for the stubcode file.
 */

static void xMakeHeader(fp, psOp)
handle fp;
struct optr *psOp;
{

    /*
     *   The header consists of one magic word for now.
     */
    mypf(fp, "%b", (TscOperand)SC_MAGIC);
}

/*
 *    Generate client stubcode.
 */

static void xPythonClOptr(fp, psOp)
handle fp;
struct optr *psOp;
{
int iFlags;
TpsPythonTree psPythonArgs, psOnWire;

    ThisOptr=psOp;
    iFlags=GetFlags(psOp->op_val);
    ClientAnalysis(psOp);
    xMakeHeader(fp, psOp);
    xDeclStcBuf(fp,psOp);
    if(AllocWay==B_ERROR) return;
    assert(psOp->op_flags & OP_CLIENT);
    assert(psOp->op_flags & OP_STARRED);
    Minit(NULL);
    psPythonArgs=xpsOrderArgs(psOp,psOp->op_order);
    if(ErrCnt != 0) return;
    psOnWire=xpsOrderArgs(psOp,psOp->op_args);
    if(ErrCnt != 0) return;
#ifdef PDEBUG
    xDumpTree(psPythonArgs);
    xDumpTree(psOnWire);
#endif
    xGenClCode(fp, psPythonArgs, psOnWire, psOp->op_val, psOp->op_ailword);
    xFreeTree(psPythonArgs);
    xFreeTree(psOnWire);
}

static void xPythonG(pcName, psOp)
char *pcName;
struct optr *psOp;
{
char acFName[256];
handle fp;

    if(psOp==NULL){
        mypf(ERR_HANDLE,"%r Unknown operator %s%n",pcName);
    }else if(psOp->op_flags & OP_STARRED){
        sprintf(acFName,"%s.sc",psOp->op_name);
        fp=OpenFile(acFName);
        if (ErrCnt != 0) return;
        xPythonClOptr(fp, psOp);
        CloseFile(fp);
    }else{
        mypf(ERR_HANDLE,"%r No '*' parameter defined.");
    }
}

void xPythonClientGen(psArgs)
struct gen *psArgs;
{
struct name *psNm;

    
    if (psArgs != NULL) {
    	FindOptrs(psArgs, 0, xPythonG);
    } else {
        for (psNm = ThisClass->cl_scope; psNm != NULL; psNm = psNm->nm_next) {
            if (psNm->nm_kind == CALL) {
                xPythonG(psNm->nm_name, psNm->nm_optr);
            }
            if (ErrCnt) break;
        }
    }
}

void AllPythonStubs(psArgs)
struct gen *psArgs;
{
struct clist *clp;

    for (clp = ThisClass->cl_effinh; clp != NULL; clp = clp->cl_next) {
        ThisClass = clp->cl_this;
        xPythonClientGen(psArgs);
        if (ErrCnt) break;
    }
}

/*
**	Generate the stubcode document that shows how the stubs can be
**	used in Python.
*/

static void xPythonClOptrDoc(fp, psOp)
handle fp;
struct optr *psOp;
{
int iFlags;
TpsPythonTree psPythonArgs;

    if(ErrCnt!=0) return;
    ThisOptr=psOp;
    iFlags=GetFlags(psOp->op_val);
    ClientAnalysis(psOp);
    assert(psOp->op_flags & OP_CLIENT);
    assert(psOp->op_flags & OP_STARRED);
    Minit(NULL);
    psPythonArgs=xpsOrderArgs(psOp,psOp->op_order);
    if(ErrCnt != 0) return;
#ifdef PDEBUG
    xDumpTree(psPythonArgs);
#endif
    xGenDoc(fp, psPythonArgs);
    xFreeTree(psPythonArgs);
}

void xPythonDocGen(psArgs)
struct gen *psArgs;
{
struct name *psNm;
char acFName[256];
handle fp;

    sprintf(acFName, "%s.scdoc", ThisClass->cl_name);
    fp = OpenFile(acFName);
    /*
     *   Generate the document for all stubs in this class.
     *   Do this in file <classname>.scdoc
     */
    for (psNm = ThisClass->cl_scope;psNm != NULL;psNm = psNm->nm_next) {
        if (psNm->nm_kind == CALL) {
            xPythonClOptrDoc(fp, psNm->nm_optr);
            if(ErrCnt) break;
        }
    }
    CloseFile(fp);
}

void AllPythonDocs(psArgs)
struct gen *psArgs;
{
struct clist *cl;

    for (cl = ThisClass->cl_effinh; cl != NULL; cl = cl->cl_next) {
        ThisClass = cl->cl_this;
        xPythonDocGen(psArgs);
    }
}

void xPythonServerHead(fp)
handle fp;
{

}

void xPythonServerTail(fp)
handle fp;
{

}

void xPythonServer(fp, classname, monitor)
handle fp;
struct name *classname;
Bool monitor;
{
struct name *walk;

    for (walk = classname; walk != NULL; walk = walk->nm_next) {
    	if (walk->nm_kind == CALL) {
    	    mypf(fp, "Name : %s%n", walk->nm_name);
    	}
    }
}

void xPythonSvGen(psArgs)
struct gen *psArgs;
{
char acFName[256];
handle fp;
struct clist *clp;

    mypf(ERR_HANDLE, 
    	"%wNo Python support for server code at this moment. Sorry...%n");
#if 0
    sprintf(acFName, "ml_%s.sc", ThisClass->cl_name);
    fp = OpenFile(acFName);
    if (ErrCnt) return;
    xMakeHeader(fp);
    xPythonServerHead(fp);
    xPythonServer(fp, ThisClass->cl_scope);
    for (clp = ThisClass->cl_effinh; clp != NULL; clp = clp->cl_next) {
    	xPythonServer(fp, clp->cl_this->cl_scope);
    }
    xPythonServerTail(fp);
    CloseFile(fp);
#endif
}
#else
python_dummy() { }
#endif /* PYTHON */
