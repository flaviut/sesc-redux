/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze
                  Jose Renau
                  Karin Strauss
		  Milos Prvulovic

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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iostream>

#include "nanassert.h"

#include "SescConf.h"
#include "libmem/MemorySystem.h"
#include "SMPSliceCache.h"
#include "MSHR.h"
#include "libcore/OSSim.h"
#include "libcore/GProcessor.h"

#include "SMPCache.h"

#include "SMPMemRequest.h"
#include "SMemorySystem.h"

static const char
*k_numPorts="numPorts", *k_portOccp="portOccp",
 *k_hitDelay="hitDelay", *k_missDelay="missDelay";

extern char* MemOperationStr[];

extern char* DirStatusStr[];

Directory **SMPSliceCache::globalDirMap;

//#define DEBUGPRINT printf
GStatsCntr SMPSliceCache::Read_U("Read_U");
GStatsCntr SMPSliceCache::Read_S("Read_S");
GStatsCntr SMPSliceCache::Read_E("Read_E");
GStatsCntr SMPSliceCache::Read_bS("Read_bS");
GStatsCntr SMPSliceCache::Read_bE("Read_bE");

GStatsCntr SMPSliceCache::Read_bS_H("Read_bS_H");
GStatsCntr SMPSliceCache::Read_bE_H("Read_bE_H");

GStatsCntr SMPSliceCache::Write_U("Write_U");
GStatsCntr SMPSliceCache::Write_S("Write_S");
GStatsCntr SMPSliceCache::Write_E("Write_E");
GStatsCntr SMPSliceCache::Write_bS("Write_bS");
GStatsCntr SMPSliceCache::Write_bE("Write_bE");

GStatsCntr SMPSliceCache::Write_bS_H("Write_bS_H");
GStatsCntr SMPSliceCache::Write_bE_H("Write_bE_H");

SMPSliceCache::SMPSliceCache(SMemorySystem *gms, const char *section, const char *name)
    : MemObj(section, name)
#ifdef SESC_SMP
    ,inclusiveCache(SescConf->checkBool(section, "inclusive") ?
                    SescConf->getBool(section, "inclusive") : true)
#else
    ,inclusiveCache(true)
#endif
    ,readHalfMiss("%s:readHalfMiss", name)
    ,writeHalfMiss("%s:writeHalfMiss", name)
    ,writeMiss("%s:writeMiss", name)
    ,readMiss("%s:readMiss", name)
    ,readHit("%s:readHit", name)
    ,writeHit("%s:writeHit", name)
    ,writeBack("%s:writeBack", name)
    ,lineFill("%s:lineFill", name)
    ,linePush("%s:linePush", name)
    ,nForwarded("%s:nForwarded", name)
    ,nWBFull("%s:nWBFull", name)
    ,avgPendingWrites("%s_avgPendingWrites", name)
    ,avgMissLat("%s_avgMissLat", name)
    ,rejected("%s:rejected", name)
    ,rejectedHits("%s:rejectedHits", name)
#ifdef MSHR_BWSTATS
    ,secondaryMissHist("%s:secondaryMissHist", name)
    ,accessesHist("%s:accessHistBySecondaryMiss", name)
    ,mshrBWHist(100, "%s:mshrAcceses", name)
#endif
{
    MemObj *lower_level = NULL;
    char busName[512];
    char tmpName[512];

    // JJO
    dir = new Directory(section);
    if(!globalDirMap) {
        globalDirMap = new Directory *[gms->getPPN()];
    }
    globalDirMap[gms->getPID()] = dir;

    SescConf->isInt(section, "numPortsDir");
    SescConf->isInt(section, "portOccpDir");
    SescConf->isInt(section, "hitDelayDir");
    hitDelayDir = SescConf->getInt(section, "hitDelayDir");

    char dirPortName[100];
    sprintf(dirPortName, "%s_DIR", name);
    cacheDirPort = PortGeneric::create(dirPortName,
                                       SescConf->getInt(section, "numPortsDir"),
                                       SescConf->getInt(section, "portOccpDir"));
    nodeID = gms->getPID();
    maxNodeID = gms->getPPN();
    //printf("%s nID: %d / %d\n",name, nodeID, maxNodeID);
    // JJO end

    if(SescConf->checkInt(section,"nBanks"))
        nBanks = SescConf->getInt(section,"nBanks");
    else
        nBanks = 1;

    int32_t nMSHRsharers = 1;
    if(SescConf->checkInt(section, "nMSHRsharers"))
        nMSHRsharers = SescConf->getInt(section, "nMSHRsharers");

    nAccesses = new GStatsCntr *[nBanks];
    cacheBanks = new CacheType *[nBanks];
    bankMSHRs  = new MSHR<PAddr,SMPSliceCache> *[nBanks];

    const char* mshrSection = SescConf->getCharPtr(section,"MSHR");

    sprintf(tmpName, "%s_MSHR%d", name, 0);
    cacheBanks[0] = CacheType::create(section, "", tmpName);
    bankMSHRs[0] = MSHR<PAddr,SMPSliceCache>::create(tmpName, mshrSection);
    nAccesses[0] = new GStatsCntr("%s_B%d:nAccesses", name, 0);

    for(int32_t b = 1; b < nBanks; b++) {
        sprintf(tmpName, "%s_B%d", name, b);
        cacheBanks[b] = CacheType::create(section, "", tmpName);

        sprintf(tmpName, "%s_MSHR%d", name, b);

        if((b % nMSHRsharers) == 0)
            bankMSHRs[b] = MSHR<PAddr,SMPSliceCache>::attach(tmpName, mshrSection, bankMSHRs[0]);
        else
            bankMSHRs[b] = bankMSHRs[b - (b % nMSHRsharers)];

        nAccesses[b] = new GStatsCntr("%s_B%d:nAccesses", name, b);
    }

    I(gms);
    lower_level = gms->declareMemoryObj(section, k_lowerLevel);

    if(SescConf->checkBool(section,"SetMSHRL2")
            && SescConf->getBool(section,"SetMSHRL2")) {
        IJ(0);
    }

    int32_t cacheNumPorts = SescConf->getInt(section, k_numPorts);
    int32_t cachePortOccp = SescConf->getInt(section, k_portOccp);
    int32_t bankNumPorts = cacheNumPorts;
    int32_t bankPortOccp = cachePortOccp;

    if(SescConf->checkInt(section, "bankNumPorts")) {
        bankNumPorts = SescConf->getInt(section, "bankNumPorts");
    }
    if(SescConf->checkInt(section, "bankPortOccp")) {
        bankPortOccp = SescConf->getInt(section, "bankPortOccp");
    }

    int32_t mshrNumPorts = bankNumPorts;
    int32_t mshrPortOccp = bankPortOccp;

    if(SescConf->checkInt(section, "mshrNumPorts")) {
        mshrNumPorts = SescConf->getInt(section, "mshrNumPorts");
    }
    if(SescConf->checkInt(section, "mshrPortOccp")) {
        mshrPortOccp = SescConf->getInt(section, "mshrPortOccp");
    }

    cachePort = PortGeneric::create(name, cacheNumPorts, cachePortOccp);
    bankPorts = new PortGeneric *[nBanks];
    mshrPorts = new PortGeneric *[nBanks];

    for(int32_t b = 0; b < nBanks; b++) {
        sprintf(tmpName, "%s_B%d", name, b);
        bankPorts[b] = PortGeneric::create(tmpName, bankNumPorts, bankPortOccp);
        if((b % nMSHRsharers) == 0) {
            sprintf(tmpName, "%s_MSHR_B%d", name, b);
            mshrPorts[b] = PortGeneric::create(tmpName, mshrNumPorts, mshrPortOccp);
        } else {
            mshrPorts[b] = mshrPorts[b - (b % nMSHRsharers)];
        }
    }

    SescConf->isInt(section, k_hitDelay);
    hitDelay = SescConf->getInt(section, k_hitDelay);

    SescConf->isInt(section, k_missDelay);
    missDelay = SescConf->getInt(section, k_missDelay);

    if (SescConf->checkBool(section, "wbfwd"))
        doWBFwd = SescConf->getBool(section, "wbfwd");
    else
        doWBFwd = true;

    if (SescConf->checkInt(section, "fwdDelay"))
        fwdDelay = SescConf->getInt(section, "fwdDelay");
    else
        fwdDelay = missDelay;

    pendingWrites = 0;
    if (SescConf->checkInt(section, "maxWrites"))
        maxPendingWrites = SescConf->getInt(section, "maxWrites");
    else
        maxPendingWrites = -1;

    defaultMask  = ~(cacheBanks[0]->getLineSize()-1);


	inv_opt = true;
    if(SescConf->checkBool(section, "invOpt")) {
    	inv_opt = SescConf->getBool(section, "invOpt");
    }



#ifdef MSHR_BWSTATS
    if(SescConf->checkBool(section, "parallelMSHR")) {
        parallelMSHR = SescConf->getBool(section, "parallelMSHR");
    } else {
        parallelMSHR = false;
    }
#endif

    if (lower_level != NULL)
        addLowerLevel(lower_level);
}

