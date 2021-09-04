/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Luis Ceze
                  Karin Strauss
		  Jose Renau

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

#ifndef SMPSLICECACHE_H
#define SMPSLICECACHE_H

#include <queue>

#include "libmem/Cache.h"

#include "estl.h"
#include "CacheCore.h"
#include "GStats.h"
#include "Port.h"
#include "libcore/MemObj.h"
#include "libmem/MemorySystem.h"
#include "libcore/MemRequest.h"
#include "MSHR.h"
#include "Snippets.h"
#include "libll/ThreadContext.h"

#include "SMPMemRequest.h"
#include "SMemorySystem.h"

#ifdef TASKSCALAR
#include "HVersion.h"
#include "VMemReq.h"
#endif

class SMPSliceCache: public MemObj
{
protected:
    typedef CacheGeneric<CState,PAddr> CacheType;
    typedef CacheGeneric<CState,PAddr>::CacheLine Line;

    const bool inclusiveCache;
    int32_t nBanks;

    // JJO
	bool inv_opt;
    Directory *dir;
    PortGeneric *cacheDirPort;
    TimeDelta_t hitDelayDir;
    //
    HASH_MAP<PAddr, MemRequest *> writeBackInfo;

    // Statistics
    static GStatsCntr Read_U;
    static GStatsCntr Read_S;
    static GStatsCntr Read_E;
    static GStatsCntr Read_bS;
    static GStatsCntr Read_bE;

    static GStatsCntr Read_bS_H;
    static GStatsCntr Read_bE_H;

    static GStatsCntr Write_U;
    static GStatsCntr Write_S;
    static GStatsCntr Write_E;
    static GStatsCntr Write_bS;
    static GStatsCntr Write_bE;

    static GStatsCntr Write_bS_H;
    static GStatsCntr Write_bE_H;

    CacheType **cacheBanks;
    MSHR<PAddr,SMPSliceCache> **bankMSHRs;

    typedef HASH_MAP<PAddr, int> WBuff;

    WBuff wbuff;  // write buffer
    int32_t maxPendingWrites;
    int32_t pendingWrites;

    class Entry {
    public:
        int32_t outsResps;        // outstanding responses: number of caches
        // that still need to acknowledge invalidates
        CallbackBase *cb;
        Entry() {
            outsResps = 0;
            cb = 0;
        }
    };

    typedef HASH_MAP<PAddr, Entry> PendInvTable;

    PendInvTable pendInvTable; // pending invalidate table

    PortGeneric *cachePort;
    PortGeneric **bankPorts;
    PortGeneric **mshrPorts;

    int32_t defaultMask;
    TimeDelta_t missDelay;
    TimeDelta_t hitDelay;
    TimeDelta_t fwdDelay;
    bool doWBFwd;

    // BEGIN Statistics
    GStatsCntr readHalfMiss;
    GStatsCntr writeHalfMiss;
    GStatsCntr writeMiss;
    GStatsCntr readMiss;
    GStatsCntr readHit;
    GStatsCntr writeHit;
    GStatsCntr writeBack;
    GStatsCntr lineFill;
    GStatsCntr linePush;
    GStatsCntr nForwarded;
    GStatsCntr nWBFull;
    GStatsTimingAvg avgPendingWrites;
    GStatsAvg  avgMissLat;
    GStatsCntr rejected;
    GStatsCntr rejectedHits;
    GStatsCntr **nAccesses;
    // END Statistics

#ifdef MSHR_BWSTATS
    GStatsHist secondaryMissHist;
    GStatsHist accessesHist;
    GStatsPeriodicHist mshrBWHist;
    bool parallelMSHR;
#endif

    int32_t getBankId(PAddr addr) const {
        // FIXME: perhaps we should make this more efficient
        // by allowing only power of 2 nBanks
        return ((calcTag(addr)) % nBanks);
    }

    CacheType *getCacheBank(PAddr addr) const {
        return cacheBanks[getBankId(addr)];
    }

    PortGeneric *getBankPort(PAddr addr) const {
        return bankPorts[getBankId(addr)];
    }

    MSHR<PAddr,SMPSliceCache> *getBankMSHR(PAddr addr) {
        return bankMSHRs[getBankId(addr)];
    }

    Time_t nextCacheSlot() {
        return cachePort->nextSlot();
    }

    //JJO
    Time_t nextDirSlot() {
        return cacheDirPort->nextSlot();
    }

    Time_t nextBankSlot(PAddr addr) {
        return bankPorts[getBankId(addr)]->nextSlot();
    }

