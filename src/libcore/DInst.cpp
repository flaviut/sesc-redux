/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
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

#include "DInst.h"

#include "Cluster.h"
#include "FetchEngine.h"
#include "OSSim.h"
#include "LDSTBuffer.h"
#include "Resource.h"

pool<DInst> DInst::dInstPool(256, "DInst");

#ifdef DEBUG
int32_t DInst::currentID=0;
#endif

#ifdef SESC_BAAD
int32_t DInst::fetch1QSize = 0;
int32_t DInst::fetch2QSize = 0;
int32_t DInst::issueQSize  = 0;
int32_t DInst::schedQSize  = 0;
int32_t DInst::exeQSize    = 0;
int32_t DInst::retireQSize = 0;

GStatsTimingHist *DInst::fetch1QHist1= 0;
GStatsTimingHist *DInst::fetch2QHist1= 0;
GStatsTimingHist *DInst::issueQHist1 = 0;
GStatsTimingHist *DInst::schedQHist1 = 0;
GStatsTimingHist *DInst::exeQHist1   = 0;
GStatsTimingHist *DInst::retireQHist1= 0;

GStatsHist *DInst::fetch1QHist2    = 0;
GStatsHist *DInst::fetch2QHist2    = 0;
GStatsHist *DInst::issueQHist2     = 0;
GStatsHist *DInst::schedQHist2     = 0;
GStatsHist *DInst::exeQHist2       = 0;
GStatsHist *DInst::retireQHist2    = 0;

GStatsHist *DInst::fetch1QHistUp   = 0;
GStatsHist *DInst::fetch2QHistUp   = 0;
GStatsHist *DInst::issueQHistUp    = 0;
GStatsHist *DInst::schedQHistUp    = 0;
GStatsHist *DInst::exeQHistUp      = 0;
GStatsHist *DInst::retireQHistUp   = 0;

GStatsHist *DInst::fetch1QHistDown = 0;
GStatsHist *DInst::fetch2QHistDown = 0;
GStatsHist *DInst::issueQHistDown  = 0;
GStatsHist *DInst::schedQHistDown  = 0;
GStatsHist *DInst::exeQHistDown    = 0;
GStatsHist *DInst::retireQHistDown = 0;

GStatsHist **DInst::avgFetch1QTime = 0;
GStatsHist **DInst::avgFetch2QTime = 0;
GStatsHist **DInst::avgIssueQTime  = 0;
GStatsHist **DInst::avgSchedQTime  = 0;
GStatsHist **DInst::avgExeQTime    = 0;
GStatsHist **DInst::avgRetireQTime = 0;

GStatsHist *DInst::brdistHist1 = 0;
#endif

