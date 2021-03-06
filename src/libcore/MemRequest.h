/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela

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

#ifndef MEMREQUEST_H
#define MEMREQUEST_H

#include <vector>
#include <stack>

#include "nanassert.h"
#include "libll/ThreadContext.h"
#include "Resource.h"
#include "Cluster.h"
#include "DInst.h"

#if (defined SESC_CMP)
#define stringify(name) # name
enum MemReqSrc {
    reqICache = 0,
    reqD1Cache,
    reqL2S
};
#endif

// Additional cache coherence state should not be
// added here, but in a derived class
enum MemOperation {
    MemRead = 0,
    MemWrite,
    MemPush,
    MemReadW,                  // store that misses and needs data,
    // never comes from the processor, it is
    // always converted by a cache from a MemWrite
    MemLastOp // just to get the size of the enumeration
};

#ifndef DEBUGCONDITION
#ifdef DEBUG
#define DEBUGCONDITION 0
#else
#define DEBUGCONDITION 0
#endif
#endif

#include "MemObj.h"

class MemObj;

class GMemorySystem;

class GProcessor;

class IBucket;

#ifdef SESC_SMP_DEBUG
class ReqPathEntry {
private:
    static pool<ReqPathEntry> pPool;
    friend class pool<ReqPathEntry>;

public:
    const char *memobj;
    const char *action;
    Time_t accessTime;
    int32_t specialId;

    static ReqPathEntry *create(const char *memobj,
                                const char *action,
                                Time_t accessTime,
                                int32_t specialId = -1) {
        ReqPathEntry *rpEntry = pPool.out();
        rpEntry->memobj = memobj;
        rpEntry->action = action;
        rpEntry->accessTime = accessTime;
        rpEntry->specialId = specialId;

        return rpEntry;
    }

    void destroy() {
        pPool.in(this);
    }
};
#endif

// MemRequest has lots of functionality that it is memory backend
// dependent, some fields are used by some backends, while others are
// used by other backends. This kind of specific backend functionality
// should be hidden from the shared interface.
//
// TOFIX: Someone remove the mutate (it is ugly as hell)

class MemRequest {
private:
    virtual void destroy() = 0;

protected:

    ID(static int32_t numMemReqs;)

    // Called through callback
    void access();

    void returnAccess();

    std::stack<MemObj *> memStack;

    std::stack<Time_t> clockStack;
    Time_t currentClockStamp;

    DInst *dinst;
    GProcessor *gproc;

    MemObj *currentMemObj;
    PAddr pAddr; // physical address
    MemOperation memOp;

    int32_t priority;

    Time_t l2MissDetection;

#ifdef SESC_SMP_DEBUG
    typedef std::vector<ReqPathEntry *> ReqPath;
    ReqPath reqPath;
#endif

    bool dataReq; // data or icache request
    bool prefetch;
    ID(bool acknowledged;)

    void setFields(DInst *d, MemOperation mop, MemObj *mo) {
        dinst = d;
        if (d) {
            if (d->getResource())
                gproc = d->getResource()->getCluster()->getGProcessor();
        } else {
            gproc = nullptr;
        }
        memOp = mop;
        currentMemObj = mo;
    }

    int32_t wToRLevel;

    ID(int32_t reqId;) // It can be used as a printf trigger

    MemRequest();

    virtual ~MemRequest();

    StaticCallbackMember0<MemRequest, &MemRequest::access> accessCB;
    StaticCallbackMember0<MemRequest, &MemRequest::returnAccess> returnAccessCB;

public:
    ID(int32_t id() const {
        return reqId;
    })

    MemOperation getMemOperation() const {
        return memOp;
    }

    //Call for WB cache BEFORE pushing next level
    void mutateWriteToRead() {
        if (memOp == MemWrite) {
            I(wToRLevel == -1);
            memOp = MemReadW;
            wToRLevel = memStack.size();
        }
    }

    void mutateReadToWrite() { // Justification for this in SMPCache.cpp
        if (wToRLevel == (short) memStack.size()) {
            memOp = MemWrite;
            wToRLevel = -1;
        }
    }

    void setL2MissDetection(Time_t time) {
        l2MissDetection = time;
    }

