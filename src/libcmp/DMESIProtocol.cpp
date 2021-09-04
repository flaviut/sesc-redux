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

#include "DMESIProtocol.h"

#if (defined RCHECK)
#include "libmem/Cache.h"
std::map<PAddr, Time_t> Cache::rmap[100];
#endif

const char* MemOperationStr[] = {
    stringify(MemRead),
    stringify(MemWrite),
    stringify(MemPush),
    stringify(MemReadW)
};

const char* DirStatusStr[] = {
    stringify(EXCLUSIVE),
    stringify(SHARED),
    stringify(UNOWNED)
};


DMESIProtocol::DMESIProtocol(SMPCache *cache, const char *name)
    : SMPProtocol(cache)
{
    protocolType = DMESI_Protocol;
}

DMESIProtocol::~DMESIProtocol()
{
    // nothing to do
}

void DMESIProtocol::changeState(Line *l, unsigned newstate)
{
    // should use invalidate interface
    I(newstate != DMESI_INVALID);
#if 0
    ID(unsigned currentstate);
    IS(currentstate = l->getState());
    GI(currentstate == DMESI_INVALID,      newstate == DMESI_TRANS_RSV);
    GI(currentstate == DMESI_TRANS_RSV,    newstate == DMESI_TRANS_RD ||
       newstate == DMESI_TRANS_WR);
    GI(currentstate == DMESI_TRANS_RD,     newstate == DMESI_SHARED ||
       newstate == DMESI_TRANS_RD_MEM);
    GI(currentstate == DMESI_TRANS_RD_MEM, newstate == DMESI_EXCLUSIVE);
    GI(currentstate == DMESI_TRANS_WR,     newstate == DMESI_MODIFIED ||
       newstate == DMESI_TRANS_WR_MEM);
    GI(currentstate == DMESI_TRANS_WR_MEM, newstate == DMESI_MODIFIED);
    GI(currentstate == DMESI_EXCLUSIVE,    newstate == DMESI_SHARED ||
       newstate == DMESI_MODIFIED ||
       newstate == DMESI_TRANS_INV);
    GI(currentstate == DMESI_SHARED,       newstate == DMESI_SHARED ||
       newstate == DMESI_MODIFIED ||
       newstate == DMESI_TRANS_INV ||
       newstate == DMESI_TRANS_WR);
    GI(currentstate == DMESI_MODIFIED,     newstate == DMESI_MODIFIED ||
       newstate == DMESI_SHARED ||
       newstate == DMESI_TRANS_INV_D);
#endif
    //printf("prev %x new %x\n", currentstate, newstate);
    l->changeStateTo(newstate);
}

void DMESIProtocol::makeDirty(Line *l)
{
    I(l);
    I(l->isValid());

    changeState(l, DMESI_MODIFIED);
}

// preserves the dirty state while the cache
// is being invalidated in upper levels
void DMESIProtocol::preInvalidate(Line *l)
{
    IJ(l);
    IJ(l->isValid());

    if(l->isDirty())
        changeState(l, DMESI_TRANS_INV_D);
    else
        changeState(l, DMESI_TRANS_INV);
#if 0
    I(l);
    I(l->isValid());

    if(l->isDirty())
        changeState(l, DMESI_TRANS_INV_D);
    else
        changeState(l, DMESI_TRANS_INV);
#endif
}

void DMESIProtocol::freeResource(MemRequest *mreq) {
    PAddr addr = mreq->getPAddr();
    Line *l = pCache->getLine(addr);
    if(l) {
		changeState(l, DMESI_TRANS_INV);
        l->invalidate();
    }
}

void DMESIProtocol::read(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

    // time for this has already been accounted in SMPCache::read
    Line *l = pCache->getLine(addr);

    // if line is in transient state, read should not have been called
    GI(l, !l->isLocked());

    if(!l)
        l = pCache->allocateLine(addr, doReadCB::create(this, mreq));

    if(!l) {
        // not possible to allocate a line, will be called back later
        DEBUGPRINT("-----------------%s cache line allocation fail \n", pCache->getSymbolicName());
        return;
    }

    changeState(l, DMESI_TRANS_RSV);
    doRead(mreq);
}