    Time_t nextMSHRSlot(PAddr addr) {
        return mshrPorts[getBankId(addr)]->nextSlot();
    }

    virtual void sendMiss(MemRequest *mreq) = 0;

    void doReadBank(MemRequest *mreq);
    void doRead(MemRequest *mreq);
    void doReadQueued(MemRequest *mreq);
    void doWriteBank(MemRequest *mreq);
    virtual void doWrite(MemRequest *mreq);
    void doWriteQueued(MemRequest *mreq);
    void activateOverflow(MemRequest *mreq);

    void readMissHandler(MemRequest *mreq);
    void writeMissHandler(MemRequest *mreq);

    void wbuffAdd(PAddr addr);
    void wbuffRemove(PAddr addr);
    bool isInWBuff(PAddr addr);

    Line *allocateLine(PAddr addr, CallbackBase *cb);
    void doAllocateLine(PAddr addr, PAddr rpl_addr, CallbackBase *cb);
    void doAllocateLineRetry(PAddr addr, CallbackBase *cb);

    virtual void doReturnAccess(MemRequest *mreq);
    virtual void preReturnAccess(MemRequest *mreq);

    virtual void doWriteBack(PAddr addr) = 0;
    virtual void inclusionCheck(PAddr addr) { }

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doReadBank>
    doReadBankCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doRead>
    doReadCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doWriteBank>
    doWriteBankCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doWrite>
    doWriteCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doReadQueued>
    doReadQueuedCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::doWriteQueued>
    doWriteQueuedCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *, &SMPSliceCache::activateOverflow>
    activateOverflowCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *,
            &SMPSliceCache::doReturnAccess> doReturnAccessCB;

    typedef CallbackMember1<SMPSliceCache, MemRequest *,
            &SMPSliceCache::preReturnAccess> preReturnAccessCB;

    typedef CallbackMember3<SMPSliceCache, PAddr, PAddr, CallbackBase *,
            &SMPSliceCache::doAllocateLine> doAllocateLineCB;

    typedef CallbackMember2<SMPSliceCache, PAddr, CallbackBase *,
            &SMPSliceCache::doAllocateLineRetry> doAllocateLineRetryCB;

    // JJO
    void processWriteBack(MemRequest *mreq);
    void doAccessDir(MemRequest *mreq);
    void L2requestReturn(MemRequest *mreq, TimeDelta_t d);
    //void L2writeBackReturn(MemRequest *mreq, TimeDelta_t d);

    typedef CallbackMember1<SMPSliceCache, MemRequest *,
            &SMPSliceCache::doAccessDir> doAccessDirCB;


public:
    SMPSliceCache(SMemorySystem *gms, const char *descr_section,
                  const char *name = NULL);
    virtual ~SMPSliceCache();

    // JJO
    int32_t maxNodeID;
    int32_t getMaxNodeID() {
        return maxNodeID;
    }
    int32_t getNodeID() {
        return nodeID;
    }

    static Directory **globalDirMap;

    void access(MemRequest *mreq);
    virtual void read(MemRequest *mreq);
    virtual void write(MemRequest *mreq);
    virtual void pushLine(MemRequest *mreq) = 0;
    virtual void specialOp(MemRequest *mreq);
    virtual void returnAccess(MemRequest *mreq);

    virtual bool canAcceptStore(PAddr addr);
    virtual bool canAcceptLoad(PAddr addr);

    bool isInCache(PAddr addr) const;

    // same as above plus schedule callback to doInvalidate
    void invalidate(PAddr addr, ushort size, MemObj *oc);
    void doInvalidate(PAddr addr, ushort size);

    virtual bool isCache() const {
        return true;
    }

    void dump() const;

    PAddr calcTag(PAddr addr) const {
        return cacheBanks[0]->calcTag(addr);
    }

    Time_t getNextFreeCycle() const;


    //used by SVCache
    virtual void ckpRestart(uint32_t ckpId) {}
    virtual void ckpCommit(uint32_t ckpId) {}
};

class WBSMPSliceCache : public SMPSliceCache {
protected:
    void sendMiss(MemRequest *mreq);
    void doWriteBack(PAddr addr);
    void doReturnAccess(MemRequest *mreq);

    typedef CallbackMember1<WBSMPSliceCache, MemRequest *, &WBSMPSliceCache::doReturnAccess>
    doReturnAccessCB;

public:
    WBSMPSliceCache(SMemorySystem *gms, const char *descr_section,
                    const char *name = NULL);
    ~WBSMPSliceCache();

    void pushLine(MemRequest *mreq);
};

#endif
