/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
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

#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

#include "nanassert.h"

#include "GProcessor.h"
#include "Pipeline.h"
#include "FetchEngine.h"

class Processor : public GProcessor {
private:
    FetchEngine IFID;
    PipeQueue pipeQ;

    int32_t spaceInInstQueue;

    MemObj *l1Cache;

    DInst *RAT[NumArchRegs];

protected:


    // BEGIN VIRTUAL FUNCTIONS of GProcessor
    DInst **getRAT(const int32_t contextId) override;

    FetchEngine *currentFlow() override;

    void switchIn(Pid_t pid) override;

    void switchOut(Pid_t pid) override;

    size_t availableFlows() const override;

    long long getAndClearnGradInsts(Pid_t pid) override;

    long long getAndClearnWPathInsts(Pid_t pid) override;

    void goRabbitMode(long long n2Skip) override;

    Pid_t findVictimPid() const override;

    bool hasWork() const override;

    void advanceClock() override;

    StallCause addInst(DInst *dinst) override;

    // END VIRTUAL FUNCTIONS of GProcessor
public:
    ~Processor() override;

    Processor(GMemorySystem *gm, CPU_t i);

#ifdef SESC_MISPATH
    void misBranchRestore(DInst *dinst);
#endif
};

#endif   // _PROCESSOR_H_