SMPSliceCache::~SMPSliceCache()
{
    delete [] nAccesses;
    for(int32_t b = 0; b < nBanks; b++)
        cacheBanks[b]->destroy();
    delete [] cacheBanks;
    delete [] bankMSHRs;
    delete [] bankPorts;
    delete [] mshrPorts;
}

void SMPSliceCache::access(MemRequest *mreq)
{
    IJ(0);
#if 0
    mreq->setClockStamp((Time_t) - 1);
    if(mreq->getPAddr() <= 1024) { // TODO: need to implement support for fences
        mreq->goUp(0);
        return;
    }

    nAccesses[getBankId(mreq->getPAddr())]->inc();

    switch(mreq->getMemOperation()) {
    case MemReadW:
    case MemRead:
        read(mreq);
        break;
    case MemWrite:
        write(mreq);
        break;
    case MemPush:
        pushLine(mreq);
        break;
    default:
        specialOp(mreq);
        break;
    }
#endif
}

void SMPSliceCache::read(MemRequest *mreq)
{
#ifdef MSHR_BWSTATS
    if(parallelMSHR)
        mshrBWHist.inc();
#endif
    //enforcing max ops/cycle for the specific bank
    doReadBankCB::scheduleAbs(nextBankSlot(mreq->getPAddr()), this, mreq);
}

void SMPSliceCache::doReadBank(MemRequest *mreq)
{
    // enforcing max ops/cycle for the whole cache
    doReadCB::scheduleAbs(nextCacheSlot(), this, mreq);
}

#if 0
void SMPSliceCache::L2writeBackReturn(MemRequest *mreq, TimeDelta_t d) {
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    DEBUGPRINT("   [%s] L2 writeback return to %d for  %x at %lld\n",
               getSymbolicName(), sreq->msgOwner->getNodeID(), mreq->getPAddr(), globalClock);

    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, MeshMemWriteBackReply);
    nsreq->addDstNode(sreq->msgOwner->getNodeID());
    sreq->destroyAll();
    nsreq->goDown(d, lowerLevel[0]);
}
#endif


void SMPSliceCache::L2requestReturn(MemRequest *mreq, TimeDelta_t d) {
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    if(sreq->getMeshOperation()==MeshMemAccessReply) {
        IJ(sreq->saveReq);
        sreq->saveReq->goDown(d, lowerLevel[0]);
        sreq->destroy();
    } else {
        sreq->goDown(d, lowerLevel[0]);
    }
#if 0
    if(sreq->getMeshOperation()==MeshMemAccessReply) {
#if (defined SIGDEBUG)
        MeshOperation m = sreq->meshOp;
#endif
        sreq->meshOp = sreq->saveMeshOp;
#if (defined SIGDEBUG)
        sreq->saveMeshOp = m;
#endif
    }
    //DEBUGPRINT("   [%s] L2RR called at %lld, will call doAccess at %lld (delay %d)\n", getSymbolicName(), globalClock, globalClock+d, d);
    doAccessDirCB::scheduleAbs(globalClock+d, this, mreq);
#endif
}

void SMPSliceCache::doRead(MemRequest *mreq)
{
    Line *l = getCacheBank(mreq->getPAddr())->readLine(mreq->getPAddr());

    if (l == 0) {
        if(isInWBuff(mreq->getPAddr())) {
            nForwarded.inc();
            //mreq->goUp(fwdDelay);
            L2requestReturn(mreq, fwdDelay);
            return;
        }
        readMissHandler(mreq);
        return;
    }

    readHit.inc();
    l->incReadAccesses();

    //mreq->goUp(hitDelay);
    L2requestReturn(mreq, hitDelay);
}

void SMPSliceCache::readMissHandler(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

#ifdef MSHR_BWSTATS
    if(!parallelMSHR)
        mshrBWHist.inc();
#endif


    mreq->setClockStamp(globalClock);
    nextMSHRSlot(addr); // checking if there is a pending miss
    if(!getBankMSHR(addr)->issue(addr, MemRead)) {
        getBankMSHR(addr)->addEntry(addr, doReadQueuedCB::create(this, mreq),
                                    activateOverflowCB::create(this, mreq), MemRead);
        return;
    }

    // Added a new MSHR entry, now send request to lower level
    readMiss.inc();


#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    MemRequest *oreq = sreq->getOriginalRequest();
    if(oreq) {
        DInst *dinst = oreq->getDInst();
        if(dinst) {
            ThreadContext *context = dinst->context;
            //printf("L2Miss from %x\n", dinst->getInst()->getAddr());
            if(context) {
                ThreadContext::nL2CM[context->getPid()]++;
                if( (globalClock-ThreadContext::lastUpdate)>100000) {
                    ThreadContext::lastUpdate = globalClock;
                    context->updateCM();
                }
            }
        }
    }
#endif




    sendMiss(mreq);
}

void SMPSliceCache::doReadQueued(MemRequest *mreq)
{
    PAddr paddr = mreq->getPAddr();
    readHalfMiss.inc();


    avgMissLat.sample(globalClock - mreq->getClockStamp());
    //mreq->goUp(hitDelay);
    L2requestReturn(mreq, hitDelay);

    // the request came from the MSHR, we need to retire it
    getBankMSHR(paddr)->retire(paddr);
}

// this is here just because we have no feedback from the memory
// system to the processor telling it to stall the issue of a request
// to the memory system. this will only be called if an overflow entry
// is activated in the MSHR and the is *no* pending request for the
// same line.  therefore, sometimes the callback associated with this
// function might not be called.
void SMPSliceCache::activateOverflow(MemRequest *mreq)
{
    if(mreq->getMemOperation() == MemRead || mreq->getMemOperation() == MemReadW) {

        Line *l = getCacheBank(mreq->getPAddr())->readLine(mreq->getPAddr());

        if (l == 0) {
            // no need to add to the MSHR, it is already there
            // since it came from the overflow
            readMiss.inc();

#if 0
            SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
            MemRequest *oreq = sreq->getOriginalRequest();
            if(oreq) {
                DInst *dinst = oreq->getDInst();
                if(dinst) {
                    ThreadContext *context = dinst->context;
                    if(context) {
                        ThreadContext::nL2CM[context->getPid()]++;
                        if( (globalClock-ThreadContext::lastUpdate)>100000) {
                            ThreadContext::lastUpdate = globalClock;
                            context->updateCM();
                        }
                    }
                }
            }
#endif





            sendMiss(mreq);
            return;
        }

        readHit.inc();
        PAddr paddr = mreq->getPAddr();
        //mreq->goUp(hitDelay);
        L2requestReturn(mreq, hitDelay);

        // the request came from the MSHR overflow, we need to retire it
        getBankMSHR(paddr)->retire(paddr);

        return;
    }

    I(mreq->getMemOperation() == MemWrite);

    Line *l = getCacheBank(mreq->getPAddr())->writeLine(mreq->getPAddr());

    if (l == 0) {
        // no need to add to the MSHR, it is already there
        // since it came from the overflow
        writeMiss.inc();
        sendMiss(mreq);
        return;
    }

    writeHit.inc();
#ifndef SESC_SMP
    l->makeDirty();
#endif

    PAddr paddr = mreq->getPAddr();
    //mreq->goUp(hitDelay);
    L2requestReturn(mreq, hitDelay);

    // the request came from the MSHR overflow, we need to retire it
    wbuffRemove(paddr);
    getBankMSHR(paddr)->retire(paddr);
}

void SMPSliceCache::write(MemRequest *mreq)
{
#ifdef MSHR_BWSTATS
    if(parallelMSHR)
        mshrBWHist.inc();
#endif
    doWriteBankCB::scheduleAbs(nextBankSlot(mreq->getPAddr()), this, mreq);
}

void SMPSliceCache::doWriteBank(MemRequest *mreq)
{
    doWriteCB::scheduleAbs(nextCacheSlot(), this, mreq);
}

void SMPSliceCache::doWrite(MemRequest *mreq)
{
    Line *l = getCacheBank(mreq->getPAddr())->writeLine(mreq->getPAddr());

    if (l == 0) {
        writeMissHandler(mreq);
        return;
    }

    writeHit.inc();
    l->makeDirty();

    mreq->goUp(hitDelay);
    //L2requestReturn(mreq, hitDelay);
}

void SMPSliceCache::writeMissHandler(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

#ifdef MSHR_BWSTATS
    if(!parallelMSHR)
        mshrBWHist.inc();
#endif

    wbuffAdd(addr);
    mreq->setClockStamp(globalClock);

    if(!getBankMSHR(addr)->issue(addr, MemWrite)) {
        getBankMSHR(addr)->addEntry(addr, doWriteQueuedCB::create(this, mreq),
                                    activateOverflowCB::create(this, mreq), MemWrite);
        return;
    }

    writeMiss.inc();
    sendMiss(mreq);
}

