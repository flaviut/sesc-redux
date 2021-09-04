/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "SMPDebug.h"
#include "SMPMemRequest.h"
#include "SMPCacheState.h"
#include "libcore/MemObj.h"

pool<SMPMemRequest> SMPMemRequest::rPool(256, "SMPMemRequest");
//pool<SMPMemRequest> SMPMemRequest::rPool(256);

extern char* MemOperationStr[];
SMPMemRequest::MESHCNT SMPMemRequest::nSMPMsg;
#if (defined SIGDEBUG)
SMPMemRequest::MESHCNT SMPMemRequest::nDEBUGSMPMsg;
#endif

SMPMemRequest::SMPMemRequest()
    : MemRequest()
{
#if 0
    // JJO
    nSMPMsg[MeshDirRequest]      	= new GStatsCntr("MeshMsg:DirRequest");
    nSMPMsg[MeshDirReply]    		= new GStatsCntr("MeshMsg:DirReply");
    nSMPMsg[MeshDirUpdate]		= new GStatsCntr("MeshMsg:DirUpdate");
    nSMPMsg[MeshDirUpdateAck]		= new GStatsCntr("MeshMsg:DirUpdateAck");
    nSMPMsg[MeshReadDataRequest]	= new GStatsCntr("MeshMsg:ReadDataRequest");
    nSMPMsg[MeshReadDataReply]	= new GStatsCntr("MeshMsg:ReadDataReply");
    nSMPMsg[MeshMemRequest]		= new GStatsCntr("MeshMsg:MemRequest");
    nSMPMsg[MeshMemReply]			= new GStatsCntr("MeshMsg:MemReply");
    nSMPMsg[MeshMemAccess]		= new GStatsCntr("MeshMsg:MemAccess");
    nSMPMsg[MeshMemAccessReply]	= new GStatsCntr("MeshMsg:MemAccessReply");
    nSMPMsg[MeshMemWriteBack]		= new GStatsCntr("MeshMsg:MemWriteBack");
    nSMPMsg[MeshMemPush]			= new GStatsCntr("MeshMsg:MemPush");
    nSMPMsg[MeshInvRequest]		= new GStatsCntr("MeshMsg:InvRequest");
    nSMPMsg[MeshInvReply]			= new GStatsCntr("MeshMsg:InvReply");
    nSMPMsg[MeshInvDataReply]		= new GStatsCntr("MeshMsg:InvDataReply");
    nSMPMsg[MeshInvDirUpdate]		= new GStatsCntr("MeshMsg:InvDirUpdate");
    nSMPMsg[MeshInvDirAck]		= new GStatsCntr("MeshMsg:InvDirAck");
    nSMPMsg[MeshWriteDataRequest]	= new GStatsCntr("MeshMsg:WriteDataRequest");
    nSMPMsg[MeshNOP]				= new GStatsCntr("MeshMsg:NOP");
#endif
}

SMPMemRequest *SMPMemRequest::duplicate() {
    if(oreq==NULL) {
        SMPMemRequest *r = SMPMemRequest::create(requestor, pAddr, memOp, false, NULL, meshOp);

        if(msgOwner==NULL) {
            r->msgOwner = requestor;
        } else {
            r->msgOwner = msgOwner;
        }

        getDstNodes(r->dst);
        getDstObjs(r->dstObj);

        nSMPMsg[meshOp]--;
        return r;
    } else {
        SMPMemRequest *r = SMPMemRequest::create(this, requestor, meshOp);
        getDstNodes(r->dst);
        getDstObjs(r->dstObj);

        nSMPMsg[meshOp]--;
        return r;
    }
}

SMPMemRequest *SMPMemRequest::create(SMPMemRequest *sreq,
                                     MeshOperation msh)
{
    IJ(msh==Invalidation);
    SMPMemRequest *nsreq = create(sreq, sreq->requestor, msh);
    return nsreq;
}

SMPMemRequest *SMPMemRequest::create(SMPMemRequest *sreq,
                                     MemObj *reqCache,
                                     MeshOperation msh)
{
    SMPMemRequest *nsreq = create(sreq->getOriginalRequest(), reqCache, true, msh);
    nsreq->msgOwner = sreq->msgOwner;
    return nsreq;
}