void DMESIProtocol::doRead(MemRequest *mreq)
{
    Line *l = pCache->getLine(mreq->getPAddr());
    I(l);

    GI(l->isLocked(), l->getState() == DMESI_TRANS_RSV);

    // go into transient state and send request out
    changeState(l, DMESI_TRANS_RD);

    sendReadMiss(mreq);
}

void DMESIProtocol::write(MemRequest *mreq)
{
    Line *l = pCache->getLine(mreq->getPAddr());

    // if line is in transient state, write should not have been called
    //GIJ(l, !l->isLocked() && l->getState() == DMESI_SHARED);

    if (mreq->getMemOperation() != MemReadW)
        mreq->mutateWriteToRead();
    // hit in shared state
    // Doesn't matter. go to directory
    if (l) {
        //if(!l->canBeWritten()) {
        if(l->getState() == DMESI_SHARED /*|| l->getState() == DMESI_OWNER*/) {
            DEBUGPRINT("   [%s] write miss in shared state %x at %lld\n",
                       pCache->getSymbolicName(), mreq->getPAddr(), globalClock);
            changeState(l, DMESI_TRANS_WR);
            //sendInvalidate(mreq);
            //sendWriteMiss(mreq);
            doSendUpgradeMiss(mreq);
            return;
        } else if(l->canBeWritten()) {
            DEBUGPRINT("   [%s] write changed to hit (was in shared state) %x at %lld\n",
                       pCache->getSymbolicName(), mreq->getPAddr(), globalClock);
            pCache->doWriteAgain(mreq);
            return;
        } else {
            DEBUGPRINT("   [%s] write messy miss state %x (was in shared state) %x at %lld\n",
                       pCache->getSymbolicName(), l->getState(), mreq->getPAddr(), globalClock);

            doWriteCheckCB::scheduleAbs(globalClock+1, this, mreq);
            return;
        }
    }

    // miss - check other caches
    GIJ(l, !l->isValid());

    l = pCache->allocateLine(mreq->getPAddr(), doWriteCB::create(this, mreq));


    if(!l) {
        // not possible to allocate a line, will be called back later
        DEBUGPRINT("-----------------%s cache line allocation fail for %x at %llu \n",
                   pCache->getSymbolicName(), mreq->getPAddr(), globalClock);
        return;
    }

    changeState(l, DMESI_TRANS_RSV);
    doWrite(mreq);

#if 0
    Line *l = pCache->getLine(mreq->getPAddr());

    // if line is in transient state, write should not have been called
    GI(l, !l->isLocked() && l->getState() == DMESI_SHARED);

    // hit in shared state
    if (l && !l->canBeWritten()) {
        changeState(l, DMESI_TRANS_WR);
        sendInvalidate(mreq);
        return;
    }

    // miss - check other caches
    GI(l, !l->isValid());
    if (mreq->getMemOperation() != MemReadW)
        mreq->mutateWriteToRead();

    l = pCache->allocateLine(mreq->getPAddr(), doWriteCB::create(this, mreq));


    if(!l) {
        // not possible to allocate a line, will be called back later
        return;
    }

    changeState(l, DMESI_TRANS_RSV);
    doWrite(mreq);
#endif
}

// Check until line of interest is freed (not transitive state)
void DMESIProtocol::doWriteCheck(MemRequest *mreq) {
    DEBUGPRINT("-----------------%s write check retry for %x at %llu \n",
               pCache->getSymbolicName(), mreq->getPAddr(), globalClock);

    Line *l = pCache->getLine(mreq->getPAddr());
    if(l && (l->getState()&DMESI_TRANS)) {
        Time_t nextTry = globalClock+1;
        doWriteCheckCB::scheduleAbs(nextTry, this, mreq);
        return;
    }
    write(mreq);
}