void SMPSliceCache::doWriteQueued(MemRequest *mreq)
{
    PAddr paddr = mreq->getPAddr();

    writeHalfMiss.inc();

    avgMissLat.sample(globalClock - mreq->getClockStamp());

    //L2requestReturn(mreq, hitDelay);
    mreq->goUp(hitDelay);

    wbuffRemove(paddr);

    // the request came from the MSHR, we need to retire it
    getBankMSHR(paddr)->retire(paddr);
}

void SMPSliceCache::specialOp(MemRequest *mreq)
{
    mreq->goUp(1);
}

void SMPSliceCache::doAccessDir(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    PAddr addr = mreq->getPAddr();
    PAddr taddr = calcTag(addr);

    bool dataSent = false;

    switch(sreq->meshOp) {
        //case ForwardRequestNAK:
    case ReadRequest:
    {

        DEBUGPRINT("   [%s] Read req. receive (%s) from %d at %d on %x at %lld  (%p)\n"
                   , getSymbolicName()
                   , MemOperationStr[sreq->getMemOperation()]
                   , sreq->getSrcNode()
                   , getNodeID()
                   , addr
                   , globalClock
                   , sreq);
        // Find Dir
        DirectoryEntry *de = dir->find(addr);

        IJ(de);

        if(!de->isBusy()) {
            // Case 4-1
            // If directory state is Unowned or Exclusive with requestor as
            // owner, transitions to Exclusive and returns an exclusive reply
            // to the requestor.
            //if(sreq->meshOp==ForwardRequestNAK) {
            //	de->removeSharer(sreq->msgOwner);
            //}
            if(de->getStatus()==UNOWNED || (de->getStatus()==EXCLUSIVE && de->getOwner()==sreq->msgOwner)) {
                IJ(de->getNum()==0 || de->getNum()==1);
                //de->setBusy();
                if(de->getNum()==0) {
                    de->addOwner(sreq->msgOwner);
                } else {
                    IJ(de->hasThis(sreq->msgOwner));
                }
                //de->clearSharers();

                if(de->getStatus()==UNOWNED) {
                    DEBUGPRINT("   [%s] Data UNOWNED, memory read %x at %lld\n",
                               getSymbolicName(), mreq->getPAddr(), globalClock);
                } else {
                    //IJ(0);
                    DEBUGPRINT("   [%s] Data EXCLUSIVE with requestor as owner, memory read %x at %lld\n",
                               getSymbolicName(), mreq->getPAddr(), globalClock);
                }

                de->setStatus(EXCLUSIVE);

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, ExclusiveReply);
                    nsreq->addDst(sreq->msgOwner);

                    DEBUGPRINT("   [%s] Directory: busy(%d), status %d, owner %s\n",
                               getSymbolicName(), de->isBusy(), de->getStatus(), de->getOwner()->getSymbolicName());

                    DEBUGPRINT("   [%s] Exclusive Reply (after L2 access) to %d for %x at %lld  (%p, kill %p)\n",
                               getSymbolicName(),
                               sreq->msgOwner->getNodeID(), mreq->getPAddr(), globalClock, nsreq, sreq);
                    //nsreq->goDown(0, lowerLevel[0]);
                    read(nsreq);
                }

                sreq->destroy();

                //read(mreq);
                return;
            }


            //IJ(!de->hasThis(sreq->msgOwner));
#if 0
            if(de->hasThis(sreq->getRequestor())) {
                if(de->getStatus() == EXCLUSIVE) {
                    de->setStatus(EXCLUSIVE);
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, ExclusiveReply);
                    nsreq->addDstNode(sreq->msgOwner->getNodeID());
                    sreq->destroy();

                    DEBUGPRINT("   [%s] Exclusive Reply to %d for %x at %lld  (%p, kill %p)\n",
                               getSymbolicName(), sreq->msgOwner->getNodeID(), mreq->getPAddr(), globalClock, nsreq, sreq);

                    nsreq->goDown(hitDelayDir, lowerLevel[0]);
                    return;
                }
            }
#endif

            // If directory state is Shared, the requesting node is marked in
            // the bit vector and a shared reply is returned to the requestor.
            // Naah... We need data forward request to one of ther shareres.
            if(de->getStatus()==SHARED) {
                de->addSharer(sreq->msgOwner);

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, SharedReply);
                    nsreq->addDst(sreq->msgOwner);
                    DEBUGPRINT("   [%s] Data in SHARED, sending Shared Reply to %s for %x at %lld\n",
                               getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);
                    //nsreq->goDown(0, lowerLevel[0]);
                    read(nsreq);
                }
                sreq->destroy();

                return;
            }

            // If directory state is Exclusive with another owner, transitions
            // to Busy-shared with requestor as owner and send out an intervention
            // shared request to the previous owner and a speculative
            // reply to the requestor.
            if(de->getStatus()==EXCLUSIVE) {
                IJ(de->getOwner()!=sreq->msgOwner);

                de->setStatus(SHARED);

                MemObj *prevOwner = de->getOwner();
                de->addOwner(sreq->msgOwner);
                {
                    de->setBusy();

                    {
                        SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, IntervSharedRequest);
                        nsreq->addDst(prevOwner);

                        DEBUGPRINT("   [%s] Exclusive with another owner, sending intervention to %s for %x at %lld\n",
                                   getSymbolicName(), prevOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                        //nsreq->goDown(0, lowerLevel[0]);
                        nsreq->goDown(hitDelayDir, lowerLevel[0]);

                    }
                    {
                        SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, SpeculativeReply);
                        nsreq->addDst(sreq->msgOwner);

                        DEBUGPRINT("   [%s] Exclusive with another owner, sending speculative reply to %s for %x at %lld\n",
                                   getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                        //nsreq->goDown(0, lowerLevel[0]);
                        read(nsreq);
                    }

                    IJ(writeBackInfo.find(taddr) == writeBackInfo.end());
                    writeBackInfo[taddr] = sreq->getOriginalRequest();
                    DEBUGPRINT("   [%s] to BUSY-shared for %x (tag %x) wbInfo: %d (%p) at %lld\n",
                               getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);
                    DEBUGPRINT("   [%s] to busy-shared: save original request %p for %x (%x) at %lld\n",
                               getSymbolicName(), writeBackInfo[taddr], addr, taddr, globalClock);
                }

                sreq->destroy();
                return;
            }
            IJ(0);

        } else {
            {
                // NAAAAAAAAAAAAAAAAAAK!!
                SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, NAK);

                nsreq->addDst(sreq->msgOwner);

                DEBUGPRINT("   [%s] Directory BUSY %d, state %d, sending NAK to %s for %x at %lld\n",
                           getSymbolicName(), de->isBusy(), de->getStatus(),
                           sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                sreq->destroy();

                nsreq->goDown(hitDelayDir, lowerLevel[0]);
                //nsreq->goDown(0, lowerLevel[0]);
                return;
#if 0
                Time_t nextTry = nextDirSlot();
                if (nextTry == globalClock)
                    nextTry++;
                DEBUGPRINT("   [%s] dir req. failed (locked) ty %s from %d at %d on %x at %lld  (%p)\n",
                           getSymbolicName(), MemOperationStr[sreq->getMemOperation()], sreq->getSrcNode(), getNodeID(), addr, globalClock, sreq);
                doAccessDirCB::scheduleAbs(nextTry, this, mreq);
                return;
#endif
            }
        }
        
    }
    break;

    case SharingWriteBack:
    {
        IJ(writeBackInfo.find(taddr)!=writeBackInfo.end());
        DEBUGPRINT("   [%s] SharingWriteBack received for %x (%x) n: %d (%p) at %lld\n",
                   getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);
        writeBackInfo.erase(taddr);

        DirectoryEntry *de = dir->find(addr);
        IJ(de);

        //IJ(de->isBusy());
        DEBUGPRINT("   [%s] Directory status: busy(%d) nsharers: %d owner %s for %x at %lld\n",
                   getSymbolicName(), de->isBusy(), (int)de->getNum(), de->getOwner()->getSymbolicName(), addr, globalClock);
        de->unsetBusy();
        de->setStatus(SHARED);

        // Update memory
        processWriteBack(mreq);
        return;
    }
    break;
    case SharingTransfer:
    {
        IJ(writeBackInfo.find(taddr)!=writeBackInfo.end());
        DEBUGPRINT("   [%s] SharingTransfer received for %x (%x) n: %d (%p) at %lld\n",
                   getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);
        writeBackInfo.erase(taddr);
        DirectoryEntry *de = dir->find(addr);
        IJ(de);

        //IJ(de->isBusy());
        DEBUGPRINT("   [%s] Directory status: busy(%d) nsharers: %d owner %s for %x at %lld\n",
                   getSymbolicName(), de->isBusy(), (int)de->getNum(), de->getOwner()->getSymbolicName(), addr, globalClock);
        de->unsetBusy();
        de->setStatus(SHARED);

        sreq->destroy();
        return;
    }
    break;
    case DirtyTransfer:
    {
        IJ(writeBackInfo.find(taddr)!=writeBackInfo.end());
        DEBUGPRINT("   [%s] DirtyTransfer received for %x (%x) n: %d (%p) at %lld\n",
                   getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);
        writeBackInfo.erase(taddr);

        DirectoryEntry *de = dir->find(addr);
        IJ(de);

        //IJ(de->isBusy());
        DEBUGPRINT("   [%s] Directory status: busy(%d) nsharers: %d owner %s for %x at %lld\n",
                   getSymbolicName(), de->isBusy(), (int)de->getNum(), de->getOwner()->getSymbolicName(), addr, globalClock);
        de->unsetBusy();
        de->setStatus(EXCLUSIVE);
        sreq->destroy();
        return;
    }
    break;
    case UpgradeRequest:
    case WriteRequest:
    {
        DEBUGPRINT("   [%s] ReadEx(write) req. receive (%s) from %d at %d on %x at %lld  (%p)\n"
                   , getSymbolicName()
                   , MemOperationStr[sreq->getMemOperation()]
                   , sreq->getSrcNode()
                   , getNodeID()
                   , addr
                   , globalClock
                   , sreq);
        // Find Dir
        DirectoryEntry *de = dir->find(addr);

        IJ(de);

        if(!de->isBusy()) {
            // Case 4-1
            // If directory state is Unowned or Exclusive with requestor as
            // owner, transitions to Exclusive and returns an exclusive reply
            // to the requestor.
            if(de->getStatus()==UNOWNED || (de->getStatus()==EXCLUSIVE && de->getOwner()==sreq->msgOwner)) {
                IJ(de->getNum()==0 || de->getNum()==1);
                //de->setBusy();
                if(de->getNum()==0) {
                    de->addOwner(sreq->msgOwner);
                } else {
                    //IJ(0);
                    IJ(de->hasThis(sreq->msgOwner));
                }
                //de->clearSharers();
                de->setStatus(EXCLUSIVE);

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, ExclusiveReply);
                    nsreq->addDst(sreq->msgOwner);

                    DEBUGPRINT("   [%s] Data UNOWNED, memory read %x at %lld\n",
                               getSymbolicName(), mreq->getPAddr(), globalClock);
                    DEBUGPRINT("   [%s] Directory: busy(%d), status %d, owner %s\n",
                               getSymbolicName(), de->isBusy(), de->getStatus(), de->getOwner()->getSymbolicName());

                    DEBUGPRINT("   [%s] Exclusive Reply (after L2 access) to %d for %x at %lld  (%p, kill %p)\n",
                               getSymbolicName()
                               , sreq->msgOwner->getNodeID(), mreq->getPAddr(), globalClock, nsreq, sreq);
                    //nsreq->goDown(0, lowerLevel[0]);
                    read(nsreq);
                }

                sreq->destroy();

                //read(mreq);
                return;
            }

            // If directory state is Shared, transitions to Exclusive and a exclusive
            // reply with invalidates pending is returned to the requestor
            // Invalidations are sent to the sharers.
            if(de->getStatus()==SHARED) {

                int nSharer = 0;
                {
                    if(de->getNum()==1 && de->hasThis(sreq->msgOwner)) {
                        DEBUGPRINT("   [%s] Data in SHARED, I have it, upgrade..for %x at %lld\n",
                                   getSymbolicName(), mreq->getPAddr(), globalClock);
                    } else {
						if(inv_opt) {
							SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, Invalidation);
							de->fillDst(nsreq->dst, nsreq->dstObj, sreq->msgOwner);
							//nsreq->msgDst = NULL;
							//if(!found) {
							//	nsreq->msgDst = de->getOwner();
							//}

							nSharer = nsreq->dstObj.size();
							IJ(nSharer>0);

							DEBUGPRINT("   [%s] Data in SHARED, sending Invalidations to %d shareres for %x at %lld\n",
									getSymbolicName(), nSharer, mreq->getPAddr(), globalClock);

							nsreq->goDown(hitDelayDir, lowerLevel[0]);
							//nsreq->goDown(0, lowerLevel[0]);
						} else {
							std::set<int32_t> dst;
    						std::set<MemObj*> dstObj;
							de->fillDst(dst, dstObj, sreq->msgOwner);
							nSharer = dstObj.size();
							IJ(nSharer>0);

							for(std::set<MemObj*>::iterator it = dstObj.begin(); it!=dstObj.end(); it++) {

								SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, Invalidation);
								nsreq->clearDstNodes();
								nsreq->dstObj.clear();

								nsreq->addDstNode((*it)->getNodeID());
								nsreq->dstObj.insert((*it));


								DEBUGPRINT("   [%s] Data in SHARED, sending Invalidations to %d shareres for %x at %lld\n",
										getSymbolicName(), nSharer, mreq->getPAddr(), globalClock);

								nsreq->goDown(hitDelayDir, lowerLevel[0]);
							}
						}
					}
				}

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, ExclusiveReplyInv);
                    nsreq->addDst(sreq->msgOwner);
                    nsreq->nInv = nSharer;

                    DEBUGPRINT("   [%s] Write for shared state, send ExclusiveReply with inv pending %d to %s for %x at %lld\n",
                               getSymbolicName(), nsreq->nInv, sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                    //nsreq->goDown(0, lowerLevel[0]);
                    read(nsreq);
                }

                de->setStatus(EXCLUSIVE);
                de->clearSharers();
                de->addOwner(sreq->msgOwner);
                sreq->destroy();

                return;
            }

            // If directory state is Exclusive with another owner, transitions
            // to Busy-Exclusive with requestor as owner and sends out an
            // intervention exclusive request to the previous owner and a
            // speculative reply to the requestor.
            if(de->getStatus()==EXCLUSIVE) {

                de->setBusy();

                MemObj *prevOwner = de->getOwner();
                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, IntervExRequest);
                    nsreq->addDst(prevOwner);

                    DEBUGPRINT("   [%s] Exclusive with another owner for write, sending intervention excl to %s for %x at %lld\n",
                               getSymbolicName(), prevOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                    nsreq->goDown(hitDelayDir, lowerLevel[0]);
                    //nsreq->goDown(0, lowerLevel[0]);
                }

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, SpeculativeReply);
                    nsreq->addDst(sreq->msgOwner);

                    DEBUGPRINT("   [%s] Exclusive with another owner for write, sending speculative reply to %s for %x at %lld\n",
                               getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                    //nsreq->goDown(0, lowerLevel[0]);
                    read(nsreq);
                }

                IJ(writeBackInfo.find(taddr) == writeBackInfo.end());
                writeBackInfo[taddr] = sreq->getOriginalRequest();
                DEBUGPRINT("   [%s] to BUSY-exclusive for %x (tag %x) wbInfo: %d (%p) at %lld\n",
                           getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);
                DEBUGPRINT("   [%s] to busy-exclusive: save original request %p for %x (%x) at %lld\n",
                           getSymbolicName(), writeBackInfo[taddr], addr, taddr, globalClock);

                de->setStatus(EXCLUSIVE);
                de->clearSharers();
                de->addOwner(sreq->msgOwner);
                sreq->destroy();

                return;
            }

            IJ(0);

        } else {
            // If directory state is Busy, a negative acknowledgment is sent
            // to the requestor, who must retry the request
            //
            // NAAAAAAAAAAAAAAAAAAK!!

            SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, NAK);
            nsreq->addDst(sreq->msgOwner);

            DEBUGPRINT("   [%s] Directory BUSY, sending NAK to %s for %x at %lld\n",
                       getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

            sreq->destroy();

            nsreq->goDown(hitDelayDir, lowerLevel[0]);
            //nsreq->goDown(0, lowerLevel[0]);
            return;
        }
        
    }
    break;

    //case WriteBackRequestData:
    case TokenBackRequest:
    {
        DEBUGPRINT("   [%s] Tokenback req. receive (%s) from %d at %d on %x at %lld  (%p)\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()],
                   sreq->getSrcNode(), getNodeID(), addr, globalClock, sreq);
    }
    case WriteBackRequest:
    {
        DEBUGPRINT("   [%s] Writeback receive (%s) from %d at %d on %x at %lld  (%p)\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()],
                   sreq->getSrcNode(), getNodeID(), addr, globalClock, sreq);
        // Find Dir
        DirectoryEntry *de = dir->find(addr);

        IJ(de);

        //IJ(de->hasThis(sreq->msgOwner));
#if 0
        if(!de->hasThis(sreq->msgOwner)) {
            DEBUGPRINT("Directory cotnent %d\n", de->getNum());
            for(std::set<MemObj*>::iterator it = de->dinfo.begin(); it!=de->dinfo.end(); it++) {
                DEBUGPRINT("%s ", (*it)->getSymbolicName());
            }
            DEBUGPRINT("Owner: %s\n", de->getOwner()->getSymbolicName());
        }
#endif


        if(de->isBusy() && (de->getOwner()==sreq->msgOwner)) {
            // Deadlooock issue
            DEBUGPRINT("   [%s] Deadlock avoidance (owner %s, requestor %s) for %x at %lld\n", getSymbolicName()
                       , ((de->getOwner()==NULL)? " " : de->getOwner()->getSymbolicName())
                       , sreq->msgOwner->getSymbolicName()
                       , mreq->getPAddr(), globalClock);

            doAccessDirCB::scheduleAbs(globalClock+1, this, mreq);
            return;
            
        }

        if(!de->isBusy()) {
            if(de->getStatus()==EXCLUSIVE && de->getOwner()==sreq->msgOwner) {

                de->setStatus(UNOWNED);
                de->clearSharers();

                {
                    SMPMemRequest *nsreq = SMPMemRequest::create(this, addr, MemPush, false, 0, WriteBackExAck);
                    nsreq->addDst(sreq->msgOwner);

                    nsreq->newAddr = sreq->newAddr;
                    nsreq->invCB = sreq->invCB;
                    sreq->invCB = NULL;

                    DEBUGPRINT("   [%s] WriteBack data in EXCLUSIVE, sending WriteBackExAck to %s for %x at %lld\n",
                               getSymbolicName(),
                               sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                    nsreq->goDown(hitDelayDir, lowerLevel[0]);
                    //nsreq->goDown(0, lowerLevel[0]);
                }

                if(sreq->meshOp == WriteBackRequest) {
                    processWriteBack(mreq);
                } else {
                    sreq->destroy();
                }
                //sreq->destroy();
                return;
            } else {
                IJ(0);
                DEBUGPRINT("   [%s] WTF busy %d state %s owner %s sender %s for %x at %lld\n"
                           , getSymbolicName()
                           , de->isBusy()
                           , DirStatusStr[de->getStatus()]
                           , ((de->getOwner()==NULL) ? "":de->getOwner()->getSymbolicName())
                           , sreq->msgOwner->getSymbolicName()
                           , mreq->getPAddr()
                           , globalClock);
            }
        } else if(de->isBusy() && de->getStatus()==SHARED) {
            de->unsetBusy();

            IJ(de->getOwner());

            {
                if(writeBackInfo.find(taddr)!=writeBackInfo.end()) {
                    MemRequest *oreq = writeBackInfo[taddr];
                    IJ(oreq);
                    writeBackInfo.erase(taddr);
                    DEBUGPRINT("   [%s] writeBackInfo update for %x (%x) n: %d (%p) at %lld\n",
                               getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);

                    //SMPMemRequest *nsreq = SMPMemRequest::create(this, addr, MemRead, false, 0, SharedResponse);
                    if(oreq->getMemOperation()==MemRead) {
                        SMPMemRequest *nsreq = SMPMemRequest::create(oreq, this, true, SharedResponse);
                        nsreq->addDst(de->getOwner());
                        nsreq->msgOwner = de->getOwner();
                        DEBUGPRINT("   [%s] WriteBack READ data in BUSY-shared, sending SharedResponse to %s for %x at %lld\n",
                                   getSymbolicName(),
                                   de->getOwner()->getSymbolicName(),
                                   mreq->getPAddr(), globalClock);
                        nsreq->goDown(hitDelayDir, lowerLevel[0]);
                    } else {
                        SMPMemRequest *nsreq = SMPMemRequest::create(oreq, this, true, ExclusiveResponse);

                        nsreq->addDst(de->getOwner());
                        nsreq->msgOwner = de->getOwner();
                        DEBUGPRINT("   [%s] WriteBack WRITE data in BUSY-shared, sending SharedResponse to %s for %x at %lld\n",
                                   getSymbolicName(),
                                   de->getOwner()->getSymbolicName(),
                                   mreq->getPAddr(), globalClock);
                        nsreq->goDown(hitDelayDir, lowerLevel[0]);
                    }

                    {
                        SMPMemRequest *nsreq = SMPMemRequest::create(this, addr, MemPush, false, 0, WriteBackBusyAck);
                        nsreq->addDst(sreq->msgOwner);

                        nsreq->newAddr = sreq->newAddr;
                        nsreq->invCB = sreq->invCB;
                        sreq->invCB = NULL;

                        DEBUGPRINT("   [%s] WriteBack data in BUSY-shared, sending Writeback busy ack to requestor %s for %x at %lld\n",
                                   getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                        nsreq->goDown(hitDelayDir, lowerLevel[0]);
                        //nsreq->goDown(0, lowerLevel[0]);
                    }
                }
            }

            de->removeSharer(sreq->msgOwner);

            if(sreq->meshOp == WriteBackRequest) {
                processWriteBack(mreq);
            } else {
                sreq->destroy();
            }
            //sreq->destroy();
        } else if(de->isBusy() && de->getStatus()==EXCLUSIVE) {
            de->unsetBusy();

            IJ(de->getOwner());

            {
                IJ(writeBackInfo.find(taddr)!=writeBackInfo.end());
                MemRequest *oreq = writeBackInfo[taddr];
                IJ(oreq);
                writeBackInfo.erase(taddr);
                DEBUGPRINT("   [%s] writeBackInfo update for %x (%x) n: %d (%p) at %lld\n",
                           getSymbolicName(), addr, taddr, (int)writeBackInfo.size(), &writeBackInfo, globalClock);

                MemObj *prevOwner = de->getOwner();;
                IJ(prevOwner);
                if(oreq->getMemOperation()==MemRead) {
                    SMPMemRequest *nsreq = SMPMemRequest::create(oreq, this, true, SharedResponse);
                    //nsreq->addDst(de->getOwner());
                    //nsreq->msgOwner = de->getOwner();
                    nsreq->addDst(prevOwner);
                    nsreq->msgOwner = prevOwner;

                    DEBUGPRINT("   [%s] WriteBack (READ) data in BUSY-exclusive, sending ExclusiveResp to %s for %x at %lld\n",
                               getSymbolicName(), de->getOwner()->getSymbolicName(), mreq->getPAddr(), globalClock);

                    nsreq->goDown(hitDelayDir, lowerLevel[0]);
                } else {
                    SMPMemRequest *nsreq = SMPMemRequest::create(oreq, this, true, ExclusiveResponse);
                    nsreq->addDst(prevOwner);
                    nsreq->msgOwner = prevOwner;
                    //nsreq->addDst(de->getOwner());
                    //nsreq->msgOwner = de->getOwner();

                    DEBUGPRINT("   [%s] WriteBack (WRITE) data in BUSY-exclusive, sending ExclusiveResp to %s for %x at %lld\n",
                               getSymbolicName(), de->getOwner()->getSymbolicName(), mreq->getPAddr(), globalClock);

                    nsreq->goDown(hitDelayDir, lowerLevel[0]);
                }
                //nsreq->goDown(0, lowerLevel[0]);
                //read(nsreq);
            }
            {
                SMPMemRequest *nsreq = SMPMemRequest::create(this, addr, MemPush, false, 0, WriteBackBusyAck);
                nsreq->addDst(sreq->msgOwner);

                nsreq->newAddr = sreq->newAddr;
                nsreq->invCB = sreq->invCB;
                sreq->invCB = NULL;

                DEBUGPRINT("   [%s] WriteBack data in BUSY-exclusive, sending Writeback busy ack to requestor %s for %x at %lld\n",
                           getSymbolicName(), sreq->msgOwner->getSymbolicName(), mreq->getPAddr(), globalClock);

                nsreq->goDown(hitDelayDir, lowerLevel[0]);
                //nsreq->goDown(0, lowerLevel[0]);
            }

            //de->removeSharer(sreq->msgOwner);

            if(sreq->meshOp == WriteBackRequest) {
                processWriteBack(mreq);
            } else {
                sreq->destroy();
            }

        } else {
            IJ(0);
            DEBUGPRINT("   [%s] WTF busy %d state %d for %x at %lld\n"
                       , getSymbolicName(), de->isBusy(), de->getStatus(), mreq->getPAddr(), globalClock);
        }
    }
    break;

    default:
        break;
    };

#if 0

    // If locked (other requests using this directory
    // wait...
    if(de->isLocked()) {
        Time_t nextTry = nextDirSlot();
        if (nextTry == globalClock)
            nextTry++;
        DEBUGPRINT("   [%s] dir req. failed (locked) ty %s from %d at %d on %x at %lld  (%p)\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()], sreq->getSrcNode(), getNodeID(), addr, globalClock, sreq);
        doAccessDirCB::scheduleAbs(nextTry, this, mreq);
        return;
    }
    DEBUGPRINT("   [%s] dir req. success ty %s from %d at %d on %x at %lld  (%p)\n",
               getSymbolicName(), MemOperationStr[sreq->getMemOperation()], sreq->getSrcNode(), getNodeID(), addr, globalClock, sreq);

    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, MeshDirReply);
    nsreq->addDstNode(sreq->getSrcNode());
    nsreq->setDirInfo(de);
    de->setLock(true);

    DEBUGPRINT("   [%s] dir req. reply to %d (0x%lx)\n",
               getSymbolicName(), sreq->getSrcNode(), (uint64_t)nsreq);

    sreq->destroy();

    //printf(" [%s] send msg  from %d to on %x at %lld\n", getSymbolicName(), nsreq->getSrcNode(), addr, globalClock);
    //nsreq->dumpDstNodes();
    //lowerLevel[0]->access(nsreq);
    nsreq->goDown(hitDelayDir, lowerLevel[0]);
    return;
}
else if (sreq->meshOp == MeshDirUpdate) {
    DEBUGPRINT("   [%s] dir update ty %s receive from %d [%s] at %d on %x at %lld\n",
               getSymbolicName(), MemOperationStr[sreq->getMemOperation()], sreq->getSrcNode(), sreq->msgOwner->getSymbolicName(),
               getNodeID(), addr, globalClock);
    // Find Dir
    DirectoryEntry *de = dir->find(addr);
    IJ(de->isLocked());

    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, MeshDirUpdateAck);
    nsreq->addDstNode(sreq->getSrcNode());
    nsreq->isExcl = (de->getStatus()==INVALID);
    DEBUGPRINT("   [%s] dir previous update %s (excl %d) on %x at %lld\n",
               getSymbolicName(),DirStatusStr[de->getStatus()], nsreq->isExcl, addr, globalClock);

    if(sreq->getMemOperation()==MemRead) {
        de->setRead(sreq->msgOwner);
        DEBUGPRINT("   [%s] dir read update ty %s (now %d shareres, %s) on %x at %lld\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()], de->getNum(), DirStatusStr[de->getStatus()], addr, globalClock);
    } else {
        de->setWrite(sreq->msgOwner);
        DEBUGPRINT("   [%s] dir write update ty %s (now %d shareres) on %x at %lld\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()], de->getNum(), addr, globalClock);
    }
    //de->setRead(sreq->getSrcNode(), sreq->msgOwner->levelIdx);

    // FIXME
    // now instantly update from ack receiving side to avoid inconsistency Pugh..
    //de->setLock(false);
    nsreq->setDirInfo(de);

    sreq->destroy();
    nsreq->goDown(hitDelayDir, lowerLevel[0]);
    //printf("------------- %s %d\n", sreq->msgOwner->getSymbolicName(), sreq->msgOwner->levelIdx);
    return;
} else if (sreq->meshOp == MeshInvDirUpdate) {
    DEBUGPRINT("   [%s] dir invalidate receive from %d [%s] at %d on %x (new addr %x) at %lld\n",
               getSymbolicName(), sreq->getSrcNode(), sreq->msgOwner->getSymbolicName(),
               getNodeID(), addr, sreq->newAddr, globalClock);
    // Find Dir
    DirectoryEntry *de = dir->find(addr);

    // If locked (other requests using this directory
    // wait...
    IJ(de->isLocked());
#if 0
    if(de->isLocked()) {
        Time_t nextTry = nextDirSlot();
        if (nextTry == globalClock)
            nextTry++;
        doAccessDirCB::scheduleAbs(nextTry, this, mreq);
        DEBUGPRINT(" ******** Directory Locked %s at %x\n", getSymbolicName(), addr);
        return;
    }
#endif

    SMPMemRequest *nsreq = SMPMemRequest::create(this, addr, MemPush, false, 0, MeshInvDirAck);
    nsreq->newAddr = sreq->newAddr;
    nsreq->invCB = sreq->invCB;
    sreq->invCB = NULL;
    nsreq->addDstNode(sreq->getSrcNode());
    nsreq->msgOwner = sreq->msgOwner;

    DEBUGPRINT("   [%s] dir prev: %s \n", getSymbolicName(), DirStatusStr[de->getStatus()]);
    de->doInval(sreq->msgOwner);
    DEBUGPRINT("   [%s] dir after: %s \n", getSymbolicName(), DirStatusStr[de->getStatus()]);

    DEBUGPRINT("   [%s] dir invalidate ack sending on %x (new %x) to %s at %lld\n",
               getSymbolicName(), addr, nsreq->newAddr, sreq->msgOwner->getSymbolicName(), globalClock);

    sreq->destroy();
    nsreq->goDown(hitDelayDir, lowerLevel[0]);
    return;
} else {
    IJ(0);
    DEBUGPRINT(" %s %x from %s  what is this? %x\n", getSymbolicName(), addr, sreq->msgOwner->getSymbolicName(), sreq->meshOp);
}
#endif
}