    Time_t getL2MissDetection() const {
        return l2MissDetection;
    }

    bool isDataReq() const {
        return dataReq;
    }

    bool isPrefetch() const {
        return prefetch;
    }

    void markPrefetch() {
        I(!prefetch);
        prefetch = true;
    }

    void setPriority(int32_t p) {
        priority = p;
    }

    int32_t getPriority() const {
        return priority;
    }

#ifdef SESC_DSM // libdsm stuff
    // local is true by default
    bool isLocal() const {
        return local;
    }
    void setLocal()  {
        local = true;
    }
    void setRemote() {
        local = false;
    }

    // msg is empty by default
    Message *getMessage() {
        return msg;
    }
    void setMessage(Message *newmsg) {
        msg = newmsg;
    }
#endif

    bool isWrite() const {
        return wToRLevel != -1 || memOp == MemWrite;
    }

    void goDown(TimeDelta_t lat, MemObj *newMemObj) {
        memStack.push(currentMemObj);
        clockStack.push(globalClock);
#ifdef SESC_SMP_DEBUG
        registerVisit(currentMemObj, "goDown", globalClock);
#endif
        currentMemObj = newMemObj;
        accessCB.schedule(lat);
    }

    void goDownAbs(Time_t time, MemObj *newMemObj) {
        memStack.push(currentMemObj);
        clockStack.push(globalClock);
#ifdef SESC_SMP_DEBUG
        registerVisit(currentMemObj, "goDownAbs", globalClock);
#endif
        currentMemObj = newMemObj;
        accessCB.scheduleAbs(time);
    }

    bool isTopLevel() const {
        return memStack.empty();
    }

#if (defined SESC_CMP)

    void stitchMemObj(MemObj *obj) {
        MemObj *t = memStack.top();
        memStack.pop();
        memStack.push(obj);
    }

    MemObj *getMemStackTop() {
        if (memStack.empty()) {
            return nullptr;
        }
        return memStack.top();
    }

#endif

    void goUp(TimeDelta_t lat) {

        if (memStack.empty()) {
            ack(lat);
            destroy();
            return;
        }

#ifdef SESC_SMP_DEBUG
        registerVisit(currentMemObj, "goUp", globalClock);
#endif

        currentMemObj = memStack.top();
        memStack.pop();

        I(!clockStack.empty());
        currentClockStamp = clockStack.top();
        clockStack.pop();

        returnAccessCB.schedule(lat);
    }

    void goUpAbs(Time_t time) {
        if (memStack.empty()) {
            ack(time - globalClock);
            destroy();
            return;
        }

#ifdef SESC_SMP_DEBUG
        registerVisit(currentMemObj, "goUpAbs", globalClock);
#endif

        currentMemObj = memStack.top();
        memStack.pop();

        I(!clockStack.empty());
        currentClockStamp = clockStack.top();
        clockStack.pop();

        returnAccessCB.scheduleAbs(time);
    }

    void setClockStamp(Time_t cs) {
        currentClockStamp = cs;
    }

    Time_t getClockStamp() const {
        return currentClockStamp;
    }

    DInst *getDInst() {
        return dinst;
    }

    GProcessor *getGProcessor() {
        return gproc;
    }

    MemObj *getCurrentMemObj() const {
        return currentMemObj;
    }


    void setCurrentMemObj(MemObj *obj) {
        currentMemObj = obj;
    }

    PAddr getPAddr() const {
        return pAddr;
    }

    void setPAddr(PAddr a) {
        pAddr = a;
    }

    virtual VAddr getVaddr() const = 0;

    virtual void ack(TimeDelta_t lat) = 0;

#ifdef SESC_SMP_DEBUG
    void registerVisit(MemObj *memobj, const char *action, Time_t time)
    {
        ReqPathEntry *entry = ReqPathEntry::create(memobj->getSymbolicName(),
                              action,
                              time);
        reqPath.push_back(entry);
    }

    void registerVisit(const char *name, const char *action, Time_t time, int32_t sId)
    {
        ReqPathEntry *entry = ReqPathEntry::create(name, action, time, sId);
        reqPath.push_back(entry);
    }