void DMESIProtocol::doWrite(MemRequest *mreq)
{
    Line *l = pCache->getLine(mreq->getPAddr());

    IJ(l);
    GIJ(l->isLocked(), l->getState() == DMESI_TRANS_RSV);

    changeState(l, DMESI_TRANS_WR);

    sendWriteMiss(mreq);
}

void DMESIProtocol::sendReadMiss(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();
    doSendReadMiss(mreq);
}


void DMESIProtocol::doSendReadMiss(MemRequest *mreq)
{
    I(pCache->getLine(mreq->getPAddr()));
    PAddr addr = mreq->getPAddr();
    SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true, ReadRequest);
    sreq->addDstNode(pCache->getHomeNodeID(addr));
    sreq->msgOwner = pCache;

    //printf("R\t%5d\t%10x\t%10x\t%5d\t%lld\n", pCache->getNodeID(), addr, pCache->calcTag(addr), pCache->getHomeNodeID(addr), globalClock);

    DEBUGPRINT("   [%s] read miss (%s) send to %d from %d on %x at %lld (0x%lx) (o: %p)\n", pCache->getSymbolicName(),
               MemOperationStr[sreq->getMemOperation()], pCache->getHomeNodeID(addr),
               pCache->getNodeID(), addr, globalClock, (uint64_t)sreq, mreq);
    //int32_t dstNode = getNodeID(addr, pCache);
    //sreq->dst.insert(dstNode);

    //if(dstNode == pCache->getNodeID()) {
//	  return;
    //}

    DEBUGPRINT("   [%s] read debuggin timestamp for %x at %lld\n", pCache->getSymbolicName(), addr, globalClock);

    pCache->sendBelow(sreq);
}

void DMESIProtocol::sendWriteMiss(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();
    doSendWriteMiss(mreq);
}

void DMESIProtocol::doSendUpgradeMiss(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

    IJ(pCache->getLine(mreq->getPAddr()));

    SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true, UpgradeRequest);

    DEBUGPRINT("   [%s] upgrade miss (%s) send to %d from %d on %x at %lld (o: %p)\n", pCache->getSymbolicName(),
               MemOperationStr[sreq->getMemOperation()], pCache->getHomeNodeID(addr),
               pCache->getNodeID(), mreq->getPAddr(), globalClock, mreq);
    sreq->addDstNode(pCache->getHomeNodeID(addr));
    sreq->msgOwner = pCache;

    DEBUGPRINT("   [%s] upgrd debuggin timestamp for %x at %lld\n", pCache->getSymbolicName(), addr, globalClock);

    pCache->sendBelow(sreq);
}

void DMESIProtocol::doSendWriteMiss(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

    IJ(pCache->getLine(mreq->getPAddr()));

    SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true, WriteRequest);

    DEBUGPRINT("   [%s] write miss (%s) send to %d from %d on %x at %lld (o: %p)\n", pCache->getSymbolicName(),
               MemOperationStr[sreq->getMemOperation()], pCache->getHomeNodeID(addr),
               pCache->getNodeID(), mreq->getPAddr(), globalClock, mreq);
    sreq->addDstNode(pCache->getHomeNodeID(addr));
    sreq->msgOwner = pCache;

    //printf("W\t%5d\t%10x\t%10x\t%5d\t%lld\n", pCache->getNodeID(), addr, pCache->calcTag(addr), pCache->getHomeNodeID(addr), globalClock);

#if 0
    if(addr==0x7c741a8c) {
        printf("   [%s] write miss ty %s (directory access) send to %d from %d on %x at %lld\n", pCache->getSymbolicName(),
               MemOperationStr[sreq->getMemOperation()], pCache->getHomeNodeID(addr), pCache->getNodeID(), mreq->getPAddr(), globalClock);
        Line *l = pCache->getLine(mreq->getPAddr());
        printf("%x\n", pCache->calcTag(addr));
        printf("line : %p, %x\n", l, l->getState());
    }
#endif
#if (defined RCHECK)
    Cache::rmap[pCache->getNodeID()][addr] = globalClock;
