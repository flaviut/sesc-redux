/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Milos Prvulovic
                  James Tuck
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

#include "libcore/OSSim.h"
#include "ExecutionFlow.h"
#include "libcore/DInst.h"
#include "libcore/GMemoryOS.h"
#include "TraceGen.h"
#include "libcore/GMemorySystem.h"
#include "libcore/MemRequest.h"

ExecutionFlow::ExecutionFlow(int32_t cId, int32_t i, GMemorySystem *gmem)
    : GFlow(i, cId, gmem)
{
    context=0;

    pendingDInst = 0;
}

void ExecutionFlow::exeInstFast()
{
    I(goingRabbit);
    I(!trainCache); // Not supported yet
    ThreadContext *thread=context;
    InstDesc *iDesc=thread->getIDesc();
    //  printf("F @0x%lx\n",iDesc->addr);
    (*iDesc)(thread);
    VAddr vaddr=thread->getDAddr();
    thread->setDAddr(0);
}

void ExecutionFlow::switchIn(int32_t i)
{
    I(!context);
    context=ThreadContext::getContext(i);
#if (defined DEBUG_SIGNALS)
    MSG("ExecutionFlow[%d] switchIn pid(%d) 0x%x @%lld", fid, i, context->getNextInstDesc()->addr, globalClock);
#endif
    I(context->getPid()==i);

    //I(!pendingDInst);
    if( pendingDInst ) {
        pendingDInst->scrap();
        pendingDInst = 0;
    }
}

void ExecutionFlow::switchOut(int32_t i)
{
    I(context);
    I(context->getPid()==i);
#if (defined DEBUG_SIGNALS)
    MSG("ExecutionFlow[%d] switchOut pid(%d) 0x%x @%lld", fid, i, context->getNextInstDesc()->addr, globalClock);
#endif
    context=0;

    //  I(!pendingDInst);
    if( pendingDInst ) {
        pendingDInst->scrap();
        pendingDInst = 0;
    }
}

void ExecutionFlow::dump(const char *str) const
{
}

DInst *ExecutionFlow::executePC()
{
    ThreadContext *thread=context;
    InstDesc *iDesc=thread->getIDesc();
#ifdef DEBUG
    //printf("S @0x%lx\n",iDesc->addr);
#endif
    iDesc=(*iDesc)(thread);
    if(!iDesc)
        return 0;
    VAddr vaddr=thread->getDAddr();
    thread->setDAddr(0);
    return DInst::createDInst(iDesc->getSescInst(),vaddr,fid,thread);
}

void ExecutionFlow::goRabbitMode(long long n2skip)
{
    int32_t nFastSims = 0;
    if( ev == FastSimBeginEvent ) {
        // Can't train cache in those cases. Cache only be train if the
        // processor did not even started to execute instructions
        trainCache = 0;
        nFastSims++;
    } else {
        I(globalClock==0);
    }

    if (n2skip) {
        I(!goingRabbit);
        goingRabbit = true;
    }

    nExec=0;

    do {
        ev=NoEvent;
        if( n2skip > 0 )
            n2skip--;

        nExec++;

        I(goingRabbit);
        exeInstFast();

        if(!context) {
            ev=NoEvent;
            goingRabbit = false;
            LOG("1.goRabbitMode::Skipped %lld instructions",nExec);
            return;
        }

        if( ev == FastSimEndEvent ) {
            nFastSims--;
        } else if( ev == FastSimBeginEvent ) {
            nFastSims++;
        } else if( ev ) {
            if( evCB )
                evCB->call();
            else {
                // Those kind of events have no callbacks because they
                // go through the memory backend. Since we are in
                // FastMode and everything happens atomically, we
                // don't need to notify the backend.
                I(ev == ReleaseEvent ||
                  ev == AcquireEvent ||
                  ev == MemFenceEvent||
                  ev == FetchOpEvent );
            }
        }

        if( osSim->enoughMTMarks1(context->getPid(),true) )
            break;
        if( n2skip == 0 && goingRabbit && osSim->enoughMarks1() && nFastSims == 0 )
            break;

    }
    while(nFastSims || n2skip || goingRabbit);

    ev=NoEvent;
    LOG("2.goRabbitMode::Skipped %lld instructions",nExec);

    if (trainCache) {
        // Finish all the outstading requests
        while(!EventScheduler::empty())
            EventScheduler::advanceClock();

        //    EventScheduler::reset();
    }

    goingRabbit = false;
}