    void dumpPathSumm()
    {
        MSG("dumping path for %p: 0x%08x", this, (unsigned) getPAddr());
        MSG("%d entries", (unsigned) reqPath.size());
        MSG("0: %s/%s -- %lld (abs)", reqPath[0]->memobj, reqPath[0]->action,
            reqPath[0]->accessTime);
        if(reqPath.size() > 1) {
            MSG("1: %s/%s -- +%lld", reqPath[1]->memobj, reqPath[1]->action,
                reqPath[1]->accessTime - reqPath[0]->accessTime);
        }

        if(reqPath.size() > 2) {
            MSG("...");

            int32_t i = reqPath.size() - 1;
            if(reqPath[i]->specialId == -1) {
                MSG("%d: %s/%s - +%lld", i, reqPath[i]->memobj, reqPath[i]->action,
                    reqPath[i]->accessTime - reqPath[i-1]->accessTime);
            } else {
                MSG("%d: %s/%s(%d) - +%lld", i, reqPath[i]->memobj, reqPath[i]->action,
                    reqPath[i]->specialId,
                    reqPath[i]->accessTime - reqPath[i-1]->accessTime);
            }
        }
    }

    void dumpPath()
    {
        MSG("dumping path for %p: 0x%08x", this, (unsigned) getPAddr());
        MSG("0: %s/%s -- %lld (abs)", reqPath[0]->memobj, reqPath[0]->action,
            reqPath[0]->accessTime);
        for(unsigned i = 1; i < reqPath.size(); i++) {
            if(reqPath[i]->specialId == -1) {
                MSG("%d: %s/%s - +%lld", i, reqPath[i]->memobj, reqPath[i]->action,
                    reqPath[i]->accessTime - reqPath[i-1]->accessTime);
            } else {
                MSG("%d: %s/%s(%d) - +%lld", i, reqPath[i]->memobj, reqPath[i]->action,
                    reqPath[i]->specialId,
                    reqPath[i]->accessTime - reqPath[i-1]->accessTime);
            }
        }
    }

    void clearPath()
    {
        for(unsigned i = 0; i < reqPath.size(); i++)
            reqPath[i]->destroy();

        reqPath.clear();
    }
#endif // SESC_SMP_DEBUG
};

class DMemRequest : public MemRequest {
    // MemRequest specialized for dcache
private:
    static pool<DMemRequest, true> actPool;

    friend class pool<DMemRequest, true>;

    void destroy() override;

    static void dinstAck(DInst *dinst, MemOperation memOp, TimeDelta_t lat);

protected:
public:
    static void create(DInst *dinst, GMemorySystem *gmem, MemOperation mop);

    VAddr getVaddr() const override;

    void ack(TimeDelta_t lat) override;
};

class IMemRequest : public MemRequest {
    // MemRequest specialed for icache
private:
    static pool<IMemRequest, true> actPool;

    friend class pool<IMemRequest, true>;

    IBucket *buffer;

    void destroy() override;

protected:
public:
    static void create(DInst *dinst, GMemorySystem *gmem, IBucket *buffer);

    VAddr getVaddr() const override;

    void ack(TimeDelta_t lat) override;
};

class CBMemRequest : public MemRequest {
    // Callback MemRequest. Ideal for internal memory requests
private:
    static pool<CBMemRequest, true> actPool;

    friend class pool<CBMemRequest, true>;

    CallbackBase *cb;

    void destroy() override;

protected:
public:
    static CBMemRequest *create(TimeDelta_t lat, MemObj *m, MemOperation mop, PAddr addr, CallbackBase *cb);

    VAddr getVaddr() const override;

    void ack(TimeDelta_t lat) override;
};

class StaticCBMemRequest : public MemRequest {
    // Callback MemRequest. Ideal for internal memory requests
private:
    void destroy() override;

    StaticCallbackBase *cb;
    bool ackDone;

protected:
public:
    StaticCBMemRequest(StaticCallbackBase *cb);

    void launch(TimeDelta_t lat, MemObj *m, MemOperation mop, PAddr addr);

    VAddr getVaddr() const override;

    void ack(TimeDelta_t lat) override;
};

class MemRequestHashFunc {
public:
    size_t operator()(const MemRequest *mreq) const {
        auto val = (size_t) mreq;
        return val >> 2;
    }
};

#endif   // MEMREQUEST_H
