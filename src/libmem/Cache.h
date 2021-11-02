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

#ifndef CACHE_H
#define CACHE_H

#include <queue>

#include "estl.h"
#include "CacheCore.h"
#include "GStats.h"
#include "Port.h"
#include "libcore/MemObj.h"
#include "MemorySystem.h"
#include "libcore/MemRequest.h"
#include "MSHR.h"
#include "Snippets.h"
#include "libll/ThreadContext.h"

class CState : public StateGeneric<> {
private:
    bool valid;
    bool dirty;
    bool locked;
    bool spec;
    uint32_t ckpId;
    int32_t nReadMisses; // number of pending read ops when the line was brought to the cache
    int32_t nReadAccesses;
public:
    CState() {
        valid = false;
        dirty = false;
        locked = false;
        spec = false;
        ckpId = 0;
        nReadMisses = 0;
        nReadAccesses = 0;
        clearTag();
    }
    bool isLocked() const override {
        return (locked == true);
    }
    void lock() {
        I(valid);
        locked = true;
    }
    bool isDirty() const {
        return dirty;
    }
    void makeDirty() {
        dirty = true;
    }
    void makeClean() {
        dirty = false;
    }
    bool isValid() const override {
        return valid;
    }
    void validate() {
        I(getTag());
        valid = true;
        locked = false;
        nReadMisses = 0;
        nReadAccesses = 0;
    }
    void invalidate() override {
        valid = false;
        dirty = false;
        locked = false;
        spec = false;
        ckpId = 0;
        clearTag();
    }
    void setSpec(bool s) {
        spec = s;
    }
    bool isSpec() const {
        return spec;
    }

    void incReadAccesses() {
        nReadAccesses++;
    }
    int32_t getReadAccesses() const {
        return nReadAccesses;
    }

    void setReadMisses(int32_t n) {
        nReadMisses = n;
    }
    int32_t getReadMisses() const {
        return nReadMisses;
    }

    void setCkpId(unsigned ci) {
        ckpId = ci;
    }
    uint32_t getCkpId() const {
        return ckpId;
    }
};

class Cache: public MemObj
{
protected:
    typedef CacheGeneric<CState,PAddr> CacheType;
    typedef CacheGeneric<CState,PAddr>::CacheLine Line;

    const bool inclusiveCache;
    int32_t nBanks;


    CacheType **cacheBanks;
    MSHR<PAddr,Cache> **bankMSHRs;

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
            cb = nullptr;
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

    MSHR<PAddr,Cache> *getBankMSHR(PAddr addr) {
        return bankMSHRs[getBankId(addr)];
    }

    Time_t nextCacheSlot() {
        return cachePort->nextSlot();
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

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doReadBank>
    doReadBankCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doRead>
    doReadCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doWriteBank>
    doWriteBankCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doWrite>
    doWriteCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doReadQueued>
    doReadQueuedCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::doWriteQueued>
    doWriteQueuedCB;

    typedef CallbackMember1<Cache, MemRequest *, &Cache::activateOverflow>
    activateOverflowCB;

    typedef CallbackMember1<Cache, MemRequest *,
            &Cache::doReturnAccess> doReturnAccessCB;

    typedef CallbackMember1<Cache, MemRequest *,
            &Cache::preReturnAccess> preReturnAccessCB;

    typedef CallbackMember3<Cache, PAddr, PAddr, CallbackBase *,
            &Cache::doAllocateLine> doAllocateLineCB;

    typedef CallbackMember2<Cache, PAddr, CallbackBase *,
            &Cache::doAllocateLineRetry> doAllocateLineRetryCB;

public:
    Cache(MemorySystem *gms, const char *descr_section,
          const char *name = nullptr);
    ~Cache() override;

    void access(MemRequest *mreq) override;
    virtual void read(MemRequest *mreq);
    virtual void write(MemRequest *mreq);
    virtual void pushLine(MemRequest *mreq) = 0;
    virtual void specialOp(MemRequest *mreq);
    void returnAccess(MemRequest *mreq) override;

    bool canAcceptStore(PAddr addr) override;
    bool canAcceptLoad(PAddr addr) override;

    bool isInCache(PAddr addr) const;

    // same as above plus schedule callback to doInvalidate
    void invalidate(PAddr addr, ushort size, MemObj *oc) override;
    void doInvalidate(PAddr addr, ushort size) override;

    bool isCache() const override {
        return true;
    }
    //virtual const bool isCache() const { return true; }

    void dump() const override;

    PAddr calcTag(PAddr addr) const {
        return cacheBanks[0]->calcTag(addr);
    }

    Time_t getNextFreeCycle() const override;


    //used by SVCache
    virtual void ckpRestart(uint32_t ckpId) {}
    virtual void ckpCommit(uint32_t ckpId) {}
};

class WBCache : public Cache {
protected:
    void sendMiss(MemRequest *mreq) override;
    void doWriteBack(PAddr addr) override;
    void doReturnAccess(MemRequest *mreq) override;

    typedef CallbackMember1<WBCache, MemRequest *, &WBCache::doReturnAccess>
    doReturnAccessCB;

public:
    WBCache(MemorySystem *gms, const char *descr_section,
            const char *name = nullptr);
    ~WBCache() override;

    void pushLine(MemRequest *mreq) override;
};

class WTCache : public Cache {
protected:
    void doWrite(MemRequest *mreq) override;
    void sendMiss(MemRequest *mreq) override;
    void doWriteBack(PAddr addr) override;
    void writePropagateHandler(MemRequest *mreq);
    void propagateDown(MemRequest *mreq);
    void reexecuteDoWrite(MemRequest *mreq);
    void doReturnAccess(MemRequest *mreq) override;

    typedef CallbackMember1<WTCache, MemRequest *, &WTCache::reexecuteDoWrite>
    reexecuteDoWriteCB;

    typedef CallbackMember1<WTCache, MemRequest *, &WTCache::doReturnAccess>
    doReturnAccessCB;

    void inclusionCheck(PAddr addr) override;

public:
    WTCache(MemorySystem *gms, const char *descr_section,
            const char *name = nullptr);
    ~WTCache() override;

    void pushLine(MemRequest *mreq) override;
};

class SVCache : public WBCache {
protected:
    GStatsCntr nInvLines;
    GStatsCntr nCommitedLines;
    GStatsCntr nSpecOverflow;

    bool doMSHRopt;

public:

    SVCache(MemorySystem *gms, const char *descr_section,
            const char *name = nullptr);
    ~SVCache() override;

    void doWrite(MemRequest *mreq) override;
    void preReturnAccess(MemRequest *mreq) override;
    void doReturnAccess(MemRequest *mreq) override;
    void ckpRestart(uint32_t ckpId) override;
    void ckpCommit(uint32_t ckpId) override;
};

class NICECache : public Cache
{
// a 100% hit cache, used for debugging or as main memory
protected:
    void sendMiss(MemRequest *mreq) override;
    void doWriteBack(PAddr addr) override;

public:
    NICECache(MemorySystem *gms, const char *section, const char *name = nullptr);
    ~NICECache() override;

    void read(MemRequest *mreq) override;
    void write(MemRequest *mreq) override;
    void pushLine(MemRequest *mreq) override;
    void returnAccess(MemRequest *mreq) override;
    void specialOp(MemRequest *mreq) override;
};


#endif
