/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Milos Prvulovic
                  Smruti Sarangi
                  Luis Ceze

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

#ifndef FETCHENGINE_H
#define FETCHENGINE_H

#include "nanassert.h"

#include "libll/ExecutionFlow.h"
#include "BPred.h"
#include "GStats.h"


class GMemorySystem;
class IBucket;
class GProcessor;

class FetchEngine {
private:
    // Static data
    static long long nInst2Sim;
    static long long totalnInst;

    const int32_t Id;
    const int32_t cpuId;

    GMemorySystem *gms;
    GProcessor *gproc;

    Pid_t pid;

    BPredictor *bpred;

    ExecutionFlow flow;

    ushort FetchWidth;
    ushort bbSize;
    ushort fbSize;
    ushort fbSizeBB;

    ushort BB4Cycle;
    ushort maxBB;
    TimeDelta_t BTACDelay;
    TimeDelta_t IL1HitDelay;

    // InstID of the address that generated a misprediction
    InstID missInstID;
    Time_t missFetchTime;

#ifdef SESC_MISPATH
    bool issueWrongPath;
#endif
    bool enableICache;



protected:
    void instFetched(DInst *dinst) {
#ifdef SESC_BAAD
        dinst->setFetch1Time();
#endif

    }
    bool processBranch(DInst *dinst, ushort n2Fetched);

    // ******************* Statistics section
    GStatsAvg  avgBranchTime;
    GStatsAvg  avgInstsFetched;
    GStatsCntr nDelayInst1;
    GStatsCntr nDelayInst2;
    GStatsCntr nFetched;
    GStatsCntr nBTAC;
    GStatsHist szBB;
    GStatsHist szFB;
    GStatsHist szFS;
    long long nGradInsts;
    long long nWPathInsts;
    // *******************

public:
    FetchEngine(int32_t cId, int32_t i
                ,GMemorySystem *gms
                ,GProcessor *gp
                ,FetchEngine *fe = 0);
    ~FetchEngine();

    void addEvent(EventType ev, CallbackBase *cb, int32_t vaddr) {
        flow.addEvent(ev,cb,vaddr);
    }


    // Fills the current fetch buffer.
    //  Always fetches at most fetchWidth instructions
    void fetch(IBucket *buffer, int32_t fetchMax = -1);

    // Fake fetch. Fills the buffer with fake (mispredicted) instructions
    // Only active is SESC_MISPATH def'd
    void fakeFetch(IBucket *buffer, int32_t fetchMax = -1);
    void realFetch(IBucket *buffer, int32_t fetchMax = -1);

    // -1 if there is no pid
    Pid_t getPid() const {
        return pid;
    }

    void goRabbitMode(long long n2Skip) {
        flow.goRabbitMode(n2Skip);
    }

    void unBlockFetch();
    StaticCallbackMember0<FetchEngine,&FetchEngine::unBlockFetch> unBlockFetchCB;

    void dump(const char *str) const;

    void switchIn(Pid_t i);
    void switchOut(Pid_t i);
    long long getAndClearnGradInsts() {
        long long tmp = nGradInsts;
        nGradInsts=0;
        return tmp;
    }
    long long getAndClearnWPathInsts() {
        long long tmp = nWPathInsts;
        nWPathInsts=0;
        return tmp;
    }

    int32_t getCPUId() const {
        return cpuId;
    }
    GProcessor* getGProcessor() {
        return gproc;
    }

    BPredictor *getBPred() const {
        return bpred;
    }
    GFlow *getFlow() {
        return &flow ;
    }

    bool hasWork() const {
        return pid >= 0;
    }

    static void setnInst2Sim(long long a) {
        nInst2Sim = a;
    }
};

#endif   // FETCHENGINE_H