DInst::DInst()
        : doAtSimTimeCB(this), doAtSelectCB(this), doAtExecutedCB(this) {
    pend[0].init(this);
    pend[1].init(this);
    I(MAX_PENDING_SOURCES == 2);
    nDeps = 0;
#ifdef DINST_TRACK_PHYS
    static ushort max_preg = 1;
    preg = max_preg++;
    GMSG(max_preg>10240,"WARNING: Too many physical register created [%d]",max_preg);
#endif

#ifdef SESC_BAAD
    if (avgFetch1QTime == 0) {
        int32_t maxType = static_cast<int>(MaxInstType);
        avgFetch1QTime = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        avgFetch2QTime = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        avgIssueQTime  = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        avgSchedQTime  = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        avgExeQTime    = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        avgRetireQTime = (GStatsHist **)malloc(sizeof(GStatsHist *)*maxType);
        for(int32_t i = 1; i < maxType ; i++) {
            avgFetch1QTime[i] = new GStatsHist("BAAD_%sFetch1QTime",Instruction::opcode2Name(static_cast<InstType>(i)));
            avgFetch2QTime[i] = new GStatsHist("BAAD_%sFetch2QTime",Instruction::opcode2Name(static_cast<InstType>(i)));
            avgIssueQTime[i]  = new GStatsHist("BAAD_%sDepQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
            avgSchedQTime[i]  = new GStatsHist("BAAD_%sSchQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
            avgExeQTime[i]    = new GStatsHist("BAAD_%sExeQTime"   ,Instruction::opcode2Name(static_cast<InstType>(i)));
            avgRetireQTime[i] = new GStatsHist("BAAD_%sComRetQTime",Instruction::opcode2Name(static_cast<InstType>(i)));
        }

        fetch1QHist1 = new GStatsTimingHist("BAAD_Fetch1QHist1");
        fetch2QHist1 = new GStatsTimingHist("BAAD_Fetch2QHist1");
        issueQHist1  = new GStatsTimingHist("BAAD_DepQHist1");
        schedQHist1  = new GStatsTimingHist("BAAD_SchQHist1");
        exeQHist1    = new GStatsTimingHist("BAAD_ExeQHist1");
        retireQHist1 = new GStatsTimingHist("BAAD_ComRetQHist1");

        fetch1QHist2 = new GStatsHist("BAAD_Fetch1QHist2");
        fetch2QHist2 = new GStatsHist("BAAD_Fetch2QHist2");
        issueQHist2  = new GStatsHist("BAAD_DepQHist2");
        schedQHist2  = new GStatsHist("BAAD_SchQHist2");
        exeQHist2    = new GStatsHist("BAAD_ExeQHist2");
        retireQHist2 = new GStatsHist("BAAD_ComRetQHist2");

        fetch1QHistUp = new GStatsHist("BAAD_Fetch1QHistUp");
        fetch2QHistUp = new GStatsHist("BAAD_Fetch2QHistUp");
        issueQHistUp  = new GStatsHist("BAAD_DepQHistUp");
        schedQHistUp  = new GStatsHist("BAAD_SchQHistUp");
        exeQHistUp    = new GStatsHist("BAAD_ExeQHistUp");
        retireQHistUp = new GStatsHist("BAAD_ComRetQHistUp");

        fetch1QHistDown = new GStatsHist("BAAD_Fetch1QHistDown");
        fetch2QHistDown = new GStatsHist("BAAD_Fetch2QHistDown");
        issueQHistDown  = new GStatsHist("BAAD_DepQHistDown");
        schedQHistDown  = new GStatsHist("BAAD_SchQHistDown");
        exeQHistDown    = new GStatsHist("BAAD_ExeQHistDown");
        retireQHistDown = new GStatsHist("BAAD_ComRetQHistDown");

        brdistHist1  = new GStatsHist("BAAD_BrDistHist1");
    }
#endif
}

void DInst::dump(const char *str) {
    fprintf(stderr, "%s:(%d)  DInst: vaddr=0x%x ", str, cId, (int) vaddr);
    if (executed) {
        fprintf(stderr, " executed");
    } else if (issued) {
        fprintf(stderr, " issued");
    } else {
        fprintf(stderr, " non-issued");
    }
    if (hasPending())
        fprintf(stderr, " has pending");
    if (!isSrc1Ready())
        fprintf(stderr, " has src1 deps");
    if (!isSrc2Ready())
        fprintf(stderr, " has src2 deps");

    inst->dump("");
}

void DInst::doAtSimTime() {
    I(resource);

    I(!isExecuted());

    I(resource->getCluster());

    if (!isStallOnLoad())
        resource->getCluster()->wakeUpDeps(this);

#ifdef SESC_BAAD
    setSchedTime();
#endif

    resource->simTime(this);
}

void DInst::doAtSelect() {
    I(resource->getCluster());
    resource->getCluster()->select(this);
}

void DInst::doAtExecuted() {
    I(RATEntry);
    if ((*RATEntry) == this)
        *RATEntry = nullptr;

    I(resource);
    resource->executed(this);
}


DInst *DInst::createDInst(const Instruction *inst, VAddr va, int32_t cId, ThreadContext *context) {
#ifdef SESC_MISPATH
    if (inst->isType(iOpInvalid))
        return 0;
#endif

    DInst *i = dInstPool.out();

    i->context = context;
    context->addDInst();

#ifdef SESC_BAAD
    i->fetch1Time   = 0;
    i->fetch2Time= 0;
    i->renameTime   = 0;
    i->issueTime    = 0;
    i->schedTime    = 0;
    i->exeTime      = 0;
#endif

    i->inst = inst;
    Prefetch(i->inst);
    i->cId = cId;
    i->wakeUpTime = 0;
    i->vaddr = va;
    i->first = nullptr;
#ifdef DEBUG
    i->ID = currentID++;
#endif
    i->resource = nullptr;
    i->RATEntry = nullptr;
    i->pendEvent = nullptr;
    i->fetch = nullptr;
    i->loadForwarded = false;
    i->issued = false;
    i->executed = false;
    i->depsAtRetire = false;
    i->deadStore = false;
    i->resolved = false;
    i->deadInst = false;
    i->waitOnMemory = false;
#ifdef SESC_MISPATH
    i->fake         = false;
#endif

#ifdef BPRED_UPDATE_RETIRE
    i->bpred    = 0;
    i->oracleID = 0;
#endif

#ifdef DINST_PARENT
    i->pend[0].setParentDInst(0);
    i->pend[1].setParentDInst(0);
#endif

    i->pend[0].isUsed = false;
    i->pend[1].isUsed = false;
#ifdef DINST_TRACK_PHYS
    i->pend[0].preg = 0;
    i->pend[1].preg = 0;
#endif

    return i;
}

DInst *DInst::createInst(InstID pc, VAddr va, int32_t cId, ThreadContext *context) {
    const Instruction *inst = Instruction::getInst(pc);
    return createDInst(inst, va, cId, context);
}

DInst *DInst::clone() {
    DInst *newDInst = createDInst(inst, vaddr, cId, context);

#ifdef SESC_BAAD
    I(0);
#endif

    return newDInst;
}

void DInst::killSilently() {
    I(getPendEvent() == 0);
    I(getResource() == 0);

#ifdef SESC_BAAD
    if (fetch2Time == 0) {
        fetch1QSize--;
    } else if (renameTime == 0) {
        fetch2QSize--;
    } else if (issueTime == 0) {
        issueQSize--;
    } else if (schedTime == 0) {
        schedQSize--;
    } else if (exeTime == 0) {
        exeQSize--;
    } else {
        retireQSize--;
    }
#endif

    markIssued();
    markExecuted();
    if (getFetch()) {
        getFetch()->unBlockFetch();
        IS(setFetch(0));
    }

    if (getInst()->isStore())
        LDSTBuffer::storeLocallyPerformed(this);

    while (hasPending()) {
        DInst *dstReady = getNextPending();

        if (!dstReady->isIssued()) {
            // Accross processor dependence
            if (dstReady->hasDepsAtRetire())
                dstReady->clearDepsAtRetire();

            I(!dstReady->hasDeps());
            continue;
        }
        if (dstReady->isExecuted()) {
            // The instruction got executed even though it has dependences. This is
            // because the instruction got silently killed (killSilently)
            if (!dstReady->hasDeps())
                dstReady->scrap();
            continue;
        }

        if (!dstReady->hasDeps()) {
            I(dstReady->isIssued());
            I(!dstReady->isExecuted());
            Resource *dstRes = dstReady->getResource();
            I(dstRes);
            dstRes->simTime(dstReady);
        }
    }

    I(!getFetch());

    if (hasDeps())
        return;

    I(nDeps == 0);   // No deps src

    I(!getFetch());
    context->delDInst();
    context = nullptr;
    dInstPool.in(this);
}

void DInst::scrap() {
    I(nDeps == 0);   // No deps src
    I(first == 0);   // no dependent instructions

#ifdef SESC_BAAD
    I(issued && executed);
#endif

#ifdef BPRED_UPDATE_RETIRE
    if (bpred) {
        bpred->predict(inst, oracleID, true);
        IS(bred = 0);
    }
#endif

    I(!getFetch());
    context->delDInst();
    context = nullptr;

    dInstPool.in(this);
}

void DInst::destroy() {
    I(nDeps == 0);   // No deps src

    I(!fetch); // if it block the fetch engine. it is unblocked again

    I(issued);
    I(executed);

    awakeRemoteInstructions();

    I(first == 0);   // no dependent instructions
    if (first) {
        LOG("Instruction pc=0x%x failed first is pc=0x%x", (int) inst->getAddr(),
            (int) first->getDInst()->inst->getAddr());
    }

    scrap();
}

void DInst::awakeRemoteInstructions() {
    while (hasPending()) {
        DInst *dstReady = getNextPending();

        I(inst->isStore());
        I(dstReady->inst->isLoad());
        I(!dstReady->isExecuted());
        I(dstReady->hasDepsAtRetire());

        I(dstReady->isSrc2Ready()); // LDSTBuffer queue in src2, free by now

        dstReady->clearDepsAtRetire();
        if (dstReady->isIssued() && !dstReady->hasDeps()) {
            Resource *dstRes = dstReady->getResource();
            // Coherence would add the latency because the cache line must be brought
            // again (in theory it must be local to dinst processor and marked dirty
            I(dstRes); // since isIssued it should have a resource
            dstRes->simTime(dstReady);
        }
    }
}

#ifdef SESC_BAAD
void DInst::setFetch1Time()
{
    I(fetch1Time == 0);
    fetch1Time = globalClock;

    fetch1QHistUp->sample(fetch1QSize);
    fetch1QSize++;

    fetch1QHist1->sample(fetch1QSize);
    fetch1QHist2->sample(fetch1QSize);
}

void DInst::setFetch2Time()
{
    I(fetch2Time == 0);
    fetch2Time = globalClock;

    fetch1QHistDown->sample(fetch1QSize);
    fetch1QSize--;
    fetch2QHistUp->sample(fetch2QSize);
    fetch2QSize++;

    fetch1QHist2->sample(fetch1QSize);

    fetch2QHist1->sample(fetch2QSize);
    fetch2QHist2->sample(fetch2QSize);
}

void DInst::setRenameTime()
{
    I(renameTime == 0);
    renameTime = globalClock;

    fetch2QHistDown->sample(fetch2QSize);
    fetch2QSize--;
    issueQHistUp->sample(issueQSize);
    issueQSize++;

    fetch2QHist2->sample(fetch2QSize);

    issueQHist1->sample(issueQSize);
    issueQHist2->sample(issueQSize);
}

void DInst::setIssueTime()
{
    I(issueTime == 0);
    issueTime = globalClock;

    issueQHistDown->sample(issueQSize);
    issueQSize--;

    schedQHistUp->sample(schedQSize);
    schedQSize++;

    issueQHist2->sample(issueQSize);

    schedQHist1->sample(schedQSize);
    schedQHist2->sample(schedQSize);
}

void DInst::setSchedTime()
{
    I(schedTime == 0);
    schedTime = globalClock;

    schedQHistDown->sample(schedQSize);
    schedQSize--;

    exeQHistUp->sample(exeQSize);
    exeQSize++;

    schedQHist2->sample(schedQSize);

    exeQHist1->sample(exeQSize);
    exeQHist2->sample(exeQSize);
}

void DInst::setExeTime()
{
    I(exeTime == 0);
    exeTime = globalClock;

    exeQHistDown->sample(exeQSize);
    exeQSize--;

    retireQHistUp->sample(retireQSize);
    retireQSize++;

    exeQHist2->sample(exeQSize);

    retireQHist1->sample(retireQSize);
    retireQHist2->sample(retireQSize);
}

void DInst::setRetireTime()
{
    I(fetch1Time);
    I(fetch2Time);
    I(renameTime);
    I(issueTime);
    I(schedTime);
    I(exeTime);

    InstType i = inst->getOpcode();
    // Based on instruction type keep statistics
    avgFetch1QTime[i]->sample(fetch2Time-fetch1Time);
    avgFetch2QTime[i]->sample(renameTime-fetch2Time);
    avgIssueQTime[i]->sample(issueTime-renameTime);
    avgSchedQTime[i]->sample(schedTime-issueTime);
    avgExeQTime[i]->sample(exeTime-schedTime);
    avgRetireQTime[i]->sample(globalClock-exeTime);

    retireQHistDown->sample(retireQSize);
    retireQSize--;


    static int32_t nInsts = 0;
    nInsts++;
    if (getFetch()) {
        // Instruction that triggered a branch miss
        brdistHist1->sample(nInsts);
        nInsts = 0;
    }

#if 0
    int32_t pc = inst->getAddr();
    if (pc) {
#ifdef SESC_BAAD
        printf("BAAD: f1T=%lld f2T=%lld rT=%lld xT=%lld cT=%lld pc=0x%x op=%d:%d s1=%d s2=%d d=%u "
               ,fetch1Time
               ,fetch2Time
               ,renameTime
               ,exeTime
               ,globalClock
               ,pc,inst->getOpcode(),inst->getSubCode()
               ,inst->getSrc1(), inst->getSrc2(), inst->getDest()
              );

        if (inst->isMemory())
            printf(" nmB a=0x%x t=%d", getVaddr(), (int)(exeTime-schedTime));
        else if (getFetch())
            printf(" mB");
        else
            printf(" nmB");
#else
        printf("TR: %lld dest=%3d src1=%3d src2=%3d lat=%3d "
               ,renameTime
               ,preg, pend[0].preg, pend[1].preg
               ,(int)(exeTime-schedTime));

        if (inst->getOpcode() == iALU)
            printf(" unit=0");
        else if (inst->getOpcode() == iMult)
            printf(" unit=1");
        else if (inst->getOpcode() == iDiv)
            printf(" unit=1");
        else if (inst->getOpcode() == iBJ)
            printf(" unit=2");
        else if (inst->getOpcode() == iLoad)
            printf(" unit=3");
        else if (inst->getOpcode() == iStore)
            printf(" unit=3");
        else
            printf(" unit=1");

        if (getFetch())
            printf(" FLUSH");
#endif

        printf("\n");
    }
#endif
}
#endif