#if (defined DEBUG_SMPREQ)
std::list<SMPMemRequest*> outStandingSMPReq;
#endif

SMPMemRequest *SMPMemRequest::create(MemRequest *mreq,
                                     MemObj *reqCache,
                                     bool sendData, MeshOperation msh)
{
    SMPMemRequest *sreq = rPool.out();
#if (defined DEBUG_SMPREQ)
	outStandingSMPReq.push_back(sreq);
#endif

    I(mreq);
    sreq->oreq = mreq;
    IS(sreq->acknowledged = false);
    I(sreq->memStack.empty());

    IJ(sreq->memStack.empty());
    IJ(sreq->clockStack.empty());

    sreq->currentClockStamp = globalClock;

    sreq->pAddr = mreq->getPAddr();
    sreq->memOp = mreq->getMemOperation();
    // JJO
    sreq->meshOp = msh;
    sreq->writeBack = false;

    sreq->dataReq = mreq->isDataReq();
    sreq->prefetch = mreq->isPrefetch();

    sreq->state = SMP_INVALID;

    I(reqCache);
    sreq->requestor = reqCache;
    sreq->supplier = 0;
    sreq->currentMemObj = reqCache;

    sreq->writeDown = false;
    sreq->needData = sendData;

    if(sreq->memOp == MemPush)
        sreq->needSnoop = false;
    else
        sreq->needSnoop = false;
    //sreq->needSnoop = true;

    sreq->found = false;
    sreq->nUses = 1;

    sreq->cb = 0;

    sreq->src = reqCache->getNodeID();
    sreq->dst.clear();
    sreq->dstObj.clear();
    sreq->dentry = NULL;
    //printf("%s src %d\n",reqCache->getSymbolicName(), sreq->src);

    IJ(msh!=NOP);
    //printf("%p (%s)\n", sreq, reqCache->getSymbolicName());
    nSMPMsg[msh]++;
#if (defined SIGDEBUG)
    nDEBUGSMPMsg[msh]++;
#endif
    sreq->invCB = NULL;
    //sreq->dead = false;
	
	sreq->clearValue();
#if 0
    sreq->saveReq = NULL;
	sreq->L2Miss = false;
	sreq->fallBack = false;
	sreq->estTLLat = -1;
	sreq->estNOCLat = -1;
	sreq->sentMethod = SM_NA;
	sreq->hops = -1;
	sreq->TLhops = -1;
	sreq->e[0] = -1;
	sreq->e[1] = -1;
	sreq->e[2] = -1;
#endif
    DEBUGPRINT("\t\t +++create %s at %lld (%p)\n", SMPMemReqStrMap[msh], globalClock, sreq);
    return sreq;
}

void SMPMemRequest::clearValue() 
{
    hops = -1;
	plat = -1;
    routerTime = 0;

    saveReq = NULL;
}

SMPMemRequest *SMPMemRequest::create(MemObj *reqCache,
                                     PAddr addr,
                                     MemOperation mOp,
                                     bool needsWriteDown,
                                     CallbackBase *cb, MeshOperation msh)
{
    SMPMemRequest *sreq = rPool.out();
#if (defined DEBUG_SMPREQ)
	outStandingSMPReq.push_back(sreq);
#endif

    sreq->oreq = 0;
    IS(sreq->acknowledged = false);
    I(sreq->memStack.empty());

    sreq->currentClockStamp = globalClock;

    sreq->pAddr = addr;
    sreq->memOp = mOp;
    // JJO
    sreq->meshOp = msh;
    sreq->writeBack = false;
    sreq->cb = cb;

    sreq->dataReq = false;
    sreq->prefetch = false;

    sreq->state = SMP_INVALID;

    I(reqCache);
    sreq->requestor = reqCache;
    sreq->supplier = 0;
    sreq->currentMemObj = reqCache;

    sreq->writeDown = needsWriteDown;
    sreq->needData = false; // TODO: check this (should it be true?)

    if(sreq->memOp == MemPush)
        sreq->needSnoop = false;
    else
        sreq->needSnoop = true;

    sreq->found = false;
    sreq->nUses = 1;

    sreq->src = reqCache->getNodeID();
    sreq->dst.clear();
    sreq->dstObj.clear();
    sreq->dentry = NULL;
    
	IJ(msh!=NOP);
    //printf("%p (%s)\n", sreq, reqCache->getSymbolicName());
    nSMPMsg[msh]++;
#if (defined SIGDEBUG)
    nDEBUGSMPMsg[msh]++;
#endif
    sreq->invCB = NULL;
    //sreq->dead = false;

	sreq->clearValue();

#if 0
    sreq->hops = -1;
	sreq->plat = -1;
    sreq->routerTime = 0;
	sreq->TLBeginTime = 0;
	sreq->msgClass = -1;
	sreq->TLQLen = -1;
	sreq->sentMethod = SM_NA;
	sreq->sentFB = false;
	sreq->estTLLat = -1;
	sreq->estNOCLat = -1;
	sreq->e[0] = -1;
	sreq->e[1] = -1;
	sreq->e[2] = -1;

    sreq->saveReq = NULL;
	sreq->L2Miss = false;
	sreq->fallBack = false;
#endif

    DEBUGPRINT("\t\t +++create %s at %lld (%p)\n", SMPMemReqStrMap[msh], globalClock, sreq);
    return sreq;
}