#endif

    DEBUGPRINT("   [%s] write debuggin timestamp for %x at %lld\n", pCache->getSymbolicName(), addr, globalClock);

    pCache->sendBelow(sreq);
#if 0
    I(pCache->getLine(mreq->getPAddr()));
    SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, true);

    pCache->sendBelow(sreq);
#endif
}

void DMESIProtocol::sendInvalidate(MemRequest *mreq)
{
    IJ(0);
#if 0
    PAddr addr = mreq->getPAddr();
    doSendInvalidate(mreq);
#endif
}

void DMESIProtocol::doSendInvalidate(MemRequest *mreq)
{
    IJ(0);
#if 0
    I(pCache->getLine(mreq->getPAddr()));
    SMPMemRequest *sreq = SMPMemRequest::create(mreq, pCache, false);

    pCache->sendBelow(sreq);
#endif
}

void DMESIProtocol::sendReadMissAck(SMPMemRequest *sreq)
{
    IJ(0);
#if 0
    pCache->respondBelow(sreq);
#endif
}

void DMESIProtocol::sendWriteMissAck(SMPMemRequest *sreq)
{
    IJ(0);
#if 0
    pCache->respondBelow(sreq);
#endif
}

void DMESIProtocol::sendInvalidateAck(SMPMemRequest *sreq)
{
    IJ(sreq->dstObj.find(pCache)!=sreq->dstObj.end());
    sreq->dstObj.erase(pCache);


    {
        SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, InvalidationAck);
        nsreq->addDst(sreq->msgOwner);

        pCache->respondBelow(nsreq);
    }

    DEBUGPRINT("   [%s] Invalidate Ack to %s (%d left) for %x at %lld\n",
               pCache->getSymbolicName(), sreq->msgOwner->getSymbolicName(),
               (int)sreq->dstObj.size(), sreq->getPAddr(), globalClock);

    if(sreq->dstObj.size()==0) {
        sreq->destroy();
        // I am the last one.
        // Erase the message and send data?
    }
    
}

void DMESIProtocol::readMissHandler(SMPMemRequest *sreq)
{
    IJ(0);
#if 0
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    if(l && !l->isLocked()) {
        combineResponses(sreq, (DMESIState_t) l->getState());
        changeState(l, DMESI_SHARED);
    }

    sendReadMissAck(sreq);
    //sendData(sreq);
#endif
}

void DMESIProtocol::writeMissHandler(SMPMemRequest *sreq)
{
    IJ(0);
#if 0
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    if(l && !l->isLocked()) {
        combineResponses(sreq, (DMESIState_t) l->getState());
        pCache->invalidateLine(addr, sendWriteMissAckCB::create(this, sreq), false);
        return;
    } else {
        sendWriteMissAck(sreq);
    }

    //sendData(sreq);
#endif
}

void DMESIProtocol::invalidateHandler(SMPMemRequest *sreq)
{
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    //IJ(l);
    //IJ(!l->isLocked());

    if(l && !l->isLocked()) {
        pCache->invalidateLine(addr, sendInvalidateAckCB::create(this, sreq), false);
        return;
    } else {
        if(!l) {
            //IJ(0);
            // This is possible
            // Since we allow silent invalidation
            //
        } else {
            //IJ(l->isLocked());
            DEBUGPRINT("   [%s] Invalidate in locked state (state %x), invalidate later for %x at %lld\n",
                       pCache->getSymbolicName(),
                       l->getState(),
                       sreq->getPAddr(), globalClock);
            PAddr taddr = pCache->calcTag(addr);
            //IJ(pCache->pendingInv.find(taddr)==pCache->pendingInv.end());
            pCache->pendingInv.insert(taddr);

            //IJ(0);
        }
        //printf("what?\n");
        // What am I supposed to do?
        sendInvalidateAck(sreq);
    }
}

