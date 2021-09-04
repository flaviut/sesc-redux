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
#ifndef EXECUTIONFLOW_H
#define EXECUTIONFLOW_H

#include "nanassert.h"

#include "GFlow.h"
#include "callback.h"
#include "libcore/DInst.h"
#include "Snippets.h"
#include "ThreadContext.h"
//#include "globals.h"

class GMemoryOS;
class GMemorySystem;
class MemObj;

class ExecutionFlow : public GFlow {
private:

    ThreadContext *context;

    EventType ev;
    CallbackBase *evCB;
    int32_t evAddr;

    DInst *pendingDInst;

    void propagateDepsIfNeeded() { }

    void exeInstFast();

protected:
public:
    InstID getNextID() const {
        I(context);
        return context->getIAddr();
    }

    void addEvent(EventType e, CallbackBase *cb, int32_t addr) {
        ev     = e;
        evCB   = cb;
        evAddr = addr;
    }

    ExecutionFlow(int32_t cId, int32_t i, GMemorySystem *gms);

    void switchIn(int32_t i);
    void switchOut(int32_t i);

    int32_t currentPid(void) {
        if(!context)
            return -1;
        return context->getPid();
    }
    DInst *executePC();

    void goRabbitMode(long long n2skip=0);
    void dump(const char *str) const;
};

#endif   // EXECUTIONFLOW_H