void SMPMemRequest::incUses()
{
    nUses++;
}

#if 0
void SMPMemRequest::destroy()
{
    IJ(0);
    nUses--;
    if(!nUses) {

        GLOG(SMPDBG_MSGS, "sreq %p real destroy", this);
        I(memStack.empty());
        while(!memStack.empty()) {
            memStack.pop();
        }
        rPool.in(this);
        return;
    }

    GLOG(SMPDBG_MSGS, "sreq %p fake destroy", this);
}
#endif

void SMPMemRequest::destroy()
{
#if (defined SIGDEBUG)
    nDEBUGSMPMsg[meshOp]--;
#endif
    DEBUGPRINT("\t\t +++delete %s at %lld (%p)\n", SMPMemReqStrMap[meshOp], globalClock, this);

    nUses = 0;
    GLOG(SMPDBG_MSGS, "sreq %p real destroy", this);
    while(!memStack.empty()) {
        memStack.pop();
    }
    while(!clockStack.empty()) {
        clockStack.pop();
    }
    dst.clear();
    dstObj.clear();
    dentry = NULL;
    meshOp=NOP;
    //saveMeshOp=NOP;
    saveReq = NULL;
    msgDst=NULL;
    msgOwner=NULL;
    //dead = true;
    //saveisSnoop = false;
    //isOwner = false;
    isExcl = false;
    
	routerTime = 0;

    if(invCB!=NULL) {
        //printf("shit?\n");
        invCB->destroy();
    }
    rPool.in(this);
#if (defined DEBUG_SMPREQ)
	outStandingSMPReq.remove(this);
#endif
    return;
}

VAddr SMPMemRequest::getVaddr() const
{
    I(0);

    if(oreq)
        return oreq->getVaddr();

    return 0;
}

PAddr SMPMemRequest::getPAddr() const
{
    return pAddr;
}

void SMPMemRequest::ack(TimeDelta_t lat)
{
    I(memStack.empty());
    I(acknowledged == false);
    IS(acknowledged = true);
    I(lat == 0);

    if (cb==0) {
        destroy();
        return; // avoid double ack
    }

    CallbackBase *ncb=cb;
    cb = 0;
    ncb->call();
    destroy();
}

void SMPMemRequest::setState(uint32_t st)
{
    state = st;
}

void SMPMemRequest::setSupplier(MemObj *supCache)
{
    supplier = supCache;
}

MemRequest *SMPMemRequest::getOriginalRequest()
{
    return oreq;
}

MemOperation SMPMemRequest::getMemOperation()
{
    return memOp;
}

uint32_t SMPMemRequest::getState()
{
    return state;
}

MemObj *SMPMemRequest::getRequestor()
{
    return requestor;
}

MemObj *SMPMemRequest::getSupplier()
{
    return supplier;
}

void SMPMemRequest::setDirInfo(DirectoryEntry *de) {
    dentry = de;
}

#if 0
void SMPMemRequest::setDstInfo(DirectoryEntry *de) {
    switch(getMemOperation()) {
    case MemRead:
        break;
    case MemReadW:
    case MemWrite:
        break;
    case MemPush:
        break;
    }
}
#endif