void DMESIProtocol::invalidateReplyHandler(SMPMemRequest *sreq)  {
    PAddr addr = sreq->getPAddr();

    if(pCache->pendingInvCounter.find(addr)==pCache->pendingInvCounter.end()) {
        pCache->pendingInvCounter[addr]=1;
    } else {
        pCache->pendingInvCounter[addr]++;
    }

    DEBUGPRINT("   [%s] Invalidate Ack received from %s (%d received) for %x at %lld\n",
               pCache->getSymbolicName(), sreq->getRequestor()->getSymbolicName(),
               pCache->pendingInvCounter[addr], sreq->getPAddr(), globalClock);

    if(pCache->invCounter.find(addr)!=pCache->invCounter.end()) {
        if(pCache->pendingInvCounter[addr] == pCache->invCounter[addr]) {
            // All received
            finalizeInvReply(sreq);
            return;
        }
    }
    sreq->destroy();
}

void DMESIProtocol::finalizeInvReply(SMPMemRequest *sreq) {
    PAddr addr = sreq->getPAddr();
    pCache->pendingInvCounter.erase(addr);
    pCache->invCounter.erase(addr);

    IJ(sreq->getMemOperation()==MemReadW);
    DEBUGPRINT("   [%s] Exclusive Reply with invalidation pending finished for %x at %lld\n",
               pCache->getSymbolicName(), sreq->getPAddr(), globalClock);
    writeMissAckHandler(sreq);
}

#if 0
void DMESIProtocol::dirReplyHandler(SMPMemRequest *sreq)
{
    PAddr addr = sreq->getPAddr();
    DirectoryEntry *de = sreq->dentry;
    Line *l = pCache->getLine(addr);
    IJ(l);

    if(de->getStatus() == INVALID) {
        // Nobody has it. Go to memory
        DEBUGPRINT("   [%s] directory reply State : INVALID, %x\n", pCache->getSymbolicName(), addr);

        if(sreq->getMemOperation()==MemRead) {
            IJ(l->getState()==DMESI_TRANS_RD);
            changeState(l, DMESI_TRANS_RD_MEM);
        } else if (sreq->getMemOperation()==MemReadW) {
            IJ(l->getState()==DMESI_TRANS_WR);
            changeState(l, DMESI_TRANS_WR_MEM);
        } else {
            IJ(0);
        }
#if 0
        if(l->getState()==DMESI_TRANS_RD) {
            changeState(l, DMESI_TRANS_RD_MEM);
        } else if (l->getState()==DMESI_TRANS_WR) {
            changeState(l, DMESI_TRANS_WR_MEM);
        } else {
            IJ(0);
        }
#endif

        SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, MeshMemRequest);
        nsreq->addDstNode(pCache->getL2NodeID(addr));

        DEBUGPRINT("   [%s] requesting L2 cache access ty %s to %d for %x at %lld  (%p)\n",
                   pCache->getSymbolicName(), MemOperationStr[nsreq->getMemOperation()], pCache->getL2NodeID(addr), addr, globalClock, nsreq);

        //pCache->pendingWriteBackReq[addr] = false;

        // Kill the request
        sreq->destroy();

        pCache->sendBelow(nsreq);
        return;
    }

    if(sreq->getMemOperation()==MemRead) {
        if(de->getStatus() == SHARED || de->getStatus() == MODIFIED || de->getStatus() == EXCLUSIVE) {
            DEBUGPRINT("   [%s] directory reply for read miss State : %s, %x\n", pCache->getSymbolicName(), DirStatusStr[de->getStatus()], addr);
            // Someone has it. Grab the data from him!
            IJ(l->getState()==DMESI_TRANS_RD);

            MemObj *d = de->find();
            IJ(d);

            SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, MeshReadDataRequest);
            nsreq->addDstNode(d->getNodeID());
            nsreq->msgDst = d;

            DEBUGPRINT("   [%s] requesting remote cache access to %s for %x at %lld\n",
                       pCache->getSymbolicName(), d->getSymbolicName(), addr, globalClock);

            //pCache->pendingWriteBackReq[addr] = false;

            if(de->getStatus() == MODIFIED || de->getStatus() == EXCLUSIVE) {
                nsreq->writeBack = true;
                //pCache->pendingWriteBackReq[addr] = true;
            }

            // Kill the request
            sreq->destroy();

            pCache->sendBelow(nsreq);
            return;
        } else {
            IJ(0);
        }
    } else if(sreq->getMemOperation()==MemReadW) {
        if(de->getStatus() == SHARED) {
            IJ(de->getNum()>1);

            DEBUGPRINT("   [%s] directory reply for write miss: SHARED %x\n", pCache->getSymbolicName(), addr);

            // Send invalidate to all sharers
            // JJO THIS SHOULD BE MODIFIED FOR MESH NETWORK
            // FIXME:
//#if (defined MESHBUS)
            SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, MeshInvRequest);
            nsreq->dataBack = !(de->fillDst(nsreq->dst, nsreq->dstObj, pCache));  // return found
            // if not found, we need data back

            DEBUGPRINT("   [%s] Invalidating %d sharers for %x at %lld\n",
                       pCache->getSymbolicName(), (int)nsreq->dstObj.size(), addr, globalClock);

            IJ(pCache->pendingInvCounter.find(addr)==pCache->pendingInvCounter.end());
            pCache->pendingInvCounter[addr] = nsreq->dstObj.size();

            // Kill the request
            sreq->destroy();

            pCache->sendBelow(nsreq);
#if 0
//#elif (defined MESHNOC)
            std::set<MemObj*> dList;
            bool dataBack = !(de->fillDstOnly(dList, pCache));  // return found

            DEBUGPRINT("   [%s] Invalidating %d sharers for %x at %lld\n",
                       pCache->getSymbolicName(), (int)dList.size(), addr, globalClock);

            IJ(pCache->pendingInvCounter.find(addr)==pCache->pendingInvCounter.end());
            pCache->pendingInvCounter[addr] = dList.size();

            for(std::set<MemObj*>::iterator it = dList.begin(); it!=dList.end(); it++) {
                SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, MeshInvRequest);

                nsreq->addDstNode((*it)->getNodeID());
                nsreq->dstObj.insert((*it));

                nsreq->dataBack = dataBack;
                dataBack = false;

                DEBUGPRINT("   [%s] Invalidate sending to %s for %x at %lld\n",
                           pCache->getSymbolicName(), (*it)->getSymbolicName(), addr, globalClock);

                pCache->sendBelow(nsreq);
            }
            // Kill the request
            sreq->destroy();

