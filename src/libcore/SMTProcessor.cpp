/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Milos Prvulovic
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

#include "SMTProcessor.h"

#include "SescConf.h"

#include "FetchEngine.h"
#include "libll/ExecutionFlow.h"

SMTProcessor::Fetch::Fetch(GMemorySystem *gm, CPU_t cpuID, int32_t cid, GProcessor *gproc, FetchEngine *fe)
        : IFID(cpuID, cid, gm, gproc, fe), pipeQ(cpuID) {
}

SMTProcessor::Fetch::~Fetch() {
}

SMTProcessor::SMTProcessor(GMemorySystem *gm, CPU_t i)
        : GProcessor(gm, i, SescConf->getInt("cpucore", "smtContexts", i)),
          smtContexts(SescConf->getInt("cpucore", "smtContexts", i)),
          smtFetchs4Clk(SescConf->getInt("cpucore", "smtFetchs4Clk", i)),
          smtDecodes4Clk(SescConf->getInt("cpucore", "smtDecodes4Clk", i)),
          smtIssues4Clk(SescConf->getInt("cpucore", "smtIssues4Clk", i)), firstContext(i * smtContexts),
          fetchDist("Processor(%d)_fetchDist", i) // noFetch is on GProcessor
{
    SescConf->isInt("cpucore", "smtContexts", Id);
    SescConf->isGT("cpucore", "smtContexts", 1, Id);

    SescConf->isInt("cpucore", "smtFetchs4Clk", Id);
    SescConf->isBetween("cpucore", "smtFetchs4Clk", 1, smtContexts, Id);

    SescConf->isInt("cpucore", "smtDecodes4Clk", Id);
    SescConf->isBetween("cpucore", "smtDecodes4Clk", 1, smtContexts, Id);

    SescConf->isInt("cpucore", "smtIssues4Clk", Id);
    SescConf->isBetween("cpucore", "smtIssues4Clk", 1, smtContexts, Id);

    flow.resize(smtContexts);

    auto *f = new Fetch(gm, Id, Id * smtContexts, this);
    flow[0] = f;
    gRAT = (DInst ***) malloc(sizeof(DInst ***) * smtContexts);

    for (int32_t i = 1; i < smtContexts; i++) {
        flow[i] = new Fetch(gm, Id, Id * smtContexts + i, this, &(f->IFID));
    }

    for (int32_t i = 0; i < smtContexts; i++) {
        gRAT[i] = (DInst **) malloc(sizeof(DInst **) * NumArchRegs);
        bzero(gRAT[i], sizeof(DInst *) * NumArchRegs);
    }

    cFetchId = 0;
    cDecodeId = 0;
    cIssueId = 0;

    spaceInInstQueue = InstQueueSize;
}

SMTProcessor::~SMTProcessor() {
    for (auto & it : flow) {
        delete it;
    }
}

SMTProcessor::Fetch *SMTProcessor::findFetch(Pid_t pid) const {
    for (auto it : flow) {
        if (it->IFID.getPid() == pid) {
            return it;
        }
    }
    return nullptr;
}

DInst **SMTProcessor::getRAT(const int32_t contextId) {
    I(firstContext >= contextId);
    I(contextId <= firstContext + smtContexts);

    return gRAT[contextId - firstContext];
}

FetchEngine *SMTProcessor::currentFlow() {
    return &flow[cFetchId]->IFID;
}

void SMTProcessor::switchIn(Pid_t pid) {
    for (auto & it : flow) {
        if (it->IFID.getPid() < 0) {
            // Free FetchEngine
            it->IFID.switchIn(pid);
            return;
        }
    }

    I(0); // No free entries??
}

void SMTProcessor::switchOut(Pid_t pid) {
    Fetch *fetch = findFetch(pid);

    I(fetch); // Not found??

    fetch->IFID.switchOut(pid);
}

size_t SMTProcessor::availableFlows() const {
    size_t freeEntries = 0;

    for (auto it : flow) {
        if (it->IFID.getPid() < 0)
            freeEntries++;
    }

    return freeEntries;
}

long long SMTProcessor::getAndClearnGradInsts(Pid_t pid) {
    Fetch *fetch = findFetch(pid);

    I(fetch); // Not found??

    return fetch->IFID.getAndClearnGradInsts();
}

long long SMTProcessor::getAndClearnWPathInsts(Pid_t pid) {
    Fetch *fetch = findFetch(pid);

    I(fetch); // Not found??

    return fetch->IFID.getAndClearnWPathInsts();
}

Pid_t SMTProcessor::findVictimPid() const {
    // TODO: look for a Pid (random?)
    return flow[cFetchId]->IFID.getPid();
}

void SMTProcessor::goRabbitMode(long long n2Skip) {
    flow[cFetchId]->IFID.goRabbitMode(n2Skip);
    selectFetchFlow();
}

