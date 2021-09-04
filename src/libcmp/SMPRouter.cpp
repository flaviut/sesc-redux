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

#include "SMPRouter.h"
#include "SMemorySystem.h"
#include "SMPCache.h"
#include "SMPDebug.h"

using namespace std;
	
uint64_t SMPRouter::mdestStat = 0;
uint64_t SMPRouter::mtotDestStat = 0;
uint64_t SMPRouter::sizeStat = 0;
SMPRouter::MESHCNT SMPRouter::msgStat;
	
SMPRouter::SMPRouter(SMemorySystem *dms, const char *section, const char *name)
    : MemObj(section, name)
    , CTRLmsgLatCntHist("%s_CTRLmsgCntHist", name)
    , CTRLmsgLatS1Hist( "%s_CTRLmsgS1Hist", name)
    , CTRLmsgLatS2Hist( "%s_CTRLmsgS2Hist", name)
    , DATAmsgLatCntHist("%s_DATAmsgCntHist", name)
    , DATAmsgLatS1Hist( "%s_DATAmsgS1Hist", name)
    , DATAmsgLatS2Hist( "%s_DATAmsgS2Hist", name)
{

    MemObj *ll = NULL;

    I(dms);
    ll = dms->declareMemoryObj(section, "lowerLevel");
   
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

    // JJO
    nodeID = dms->getPID();
    maxNodeID = dms->getPPN();

	if(SescConf->checkInt(section, "dimX")) {
		dimX = SescConf->getInt(section, "dimX");
	} else {
		dimX = 8;
	}
	if(SescConf->checkInt(section, "dimY")) {
		dimY = SescConf->getInt(section, "dimY");
	} else {
		dimY = 8;
	}

#ifdef SESC_ENERGY
    busEnergy = new GStatsEnergy("busEnergy", "SMPRouter", 0,
                                 MemPower,
                                 EnergyMgr::get(section,"BusEnergy",0));
#endif
}

SMPRouter::~SMPRouter()
{
}

void SMPRouter::reset() {
    SMPMemRequest::nSMPMsg.clear();
    SMPRouter::mdestStat = 0;
    SMPRouter::mtotDestStat = 0;
}

Time_t SMPRouter::getNextFreeCycle() const
{
    return busPort->nextSlot();
}

Time_t SMPRouter::nextSlot(MemRequest *mreq)
{
    return getNextFreeCycle();
}

bool SMPRouter::canAcceptStore(PAddr addr) const
{
    return true;
}


int SMPRouter::calcDist(int s, int d) {
    int sx = s%dimX;
    int sy = s/dimX;

    int dx = d%dimX;
    int dy = d/dimX;

    return abs(dx-sx)+abs(dy-sy)+1;
	//return -1;
}





void SMPRouter::access(MemRequest *mreq)
{
    read(mreq);
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
    // for reqs coming from upper level:
    // MemRead means I need to read the data, but I don't have it
    // MemReadW means I need to write the data, but I don't have it
    // MemWrite means I need to write the data, but I don't have permission
    // MemPush means I don't have space to keep the data, send it to memory
}

void SMPRouter::read(MemRequest *mreq)
{
    doReadCB::scheduleAbs(nextSlot(mreq), this, mreq);
    //doReadCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
}

void SMPRouter::write(MemRequest *mreq)
{
    IJ(0);
#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    if(pendReqsTable.find(mreq) == pendReqsTable.end()) {
        doWriteCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
    } else {
        doWrite(mreq);
    }
#endif
}

void SMPRouter::push(MemRequest *mreq)
{
    IJ(0);
// doPushCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
}

void SMPRouter::specialOp(MemRequest *mreq)
{
    I(0);
}

void SMPRouter::doRead(MemRequest *mreq)
{
    // For whatever read miss, access whatever network below.
    // First, go to home directory node to identify who's having it.
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

#if 0
    SMPMemRequest *newSreq = SMPMemRequest::create(mreq, this, true, sreq->meshOp);
    newSreq->ttl = sreq->numDstNode();
    newSreq->src = sreq->src;
    newSreq->dst.clear();
    for(std::set<int32_t>::iterator it = sreq->dst.begin(); it!=sreq->dst.end(); it++) {
        newSreq->dst.insert((*it));
    }
#endif


    int nDst = sreq->numDstNode();
    if((nDst==1) && (sreq->isDstNode(getNodeID()))) {
    	setMsgInfo(sreq);
        DEBUGPRINT("      [%s] LOCAL ACCESS RETURN at router %d from %d for %x at %lld\n",
                   getSymbolicName(), getNodeID(), sreq->getSrcNode(), sreq->getPAddr(), globalClock);

        returnAccess(sreq);
		return;
    }

    sizeStat+=sreq->getSize();
    msgStat[sreq->getMeshOperation()]++;

    if(sreq->getMeshOperation()==Invalidation) {
        IJ(sreq->getMemOperation()==MemReadW);
        int nDst = sreq->numDstNode();
        if(nDst>1) {
            mdestStat++;
            mtotDestStat+=nDst;

            PAddr addr = sreq->getPAddr();

            bool dataBack = sreq->dataBack;

            DEBUGPRINT("       [%s] Converting Inv message to %d packets for %x at %lld\n",
                       getSymbolicName(), (int)sreq->dstObj.size(), addr, globalClock);

            for(std::set<MemObj*>::iterator it = sreq->dstObj.begin(); it!=sreq->dstObj.end(); it++) {
                SMPMemRequest *nsreq = SMPMemRequest::create(sreq, Invalidation);
	
                nsreq->addDstNode((*it)->getNodeID());
                nsreq->dstObj.insert((*it));

                nsreq->dataBack = dataBack;
                dataBack = false;

                DEBUGPRINT("       [%s] Invalidate sending to %s for %x at %lld\n",
                           getSymbolicName(), (*it)->getSymbolicName(), addr, globalClock);
				
				setMsgInfo(nsreq);

				sizeStat+=nsreq->getSize();
				msgStat[nsreq->getMeshOperation()]++;

                //pCache->sendBelow(nsreq);
    			nsreq->goDown(delay, lowerLevel[0]);
            }

			sizeStat-=sreq->getSize();
			msgStat[sreq->getMeshOperation()]--;

            // Kill the request
            sreq->destroy();
            return;
        }
    }
    	
	setMsgInfo(sreq);

    sreq->goDown(delay, lowerLevel[0]);

    return;
}