//#else
            fprintf(stderr, "You should define either MESHBUS of MESHNOC\n");
            exit(1);
//#endif
#endif

            return;

        } else { // EXCL of Modified
            IJ(de->getNum()==1);
            //IJ(!de->hasThis(pCache)); // I shouldn't have this
            DEBUGPRINT("   [%s] directory reply for write miss: %s for %x\n", pCache->getSymbolicName(), DirStatusStr[de->getStatus()], addr);

            if(de->hasThis(pCache)) {
                IJ(de->getStatus() == EXCLUSIVE);
                IJ(l && !l->canBeWritten());
                //  a write
                //  b read (a, b sharer)
                //  b invalidated (a exclusive in directory, a is shared in cache)
                //  a write -> miss in shared state

                // Write to myself
                // update the directory

                DEBUGPRINT("   [%s] EXCLUSIVE/local shared miss: LOCALLY request solved. update the directory for %x at %lld\n",
                           pCache->getSymbolicName(), addr, globalClock);

                pCache->updateDirectory(sreq);

                return;
            }


            MemObj *d = de->find();
            IJ(d);

            SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, MeshWriteDataRequest);
            nsreq->addDstNode(d->getNodeID());
            nsreq->msgDst = d;

            DEBUGPRINT("   [%s] requesting remote cache access (for write) to %s for %x at %lld\n",
                       pCache->getSymbolicName(), d->getSymbolicName(), addr, globalClock);

            // Kill the request
            sreq->destroy();

            pCache->sendBelow(nsreq);
            return;
        }
    } else {
        IJ(0);
        DEBUGPRINT(" what is this? %d\n", sreq->getMemOperation());
    }
}
#endif