void SMPSliceCache::returnAccess(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    //MemOperation memOp = sreq->getMemOperation();
    MeshOperation meshOp = sreq->getMeshOperation();

    switch(meshOp) {
    case ReadRequest:
    case WriteRequest:
    case UpgradeRequest:
        DEBUGPRINT("   [%s] %s received for %x from %s at %lld\n",
                   getSymbolicName(),
                   SMPMemRequest::SMPMemReqStrMap[meshOp],
                   mreq->getPAddr(), sreq->getRequestor()->getSymbolicName(), globalClock);
        //
        //read(mreq);
        doAccessDir(mreq);
        //doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
        
        break;

    case SharingWriteBack:
        DEBUGPRINT("   [%s] %s received for %x from %s at %lld\n",
                   getSymbolicName(),
                   SMPMemRequest::SMPMemReqStrMap[meshOp],
                   mreq->getPAddr(), sreq->getRequestor()->getSymbolicName(), globalClock);
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
        break;
    case SharingTransfer:
        DEBUGPRINT("   [%s] SharedTransfer received for %x from %s at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getRequestor()->getSymbolicName(), globalClock);
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
        break;

    case MeshMemAccessReply:
    {
        DEBUGPRINT("   [%s] Memory Access Reply recieved (%s) for %x at %lld\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()], mreq->getPAddr(), globalClock);
        PAddr addr = mreq->getPAddr();
        preReturnAccessCB::scheduleAbs(nextMSHRSlot(addr), this, mreq);
    }
    break;

    case DirtyTransfer:
        DEBUGPRINT("   [%s] DirtyTransfer received for %x from %s at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getRequestor()->getSymbolicName(), globalClock);
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
        break;
#if 0
    case MeshMemWriteBack:
        DEBUGPRINT("   [%s] L2cache write bcak recieved on %x from %d at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getSrcNode(), globalClock);
        processWriteBack(mreq);
        break;
#endif
        //case WriteBackRequestData:
    case TokenBackRequest:
    case WriteBackRequest:
        DEBUGPRINT("   [%s] WriteBack request received for %x from %s at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getRequestor()->getSymbolicName(), globalClock);
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
        break;

        //case ForwardRequest:
    case ExclusiveReply:
    case SharedReply:
    case IntervSharedRequest:
    case Invalidation:
    case InvalidationAck:
        //case InvalidationAckData:
    case ExclusiveReplyInv:
    case NAK:
    default:
        break;
    };

#if 0
    if(meshOp == ReadRequest) { // Me should be the home directory
        // Directory access
#if (defined COHOPT)
        if(sreq->isDstNode(getNodeID())) {
#endif
            doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
#if (defined COHOPT)
        }
#endif
    } else if(meshOp == MeshDirReply) {
        // Not my business
    } else if(meshOp == MeshMemRequest) {
        DEBUGPRINT("   [%s] L2cache access recieved on %x from %d at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getSrcNode(), globalClock);
        //access(mreq);
        read(mreq);
    } else if(meshOp == MeshMemReply) {
    } else if(meshOp == MeshMemAccessReply) {
        DEBUGPRINT("   [%s] Memory Access Reply recieved ty %s for %x at %lld\n",
                   getSymbolicName(), MemOperationStr[sreq->getMemOperation()], mreq->getPAddr(), globalClock);
        PAddr addr = mreq->getPAddr();
        preReturnAccessCB::scheduleAbs(nextMSHRSlot(addr), this, mreq);
    } else if(meshOp == MeshDirUpdate) {
        // Update directory
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
    } else if(meshOp == MeshReadDataRequest) {
        //
    } else if(meshOp == MeshReadDataReply) {
        //
    } else if(meshOp == MeshMemWriteBack) {
        DEBUGPRINT("   [%s] L2cache write bcak recieved on %x from %d at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), sreq->getSrcNode(), globalClock);
        processWriteBack(mreq);
    } else if(meshOp == MeshDirUpdateAck) {
        //
        //-------------------------- Read done -----------------------------------------
    } else if(meshOp == MeshInvRequest) {
        //
    } else if(meshOp == MeshInvReply || meshOp == MeshInvDataReply) {
        //
    } else if(meshOp == MeshInvDirUpdate) {
        doAccessDirCB::scheduleAbs(nextDirSlot(), this, mreq);
    } else if(meshOp == MeshInvDirAck) {
        //
    } else if(meshOp == MeshWriteDataRequest) {
        //
    } else {
        IJ(0);
    }
#endif

#if 0
    PAddr addr = mreq->getPAddr();

    // the MSHR needs to be accesed when the data comes back
#ifdef MSHR_BWSTATS
    mshrBWHist.inc();

    // extra BW usage of MSHR for each pending write
    // this does NOT hold for WT caches, so it is under an ifdef for now
    int32_t nPendWrites = getBankMSHR(addr)->getUsedWrites(addr);
    for(int32_t iw = 0; iw < nPendWrites; iw++) {
        mshrBWHist.inc();
        nextMSHRSlot(addr);
    }
#endif

    preReturnAccessCB::scheduleAbs(nextMSHRSlot(addr), this, mreq);
#endif
}

void SMPSliceCache::processWriteBack(MemRequest *mreq) {
    pushLine(mreq);
    //write(mreq);
#if 0
    PAddr addr = mreq->getPAddr();
    Line *l = getCacheBank(addr)->findLineNoEffect(addr);
    IJ(l);

    l->makeDirty();
#endif
}

void SMPSliceCache::preReturnAccess(MemRequest *mreq)
{
    //if (mreq->getMemOperation() == MemPush) {
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
#if 0
    if (sreq->getMeshOperation() == MeshMemPushReply) {
        DEBUGPRINT("   [%s] Memory Access Reply is PUSH (writeback) for %x at %lld\n",
                   getSymbolicName(), mreq->getPAddr(), globalClock);
        //mreq->goUp(0);
        L2writeBackReturn(mreq, 0);
        return;
    }
#endif

    PAddr addr = mreq->getPAddr();

    Line *l = getCacheBank(mreq->getPAddr())->writeLine(addr);

    if (l == 0) {
        nextBankSlot(addr); // had to check the bank if it can accept the new line
        CallbackBase *cb = doReturnAccessCB::create(this, mreq);
        DEBUGPRINT(" ************** [%s] allocating line...\n", getSymbolicName());
        l = allocateLine(mreq->getPAddr(), cb);

        if(l != 0) {
            // the allocation was successfull, no need for the callback
            cb->destroy();
        } else {
            // not possible to allocate a line, will be called back later
            DEBUGPRINT(" ************** [%s] allocate line failed\n", getSymbolicName());
            return;
        }
    }

    int32_t nPendReads = getBankMSHR(addr)->getUsedReads(addr);

    doReturnAccess(mreq);
    l->setReadMisses(nPendReads);
#if 0
    if (mreq->getMemOperation() == MemPush) {
        mreq->goUp(0);
        return;
    }

    PAddr addr = mreq->getPAddr();


    Line *l = getCacheBank(mreq->getPAddr())->writeLine(addr);

    if (l == 0) {
        nextBankSlot(addr); // had to check the bank if it can accept the new line
        CallbackBase *cb = doReturnAccessCB::create(this, mreq);
        l = allocateLine(mreq->getPAddr(), cb);

        if(l != 0) {
            // the allocation was successfull, no need for the callback
            cb->destroy();
        } else {
            // not possible to allocate a line, will be called back later
            return;
        }
    }

    int32_t nPendReads = getBankMSHR(addr)->getUsedReads(addr);

#ifdef MSHR_BWSTATS
    mshrBWHist.inc();

    // extra BW usage of MSHR for each 4 pending reads
    // this does NOT hold for WT caches, so it is under an ifdef for now
    for(int32_t ir = 0; ir < (nPendReads/4); ir++) {
        mshrBWHist.inc();
        nextMSHRSlot(addr);
    }
#endif

    doReturnAccess(mreq);
    l->setReadMisses(nPendReads);
#endif
}

void SMPSliceCache::doReturnAccess(MemRequest *mreq)
{
    I(0);
}

SMPSliceCache::Line *SMPSliceCache::allocateLine(PAddr addr, CallbackBase *cb)
{
    PAddr rpl_addr=0;
    I(getCacheBank(addr)->findLineDebug(addr) == 0);
    Line *l = getCacheBank(addr)->fillLine(addr, rpl_addr);
    lineFill.inc();

    if(l == 0) {
        doAllocateLineRetryCB::scheduleAbs(globalClock + 100, this, addr, cb);
        return 0;
    }

    if(!l->isValid()) {
        l->validate();
        return l;
    }

    // line was valid
    // at the L1, just write back if dirty and use line
    // in non-inclusiveCache, no need to invalidate line in upper levels
    if(isHighestLevel() || !inclusiveCache) {
#ifdef MSHR_BWSTATS
        // update some eviction stats
        secondaryMissHist.sample(l->getReadMisses(), 1);
        accessesHist.sample(l->getReadMisses(), l->getReadAccesses());
#endif

        if(l->isDirty()) {
            DEBUGPRINT(" ************** [%s] line allocated. writeback dirty line %x...\n", getSymbolicName(), rpl_addr);
            doWriteBack(rpl_addr);
            l->makeClean();
        }
        l->validate();
        return l;
    }

    I(inclusiveCache);
    IJ(0);

    // not highest level or non-inclusive,
    // must invalidate old line, which may take a while
    l->lock();
    I(pendInvTable.find(rpl_addr) == pendInvTable.end());
    pendInvTable[rpl_addr].outsResps = getNumCachesInUpperLevels();
    pendInvTable[rpl_addr].cb = doAllocateLineCB::create(this, addr, rpl_addr, cb);
    invUpperLevel(rpl_addr, cacheBanks[0]->getLineSize(), this);
    return 0;
}

void SMPSliceCache::doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb)
{
    Line *l = getCacheBank(addr)->findLineNoEffect(addr);
    I(l);

#ifdef MSHR_BWSTATS
    // update some eviction stats
    secondaryMissHist.sample(l->getReadMisses(), 1);
    accessesHist.sample(l->getReadMisses(), l->getReadAccesses());
#endif

    if(l->isDirty()) {
        doWriteBack(rpl_addr);
        l->makeClean();
    }

    l->validate();

    I(cb);
    cb->call();
}

void SMPSliceCache::doAllocateLineRetry(PAddr addr, CallbackBase *cb)
{
    Line *l = allocateLine(addr, cb);
    if(l)
        cb->call();
}

bool SMPSliceCache::canAcceptStore(PAddr addr)
{
    if(maxPendingWrites > 0 && pendingWrites >= maxPendingWrites) {
        nWBFull.inc();
        return false;
    }

    bool canAcceptReq = getBankMSHR(addr)->canAcceptRequest(addr, MemWrite);

    rejectedHits.add(!canAcceptReq && isInCache(addr) ? 1: 0);
    rejected.add(!canAcceptReq);

    return canAcceptReq;
}

bool SMPSliceCache::canAcceptLoad(PAddr addr)
{
    bool canAcceptReq = getBankMSHR(addr)->canAcceptRequest(addr, MemRead);

    rejectedHits.add(!canAcceptReq && isInCache(addr) ? 1: 0);
    rejected.add(!canAcceptReq);

    return canAcceptReq;
}

bool SMPSliceCache::isInCache(PAddr addr) const
{
    uint32_t index = getCacheBank(addr)->calcIndex4Addr(addr);
    uint32_t tag = getCacheBank(addr)->calcTag(addr);

    // check the cache not affecting the LRU state
    for(uint32_t i = 0; i < cacheBanks[0]->getAssoc(); i++) {
        Line *l = getCacheBank(addr)->getPLine(index+i);
        if(l->getTag() == tag)
            return true;
    }
    return false;
}

void SMPSliceCache::invalidate(PAddr addr, ushort size, MemObj *lowerCache)
{
    I(lowerCache);
    I(pendInvTable.find(addr) == pendInvTable.end());
    pendInvTable[addr].outsResps = getNumCachesInUpperLevels();
    pendInvTable[addr].cb = doInvalidateCB::create(lowerCache, addr, size);

    if (isHighestLevel()) {     // highest level, we have only to
        doInvalidate(addr, size); // invalidate the current cache
        return;
    }

    invUpperLevel(addr, size, this);
}

void SMPSliceCache::doInvalidate(PAddr addr, ushort size)
{
    I(pendInvTable.find(addr) != pendInvTable.end());
    CallbackBase *cb = 0;

    PendInvTable::iterator it = pendInvTable.find(addr);
    Entry *record = &(it->second);
    record->outsResps--;

    if(record->outsResps <= 0) {
        cb = record->cb;
        pendInvTable.erase(addr);
    }

    int32_t leftSize = size; // use signed because cacheline can be bigger
    while (leftSize > 0) {
        Line *l = getCacheBank(addr)->readLine(addr);

        if(l) {
            nextBankSlot(addr); // writing the INV bit in a Bank's line
            I(l->isValid());
            if(l->isDirty()) {
                doWriteBack(addr);
                l->makeClean();
            }
            l->invalidate();
        }

        addr += cacheBanks[0]->getLineSize();
        leftSize -= cacheBanks[0]->getLineSize();
    }

    // finished sending dirty lines to lower level,
    // wake up callback
    // should take at least as much as writeback (1)
    if(cb)
        EventScheduler::schedule((TimeDelta_t) 2,cb);
}

Time_t SMPSliceCache::getNextFreeCycle() const // TODO: change name to calcNextFreeCycle
{
    return cachePort->calcNextSlot();
}

void SMPSliceCache::doWriteBack(PAddr addr)
{
    I(0);
}

void SMPSliceCache::sendMiss(MemRequest *mreq)
{
    I(0);
}

void SMPSliceCache::wbuffAdd(PAddr addr)
{
    ID2(WBuff::const_iterator it =wbuff.find(addr));

    if(!doWBFwd)
        return;

    wbuff[addr]++;
    pendingWrites++;
    avgPendingWrites.sample(pendingWrites);

    GI(it == wbuff.end(), wbuff[addr] == 1);
}

void SMPSliceCache::wbuffRemove(PAddr addr)
{
    WBuff::iterator it = wbuff.find(addr);
    if(it == wbuff.end())
        return;

    pendingWrites--;
    I(pendingWrites >= 0);

    I(it->second > 0);
    it->second--;
    if(it->second == 0)
        wbuff.erase(addr);
}

bool SMPSliceCache::isInWBuff(PAddr addr)
{
    return (wbuff.find(addr) != wbuff.end());
}

void SMPSliceCache::dump() const
{
    double total =   readMiss.getDouble()  + readHit.getDouble()
                     + writeMiss.getDouble() + writeHit.getDouble() + 1;

    MSG("%s:total=%8.0f:rdMiss=%8.0f:rdMissRate=%5.3f:wrMiss=%8.0f:wrMissRate=%5.3f"
        ,symbolicName
        ,total
        ,readMiss.getDouble()
        ,100*readMiss.getDouble()  / total
        ,writeMiss.getDouble()
        ,100*writeMiss.getDouble() / total
       );
}

//
// WBCache: Write back cache, only writes to lower level on displacements
//
WBSMPSliceCache::WBSMPSliceCache(SMemorySystem *dms, const char *descr_section,
                                 const char *name)
    : SMPSliceCache(dms, descr_section, name)
{
    // nothing to do
}
WBSMPSliceCache::~WBSMPSliceCache()
{
    // nothing to do
}

void WBSMPSliceCache::pushLine(MemRequest *mreq)
{
    // here we are pretending there is an outstanding pushLine
    // buffer so operation finishes immediately

    // Line has to be pushed from a higher level to this level
    I(!isHighestLevel());
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    linePush.inc();

    nextBankSlot(mreq->getPAddr());
    Line *l = getCacheBank(mreq->getPAddr())->writeLine(mreq->getPAddr());

    if (inclusiveCache || l != 0) {
        // l == 0 if the upper level is sending a push due to a
        // displacement of the lower level
        if (l != 0) {
            DEBUGPRINT("   [%s] L2cache write back (cache hit) %x %lld\n",
                       getSymbolicName(), mreq->getPAddr(), globalClock);
            l->validate();
            l->makeDirty();
        }

        nextCacheSlot();
        sreq->destroy();
        //mreq->goUp(0);
        //L2writeBackReturn(mreq, 0);
        return;
    }

    // cache is not inclusive and line does not exist: push line directly to lower level
    I(!inclusiveCache && l == 0);

    //mreq->goDown(0, lowerLevel[0]);
    //SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, MeshMemPush);
    SMPMemRequest *nsreq = SMPMemRequest::create(this, sreq->getPAddr(), MemPush, true, 0, MeshMemPush);
    nsreq->msgOwner = this;
    sreq->destroy();
    nsreq->goDown(0, lowerLevel[0]);
}

void WBSMPSliceCache::sendMiss(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    //printf("M\t%5d\t%10x\t%10x\t%lld\n", getNodeID(), mreq->getPAddr(), calcTag(mreq->getPAddr()), globalClock);

    SMPMemRequest *nsreq = SMPMemRequest::create(sreq, this, MeshMemAccess);
    //nsreq->saveMeshOp = sreq->meshOp;
    //sreq->destroy();
    nsreq->saveReq = sreq;

    //nsreq->mutateWriteToRead();

    DEBUGPRINT("   [%s] L2 access miss (go to Mem) for %x at %lld  (%p)\n",
               getSymbolicName(), mreq->getPAddr(), globalClock, nsreq);

    nsreq->goDown(missDelay + (nextMSHRSlot(mreq->getPAddr())-globalClock),
                  lowerLevel[0]);


    //mreq->mutateWriteToRead();
    //mreq->goDown(missDelay + (nextMSHRSlot(mreq->getPAddr())-globalClock),
    //             lowerLevel[0]);
}

void WBSMPSliceCache::doWriteBack(PAddr addr)
{
    writeBack.inc();
    //FIXME: right now we are assuming all lines are the same size

    DEBUGPRINT("   [%s] L2 writeback to memory for %x at %lld\n",
               getSymbolicName(), addr, globalClock);
    SMPMemRequest *sreq = SMPMemRequest::create(this, addr, MemPush, true, 0, MeshMemPush);
    sreq->msgOwner = this;
    sreq->goDown(1, lowerLevel[0]);

    //CBMemRequest::create(1, lowerLevel[0], MemPush, addr, 0);
}

void WBSMPSliceCache::doReturnAccess(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();
    Line *l = getCacheBank(addr)->findLineNoEffect(addr);
    IJ(l);
    l->validate();

    if( (mreq->getMemOperation() != MemRead) && (mreq->getMemOperation() != MemReadW)) {
        l->makeDirty();
        wbuffRemove(addr);
    }

    avgMissLat.sample(globalClock - mreq->getClockStamp());

    //mreq->goUp(0);
    L2requestReturn(mreq, 0);

    getBankMSHR(addr)->retire(addr);
}


#if 0
// WTCache: Write through cache, always propagates writes down

WTSMPSliceCache::WTSMPSliceCache(MemorySystem *gms, const char *descr_section,
                                 const char *name)
    : SMPSliceCache(gms, descr_section, name)
{
    // nothing to do
}

WTSMPSliceCache::~WTSMPSliceCache()
{
    // nothing to do
}

void WTSMPSliceCache::doWrite(MemRequest *mreq)
{
    Line *l = getCacheBank(mreq->getPAddr())->writeLine(mreq->getPAddr());

    if(l == 0) {
        writeMissHandler(mreq);
        return;
    }

    writeHit.inc();

    writePropagateHandler(mreq); // this is not a proper miss, but a WT cache
    // always propagates writes down
}

void WTSMPSliceCache::writePropagateHandler(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();

    wbuffAdd(addr);

    if(!getBankMSHR(addr)->issue(addr))
        getBankMSHR(addr)->addEntry(addr, reexecuteDoWriteCB::create(this, mreq),
                                    /*doWriteQueuedCB::create(this, mreq)*/
                                    reexecuteDoWriteCB::create(this, mreq));
    else
        propagateDown(mreq);
}

void WTSMPSliceCache::reexecuteDoWrite(MemRequest *mreq)
{
    Line *l = getCacheBank(mreq->getPAddr())->findLine(mreq->getPAddr());

    if(l == 0)
        sendMiss(mreq);
    else
        propagateDown(mreq);
}

void WTSMPSliceCache::propagateDown(MemRequest *mreq)
{
    mreq->goDown(missDelay + (nextMSHRSlot(mreq->getPAddr()) - globalClock),
                 lowerLevel[0]);
}

void WTSMPSliceCache::sendMiss(MemRequest *mreq)
{
    Line *l = getCacheBank(mreq->getPAddr())->readLine(mreq->getPAddr());
    if (!l)
        mreq->mutateWriteToRead();
    mreq->goDown(missDelay, lowerLevel[0]);
}

void WTSMPSliceCache::doWriteBack(PAddr addr)
{
    I(0);
    // nothing to do
}

void WTSMPSliceCache::pushLine(MemRequest *mreq)
{
    I(0); // should never be called
}

void WTSMPSliceCache::doReturnAccess(MemRequest *mreq)
{
    PAddr addr = mreq->getPAddr();
    Line *l = getCacheBank(addr)->findLineNoEffect(addr);
    I(l);
    l->validate();

    if(mreq->getMemOperation() != MemRead) {
        wbuffRemove(addr);
    }

    avgMissLat.sample(globalClock - mreq->getClockStamp());
    mreq->goUp(0);

    getBankMSHR(addr)->retire(addr);
}

void WTSMPSliceCache::inclusionCheck(PAddr addr) {
    const LevelType* la  = getLowerLevel();
    MemObj*    c  = (*la)[0];
    const LevelType* lb = c->getLowerLevel();
    MemObj*    cc = (*lb)[0];
    //I(((SMPCache*)cc)->findLine(addr));
}
#endif

