/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss

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

#include "SMPMemCtrl.h"
#include "SMemorySystem.h"
#include "SMPCache.h"
#include "SMPDebug.h"
#include "SMPRouter.h"

extern const char* MEDIA_SELECT_STR[];

SMPMemCtrl::SMPMemCtrl(SMemorySystem *dms, const char *section, const char *name)
    : MemObj(section, name)
{
    MemObj *ll = NULL;

    I(dms);
    ll = dms->declareMemoryObj(section, "lowerLevel");

    if (ll != NULL)
        addLowerLevel(ll);

    SescConf->isInt(section, "numPorts");
    SescConf->isInt(section, "portOccp");
    SescConf->isInt(section, "delay");

    delay = SescConf->getInt(section, "delay");

    char portName[100];
    sprintf(portName, "%s_bus", name);

    busPort = PortGeneric::create(portName,
                                  SescConf->getInt(section, "numPorts"),
                                  SescConf->getInt(section, "portOccp"));

#ifdef SESC_ENERGY
    busEnergy = new GStatsEnergy("busEnergy", "SMPMemCtrl", 0,
                                 MemPower,
                                 EnergyMgr::get(section,"BusEnergy",0));
#endif
}

SMPMemCtrl::~SMPMemCtrl()
{
    // do nothing
}

Time_t SMPMemCtrl::getNextFreeCycle() const
{
    return busPort->nextSlot();
}

Time_t SMPMemCtrl::nextSlot(MemRequest *mreq)
{
    return getNextFreeCycle();
}

bool SMPMemCtrl::canAcceptStore(PAddr addr) const
{
    return true;
}

void SMPMemCtrl::access(MemRequest *mreq)
{
#ifdef SESC_ENERGY
    busEnergy->inc();
#endif

#if 0
    switch(mreq->getMemOperation()) {
    case MemRead:
        read(mreq);
        break;
    case MemReadW:
    case MemWrite:
        write(mreq);
        break;
    case MemPush:
        push(mreq);
        break;
    default:
        specialOp(mreq);
        break;
    }
#endif
    read(mreq);

    // for reqs coming from upper level:
    // MemRead means I need to read the data, but I don't have it
    // MemReadW means I need to write the data, but I don't have it
    // MemWrite means I need to write the data, but I don't have permission
    // MemPush means I don't have space to keep the data, send it to memory
}

void SMPMemCtrl::read(MemRequest *mreq)
{
    doReadCB::scheduleAbs(nextSlot(mreq), this, mreq);
}

void SMPMemCtrl::write(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    IJ(0);
}

void SMPMemCtrl::push(MemRequest *mreq)
{
    doPushCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
    IJ(0);
}

void SMPMemCtrl::specialOp(MemRequest *mreq)
{
    I(0);
}

void SMPMemCtrl::doRead(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    DEBUGPRINT("         MemoryController access for %x at %lld  (%p)\n",
               sreq->getPAddr(), globalClock, mreq);

    goToMem(mreq);
}

void SMPMemCtrl::doWrite(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    IJ(0);
}

void SMPMemCtrl::doPush(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    IJ(0);
}

void SMPMemCtrl::invalidate(PAddr addr, ushort size, MemObj *oc)
{
    IJ(0);
}

void SMPMemCtrl::goToMem(MemRequest *mreq)
{
    mreq->goDown(delay, lowerLevel[0]);
}

void SMPMemCtrl::returnAccess(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    IJ(sreq!=NULL);
    DEBUGPRINT("         MemoryController return access for %x at %lld  (%p)\n",
               sreq->getPAddr(), globalClock, mreq);

    MeshOperation meshOp = sreq->getMeshOperation();
    //IJ(meshOp == MeshMemAccess|| meshOp == MeshMemPush);

    if(meshOp == MeshMemPush) {
        DEBUGPRINT("         Wrtieback from %s processed (killed) for %x at %lld\n",
                   sreq->msgOwner->getSymbolicName(), sreq->getPAddr(), globalClock);

        sreq->destroy();
        //sreq->cb->call();
        return;
    }

    meshOp = sreq->mutateMemAccess();
    sreq->routerTime = globalClock;

    SMPRouter::sizeStat+=sreq->getSize();
    SMPRouter::msgStat[meshOp]++;

    DEBUGPRINT("            MemoryController sending %s at %lld\n",
               SMPMemRequest::SMPMemReqStrMap[meshOp], globalClock);

    mreq->goUp(delay);
    //mreq->goUpAbs(nextSlot(mreq)+delay);
}