void DMESIProtocol::readMissAckHandler(SMPMemRequest *sreq) {
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    IJ(l);
    DEBUGPRINT("   [%s] Read ack(finished) for %x (excl %d) at %lld\n",
               pCache->getSymbolicName(), addr, sreq->isExcl, globalClock);

    PAddr taddr = pCache->calcTag(addr);

#if 0
    if(SMPCache::mutInvReq.find(taddr) != SMPCache::mutInvReq.end()) {
        SMPCache::mutInvReq.erase(taddr);
        DEBUGPRINT("   [%s] Snoop Invalidation cleaning for %x at %lld\n"
                   , pCache->getSymbolicName()
                   , addr,
                   globalClock);
    }
#endif

    IJ(sreq->getMemOperation()==MemRead);

    if(sreq->isExcl) {
        changeState(l, DMESI_EXCLUSIVE);
        DEBUGPRINT("   [%s] Read ack(finished) Exclusive for %x (excl %d) at %lld\n",
                   pCache->getSymbolicName(), addr, sreq->isExcl, globalClock);
    } else {
#if 0
        if(sreq->isOwner) {
            changeState(l, DMESI_OWNER);
        } else {
            changeState(l, DMESI_SHARED);
        }
#endif
        DEBUGPRINT("   [%s] Read ack(finished) Shared for %x (excl %d) at %lld\n",
                   pCache->getSymbolicName(), addr, sreq->isExcl, globalClock);

        changeState(l, DMESI_SHARED);
    }
    DEBUGPRINT("   [%s] Cache line set to %x\n", pCache->getSymbolicName(), l->getState());

    pCache->writeLine(addr);
    pCache->concludeAccess(sreq->getOriginalRequest());

#if 0
    if(pCache->TLReqList.find(pCache->calcTag(addr))!=pCache->TLReqList.end()) {
        //SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, SNRequestRelease);
        SMPMemRequest *nsreq = SMPMemRequest::create(pCache, addr, MemRead, false, NULL, SNRequestRelease);
        nsreq->msgOwner = pCache;
        nsreq->addDstNode(pCache->getHomeNodeID(addr));

        DEBUGPRINT("   [%s] Snoop Request Release for %x at %lld\n",
                   pCache->getSymbolicName(), addr, globalClock);

        //nsreq->goDown(0, pCache->lowerLevel[0]);
        pCache->sendBelowI(nsreq);
        pCache->TLReqList.erase(pCache->calcTag(addr));
    }
#endif


    //DirectoryEntry *de = sreq->dentry;
    //IJ(de->isLocked());
    //de->setLock(false);

    sreq->destroy();
#if 0
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    I(l);

    if(sreq->getState() == DMESI_INVALID && l->getState() == DMESI_TRANS_RD) {
        I(!sreq->isFound());
        changeState(l, DMESI_TRANS_RD_MEM);
        sreq->noSnoop();
        pCache->sendBelow(sreq); // miss delay may be counted twice
        return;
    }

    if(sreq->getState() == DMESI_INVALID) {
        I(l->getState() == DMESI_TRANS_RD_MEM);
        changeState(l, DMESI_EXCLUSIVE);
    } else {
        I(l->getState() == DMESI_TRANS_RD);
        changeState(l, DMESI_SHARED);
    }

    pCache->writeLine(addr);
    pCache->concludeAccess(sreq->getOriginalRequest());
    sreq->destroy();
#endif
}