void SMTProcessor::selectFetchFlow() {
    // ROUND-ROBIN POLICY
    for (int32_t i = 0; i < smtContexts; i++) {
        cFetchId = (cFetchId + 1) % smtContexts;
        if (flow[cFetchId]->IFID.getPid() >= 0)
            return;
    }

    cFetchId = -1;
    I(hasWork());
}

void SMTProcessor::selectDecodeFlow() {
    // ROUND-ROBIN POLICY
    cDecodeId = (cDecodeId + 1) % smtContexts;
}

void SMTProcessor::selectIssueFlow() {
    // ROUND-ROBIN POLICY
    cIssueId = (cIssueId + 1) % smtContexts;
}

void SMTProcessor::advanceClock() {
    clockTicks++;

    // Fetch Stage
    int32_t nFetched = 0;
    int32_t fetchMax = FetchWidth;

    for (int32_t i = 0; i < smtContexts && nFetched < FetchWidth; i++) {
        selectFetchFlow();
        if (cFetchId >= 0) {
            I(flow[cFetchId]->IFID.hasWork());

            fetchMax = fetchMax > (FetchWidth - nFetched)
                       ? (FetchWidth - nFetched) : fetchMax;

            I(fetchMax > 0);

            IBucket *bucket = flow[cFetchId]->pipeQ.pipeLine.newItem();
            if (bucket) {
                flow[cFetchId]->IFID.fetch(bucket, fetchMax);
                // readyItem will be called once the bucket is fetched
                nFetched += bucket->size();
                fetchDist.sample(cFetchId, bucket->size());
            } else {
                noFetch.inc();
            }
        } else {
            I(!flow[0]->IFID.hasWork());
        }
    }

    // ID Stage (insert to instQueue)
    for (int32_t i = 0; i < smtContexts && spaceInInstQueue >= FetchWidth; i++) {
        selectDecodeFlow();

        IBucket *bucket = flow[cDecodeId]->pipeQ.pipeLine.nextItem();
        if (bucket) {
            I(!bucket->empty());

            spaceInInstQueue -= bucket->size();

            flow[cDecodeId]->pipeQ.instQueue.push(bucket);
        }
    }

    // RENAME Stage
    int32_t totalIssuedInsts = 0;
    for (int32_t i = 0; i < smtContexts && totalIssuedInsts < IssueWidth; i++) {
        selectIssueFlow();

        if (flow[cIssueId]->pipeQ.instQueue.empty())
            continue;

        int32_t issuedInsts = issue(flow[cIssueId]->pipeQ);

        totalIssuedInsts += issuedInsts;
    }
    spaceInInstQueue += totalIssuedInsts;

    retire();
}

StallCause SMTProcessor::addInst(DInst *dinst) {
    const Instruction *inst = dinst->getInst();

    DInst **RAT = gRAT[dinst->getContextId() - firstContext];

    if (InOrderCore) {
        if (RAT[inst->getSrc1()] != nullptr || RAT[inst->getSrc2()] != nullptr) {
            return SmallWinStall;
        }
    }

    StallCause sc = sharedAddInst(dinst);
    if (sc != NoStall)
        return sc;

    I(dinst->getResource() != 0); // Resource::schedule must set the resource field

    if (!dinst->isSrc2Ready()) {
        // It already has a src2 dep. It means that it is solved at
        // retirement (Memory consistency. coherence issues)
        if (RAT[inst->getSrc1()])
            RAT[inst->getSrc1()]->addSrc1(dinst);
    } else {
        if (RAT[inst->getSrc1()])
            RAT[inst->getSrc1()]->addSrc1(dinst);

        if (RAT[inst->getSrc2()])
            RAT[inst->getSrc2()]->addSrc2(dinst);
    }

    dinst->setRATEntry(&RAT[inst->getDest()]);

    RAT[inst->getDest()] = dinst;

    I(dinst->getResource());
    dinst->getResource()->getCluster()->addInst(dinst);

    return NoStall;
}

bool SMTProcessor::hasWork() const {

    if (!ROB.empty())
        return true;

    for (auto it : flow) {
        if (it->IFID.hasWork() || it->pipeQ.hasWork())
            return true;
    }

    return false;
}

#ifdef SESC_MISPATH
void SMTProcessor::misBranchRestore(DInst *dinst)
{
    clusterManager.misBranchRestore();

    for (unsigned i = 0 ; i < INSTRUCTION_MAX_DESTPOOL; i++)
        misRegPool[i] = 0;

    for(FetchContainer::const_iterator it = flow.begin();
            it != flow.end();
            it++) {
        if( &(*it)->IFID == dinst->getFetch() ) {
            (*it)->pipeQ.pipeLine.cleanMark();
            break;
        }
    }

    // TODO: try to remove from ROB
}
#endif