void SMPRouter::finalizeRead(MemRequest *mreq)
{
    IJ(0);
    //finalizeAccess(mreq);
}

void SMPRouter::doWrite(MemRequest *mreq)
{
    IJ(0);
#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    // no need to snoop, go straight to memory
    if(!sreq->needsSnoop()) {
        goToMem(mreq);
        return;
    }

    if(pendReqsTable.find(mreq) == pendReqsTable.end()) {

        unsigned numSnoops = getNumSnoopCaches(sreq);

        // operation is starting now, add it to the pending requests buffer
        pendReqsTable[mreq] = getNumSnoopCaches(sreq);

        if(!numSnoops) {
            // nothing to snoop on this chip
            finalizeWrite(mreq);
            return;
            // TODO: even if there is only one processor on each chip,
            // request is doing two rounds: snoop and memory
        }

        // distribute requests to other caches, wait for responses
        for(uint32_t i = 0; i < upperLevel.size(); i++) {
            if(upperLevel[i] != static_cast<SMPMemRequest *>(mreq)->getRequestor()) {
                upperLevel[i]->returnAccess(mreq);
            }
        }
    }
    else {
        // operation has already been sent to other caches, receive responses

        I(pendReqsTable[mreq] > 0);
        I(pendReqsTable[mreq] <= (int) upperLevel.size());

        pendReqsTable[mreq]--;
        if(pendReqsTable[mreq] != 0) {
            // this is an intermediate response, request is not serviced yet
            return;
        }

        // this is the final response, request can go up now
        finalizeWrite(mreq);
    }
#endif
}

void SMPRouter::finalizeWrite(MemRequest *mreq)
{
    IJ(0);
    //finalizeAccess(mreq);
}

void SMPRouter::finalizeAccess(MemRequest *mreq)
{
    IJ(0);
#if 0
    PAddr addr  = mreq->getPAddr();
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    pendReqsTable.erase(mreq);

    // request completed, respond to requestor
    // (may have to come back later to go to memory)
    sreq->goUpAbs(nextSlot(mreq)+delay);
#endif
}

void SMPRouter::goToMem(MemRequest *mreq)
{
    IJ(0);
#if 0
    mreq->goDown(delay, lowerLevel[0]);
#endif
}

void SMPRouter::doPush(MemRequest *mreq)
{
    IJ(0);
#if 0
    mreq->goDown(delay, lowerLevel[0]);
#endif
}

void SMPRouter::invalidate(PAddr addr, ushort size, MemObj *oc)
{
    IJ(0);
#if 0
    invUpperLevel(addr, size, oc);
#endif
}

void SMPRouter::doInvalidate(PAddr addr, ushort size)
{
    I(0);
}

void SMPRouter::returnAccess(MemRequest *mreq)
{
    //mreq->goUpAbs(nextSlot(mreq)+delay);
    // If broadcast, filter which is not mine
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    std::set<MemObj*> dstObjCopy;
    sreq->getDstObjs(dstObjCopy);

    DEBUGPRINT("      [%s] Recieved at router %d (dst %d) from %d for %x at %lld  (%p)\n",
               getSymbolicName(), getNodeID(), (int)dstObjCopy.size(), sreq->getSrcNode(), sreq->getPAddr(), globalClock, mreq);

	// Statistics
    if( (sreq->hops>=0) ) {
        IJ(sreq->routerTime!=0);
        int msgLat = (int)(globalClock - sreq->routerTime);
        //int msgSize = sreq->getSize();
		bool isDataPacket = sreq->isDataPacket();

		if (isDataPacket) {
			DATAmsgLatCntHist.sample(sreq->hops);
			DATAmsgLatS1Hist.sample(sreq->hops, msgLat);
			DATAmsgLatS2Hist.sample(sreq->hops, msgLat*msgLat);
		} else {
			CTRLmsgLatCntHist.sample(sreq->hops);
			CTRLmsgLatS1Hist.sample(sreq->hops, msgLat);
			CTRLmsgLatS2Hist.sample(sreq->hops, msgLat*msgLat);
		}
	} else {
		IJ(sreq->getMeshOperation()==MeshMemAccessReply);
	}

    if(!dstObjCopy.empty()) {
        bool found = false;
        for(int i=(int)getUpperLevelSize()-1; i>=0; i--) {
            if(dstObjCopy.find(upperLevel[i])!=dstObjCopy.end()) {
                upperLevel[i]->returnAccess(mreq);
                found = true;
            }
        }
        IJ(found);
    } else {
        for(int i=(int)getUpperLevelSize()-1; i>=0; i--) {
            upperLevel[i]->returnAccess(mreq);
        }
    }
}


// JJO
void SMPRouter::setMsgInfo(SMPMemRequest *sreq) {
    sreq->routerTime = globalClock;

    int nDst = sreq->numDstNode();
    sreq->hops = -1;
    int src = getNodeID();

	if(nDst==1) {
		int32_t dst = sreq->getFirstDstNode();
		sreq->hops = calcDist(src, dst);
    } else if(nDst==0) {
		sreq->hops = -1;
    } else {
		IJ(0);
	}
}