void DMESIProtocol::writeMissAckHandler(SMPMemRequest *sreq)
{
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    IJ(l);

    IJ(l->getState() == DMESI_TRANS_WR || l->getState() == DMESI_TRANS_WR_MEM);

    DEBUGPRINT("   [%s] Write ack(finished) for %x at %lld\n"
               , pCache->getSymbolicName()
               , addr, globalClock);

    PAddr taddr = pCache->calcTag(addr);

    changeState(l, DMESI_MODIFIED);

    pCache->writeLine(addr);
    pCache->concludeAccess(sreq->getOriginalRequest());

    //DirectoryEntry *de = sreq->dentry;
    //IJ(de->isLocked());
    //de->setLock(false);

#if 0
    if(pCache->TLReqList.find(pCache->calcTag(addr))!=pCache->TLReqList.end()) {
        //SMPMemRequest *nsreq = SMPMemRequest::create(sreq, pCache, SNRequestRelease);
        SMPMemRequest *nsreq = SMPMemRequest::create(pCache, addr, MemRead, false, NULL, SNRequestRelease);
        nsreq->msgOwner = pCache;
        nsreq->addDstNode(pCache->getHomeNodeID(addr));

        DEBUGPRINT("   [%s] Snoop Request Release for %x at %lld\n",
                   pCache->getSymbolicName(), addr, globalClock);

        //nsreq->goDown(0, pCache->lowerLevel[0]);
        pCache->sendBelowI(nsreq);
        pCache->TLReqList.erase(pCache->calcTag(addr));
    }
#endif

    sreq->destroy();
#if 0
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    I(l);
    I(sreq->needsData());

    if(sreq->getState() == DMESI_INVALID && l->getState() == DMESI_TRANS_WR) {
        I(!sreq->isFound());
        changeState(l, DMESI_TRANS_WR_MEM);
        sreq->noSnoop();
        pCache->sendBelow(sreq);
        return;
    }

    I(l->getState() == DMESI_TRANS_WR || l->getState() == DMESI_TRANS_WR_MEM);
    changeState(l, DMESI_MODIFIED);

    pCache->writeLine(addr);
    pCache->concludeAccess(sreq->getOriginalRequest());
    sreq->destroy();
#endif
}

#if 0
void DMESIProtocol::invalidateAckHandler(SMPMemRequest *sreq)
{
    PAddr addr = sreq->getPAddr();
    Line *l = pCache->getLine(addr);

    I(l);
    I(l->getState() == DMESI_TRANS_WR);
    changeState(l, DMESI_MODIFIED);

    pCache->concludeAccess(sreq->getOriginalRequest());
    sreq->destroy();
}
#endif

#if 0
void DMESIProtocol::sendDisplaceNotify(PAddr addr, CallbackBase *cb)
{
    I(pCache->getLine(addr));
    SMPMemRequest *sreq = SMPMemRequest::create(pCache, addr, MemPush, false, cb);
    pCache->sendBelow(sreq);
}
#endif

void DMESIProtocol::sendData(SMPMemRequest *sreq)
{
    I(0);
}

void DMESIProtocol::dataHandler(SMPMemRequest *sreq)
{
    // this should cause an access to the cache
    // for now, I am assuming data comes with response,
    // so the access is done in the ack handlers
    I(0);
}

void DMESIProtocol::combineResponses(SMPMemRequest *sreq,
                                     DMESIState_t localState)
{
#if 0
    DMESIState_t currentResponse = (DMESIState_t) sreq->getState();

    if((localState == DMESI_INVALID) || (localState & DMESI_TRANS)) {
        sreq->setState(currentResponse);
        return;
    }

    if(localState == DMESI_SHARED) {
        I(currentResponse != DMESI_EXCLUSIVE && currentResponse != DMESI_MODIFIED);
        if(!sreq->isFound()) {
            sreq->setFound();
            sreq->setSupplier(pCache);
            sreq->setState(localState);
        }
        return;
    }

    if(localState == DMESI_EXCLUSIVE) {
        I(currentResponse != DMESI_EXCLUSIVE && currentResponse != DMESI_MODIFIED);
        sreq->setFound();
        sreq->setSupplier(pCache);
        sreq->setState(localState);
        return;
    }

    I(localState == DMESI_MODIFIED);
    I(currentResponse != DMESI_EXCLUSIVE && currentResponse != DMESI_MODIFIED);
    sreq->setFound();
    sreq->setSupplier(pCache);
    sreq->setWriteDown();
    sreq->setState(localState);
#endif
}

