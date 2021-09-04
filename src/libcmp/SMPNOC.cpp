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

#include "SMPNOC.h"
#include "SMemorySystem.h"
#include "SMPCache.h"
#include "SMPDebug.h"

#include <set>

pool<SMPNOC::SMPPacket> SMPNOC::SMPPacket::rPool(5192, "SMPPacket");
SMPNOC *SMPNOC::myself = NULL;
int SMPNOC::bs_sample = 0;
list<pair<void *, pair<int, int> > > SMPNOC::returnPackets;

/* the current traffic manager instance */
TrafficManager * trafficManager = NULL;

long long GetSimTime()
{
	return trafficManager->getTime();
}

class Stats;
Stats * GetStats(const std::string & name)
{
	Stats* test =  trafficManager->getStats(name);
	if(test == 0)
	{   
		cout<<"warning statistics "<<name<<" not found"<<endl;
	}   
	return test;
}
/* printing activity factor*/
bool gPrintActivity;
int gK;//radix
int gN;//dimension
int gC;//concentration
int gNodes;
//generate nocviewer trace
bool gTrace;
ostream * gWatchOut;

SMPNOC::SMPPacket *SMPNOC::SMPPacket::Get(MemRequest *mreq, int32_t from, int32_t to, int32_t msgSize, MeshOperation meshOp, PAddr addr, Time_t clock)
{
	SMPPacket *pkt = rPool.out();
	pkt->Set(mreq, from, to, msgSize, meshOp, addr, clock);
	return pkt;
}

void SMPNOC::SMPPacket::Set(MemRequest *mreq, int32_t from, int32_t to, int32_t msgSize, MeshOperation meshOp, PAddr addr, Time_t clock)
{
	_mreq = mreq;
	_from = from;
	_to = to;
	_meshOp = meshOp;
	_msgSize = msgSize;
	_addr= addr;
	_clock = clock;
}

void SMPNOC::SMPPacket::destroy()
{
	_mreq = NULL;
	_from = -1;
	_to = -1;
	_meshOp = NOP;
	_msgSize = -1;
	_addr = 0;
	_clock = 0;
	rPool.in(this);
}

SMPNOC::SMPNOC(SMemorySystem *dms, const char *section, const char *name)
    : MemObj(section, name)
    , CTRLmsgLatCntHist("%s_MESH_CTRLmsgCntHist", name)
    , CTRLmsgLatS1Hist("%s_MESH_CTRLmsgS1Hist", name)
    , CTRLmsgLatS2Hist("%s_MESH_CTRLmsgS2Hist", name)
    , DATAmsgLatCntHist("%s_MESH_DATAmsgCntHist", name)
    , DATAmsgLatS1Hist("%s_MESH_DATAmsgS1Hist", name)
    , DATAmsgLatS2Hist("%s_MESH_DATAmsgS2Hist", name)
{
    MemObj *ll = NULL;

	if(myself!=NULL) {
		printf("Error: Cannot create more than one SMPNOC\n");
		exit(1);
	}
	myself = this;

    I(dms);
    ll = dms->declareMemoryObj(section, "lowerLevel");

    if (ll != NULL) {
        addLowerLevel(ll);
    }

	SescConf->isCharPtr(section, "booksim_config");
	const char *noc_config = SescConf->getCharPtr(section, "booksim_config");

	string fdir=SescConf->getConfDir();
	string bs_conf(noc_config);
	if(fdir.size()>0) {
		bs_conf = fdir+"/"+bs_conf;
	}

	//std::cout<<"Config: "<<bs_conf<<endl;
	

	bs_config.ParseFile( bs_conf );
	//ifstream in(bs_conf);
	//cout << "BEGIN Configuration File: " << bs_conf << endl;
	//while (!in.eof())
	//{
	//	char c;
	//	in.get(c);
	//	cout << c ;
	//}
	//cout << "END Configuration File: " << bs_conf << endl;

	/*initialize routing, traffic, injection functions
	 *      */
	InitializeRoutingMap( bs_config );

	gPrintActivity = (bs_config.GetInt("print_activity") > 0); 
	gTrace = (bs_config.GetInt("viewer_trace") > 0); 

	string watch_out_file = bs_config.GetStr( "watch_out" );
	if(watch_out_file == "") 
	{   
		gWatchOut = NULL;
	}   
	else if(watch_out_file == "-")
	{   
		gWatchOut = &cout;
	}   
	else
	{   
		gWatchOut = new ofstream(watch_out_file.c_str());
	}   


    subnets = bs_config.GetInt("subnets");
    /*To include a new network, must register the network here
     *add an else if statement with the name of the network
     */
    net.resize(subnets);
    for (int i = 0; i < subnets; ++i)
    {   
        ostringstream name;
        name << "network_" << i;
        net[i] = Network::New( bs_config, name.str() );
    }   

    /*tcc and characterize are legacy
     *not sure how to use them
     */

    assert(trafficManager == NULL);
    trafficManager = TrafficManager::New( bs_config, net ) ; 

    /*Start the simulation run
     */

    total_time = 0.0;
    gettimeofday(&start_time, NULL);

    //bool result = trafficManager->Run() ;

	SescConf->isCharPtr(section, "booksim_output");
	const char *bs_output = SescConf->getCharPtr(section, "booksim_output");
	fs_booksim.open(bs_output, ios::trunc);
	if (!fs_booksim.is_open()) {
		printf("Cannot open booksim output : %s\n", bs_output);
		exit(1);
	}

	SescConf->isInt(section, "booksim_sample");
	bs_sample = SescConf->getInt(section, "booksim_sample");
	
	trafficManager->Init(&returnPackets, &fs_booksim);

	//doAdvanceNOCCycle();

	//trafficManager->UpdateStats();
	//trafficManager->DisplayStats();



#if 0
    // JJO
    // Routers setup
    totalRNum = dms->getPPN();
    routers = new SMPNoCRouter* [totalRNum+dimX];
    for(int i=0; i<(int)totalRNum; i++) {
        routers[i] = new SMPNoCRouter(i, totalRNum, this, section, name, false);
    }
    for(int i=0; i<dimX; i++) { // Memory controllers
        routers[totalRNum+i] = new SMPNoCRouter(totalRNum+i, totalRNum, this, section, name, true);
    }

    for(int i=0; i<(int)(totalRNum+dimX); i++) {
        int ix = i%dimX;
        int iy = i/dimX;

        if(ix>=1 && ix<(dimX-1) && iy>=1 && iy<(dimY)) {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        } else if (ix>=1 && ix<(dimX-1) && iy==0) {
            routers[i]->adjRouter[NORTH] = NULL;
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        } else if (ix==0 && iy>=1 && iy<(dimY)) {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = NULL;
        } else if (iy==(dimY) && ix>=1 && ix<(dimX-1)) {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = NULL;
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        } else if (ix==(dimX-1) && iy>=1 && iy<(dimY)) {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = NULL;
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        } else if (ix==0 && iy==0) {
            routers[i]->adjRouter[NORTH] = NULL;
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = NULL;
        } else if (ix==0 && iy==(dimY)) {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = routers[ iy*dimX+ix+1 ];
            routers[i]->adjRouter[SOUTH] = NULL;
            routers[i]->adjRouter[WEST] = NULL;
        } else if (ix==(dimX-1) && iy==0) {
            routers[i]->adjRouter[NORTH] = NULL;
            routers[i]->adjRouter[EAST] = NULL;
            routers[i]->adjRouter[SOUTH] = routers[ (iy+1)*dimX+ix ];
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        } else {
            routers[i]->adjRouter[NORTH] = routers[ (iy-1)*dimX+ix ];
            routers[i]->adjRouter[EAST] = NULL;
            routers[i]->adjRouter[SOUTH] = NULL;
            routers[i]->adjRouter[WEST] = routers[ iy*dimX+ix-1 ];
        }

        routers[i]->adjRouter[LOCAL] = NULL;
    }
#endif


    //char portName[100];
    //sprintf(portName, "%s_bus", name);

    // JJO
    //busPort = PortGeneric::create(portName,
    //                              SescConf->getInt(section, "numPorts"),
    //                              SescConf->getInt(section, "portOccp"));
    //
#if 0
    int32_t occ = SescConf->getInt(section, "portOccp");
    if(occ<0) {
        busPort = new PortNPipe(portName,
                                SescConf->getInt(section, "numPorts"), 1);
    } else {
        busPort = PortGeneric::create(portName,
                                      SescConf->getInt(section, "numPorts"),
                                      SescConf->getInt(section, "portOccp"));
    }
#endif

#ifdef SESC_ENERGY
    busEnergy = new GStatsEnergy("busEnergy", "SMPNOC", 0,
                                 MemPower,
                                 EnergyMgr::get(section,"BusEnergy",0));
#endif
}


void SMPNOC::doAdvanceNOCCycle()
{
	assert(trafficManager!=NULL);
		
	trafficManager->CallEveryCycle();

	//cout<<"Flight  "<<trafficManager->IsFlitInFlight()<<" at "<<globalClock<<endl;

	while(!returnPackets.empty()) {
		void *p_pkt = returnPackets.front().first;
		int hops = returnPackets.front().second.first;
		int plat = returnPackets.front().second.second;
		returnPackets.pop_front();
		SMPPacket *packet = static_cast<SMPPacket *>(p_pkt);

		MemRequest *mreq = packet->GetMemRequest();
    	SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
		sreq->hops = hops;
		sreq->plat = plat;
		packet->destroy();

		myself->returnAccess(mreq);
	}

	if(globalClock%bs_sample==0) {
		bool test = trafficManager->Checkpoint();
		if(!test) {
			std::cout<<"Network unstable..."<<endl;
			exit(1);
		}
	}
}

void SMPNOC::PrintStat() 
{
	assert(myself!=NULL);
	myself->_PrintStat();
}

void SMPNOC::_PrintStat()
{
	trafficManager->Checkpoint();
	trafficManager->Finish();

    gettimeofday(&end_time, NULL);
    total_time = ((double)(end_time.tv_sec) + (double)(end_time.tv_usec)/1000000.0)
                 - ((double)(start_time.tv_sec) + (double)(start_time.tv_usec)/1000000.0);

    fs_booksim<<"BookSim: Total_run_time "<<total_time<<endl;

    for (int i=0; i<subnets; ++i)
    {   
        ///Power analysis
        if(bs_config.GetInt("sim_power") > 0)
        {   
            Power_Module pnet(net[i], bs_config);
            pnet.run();
        }

        delete net[i];
    }

    delete trafficManager;
    trafficManager = NULL;
	fs_booksim.close();
}

SMPNOC::~SMPNOC()
{

    // do nothing
#if 0
    for(int i=0; i<(int)totalRNum; i++) {
        delete routers[i];
    }
    delete[] routers;
#endif
}

Time_t SMPNOC::getNextFreeCycle() const
{
	assert(false);
	return 0;
}

Time_t SMPNOC::getNextFreeCycle(int32_t cycle)
{
	assert(false);
	return 0;
#if 0
    return busPort->nextSlot(cycle);
#endif
}

Time_t SMPNOC::nextSlot(MemRequest *mreq)
{
	assert(false);
	return 0;
#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);
    int32_t msgS = sreq->getSize();
    //	printf("size: %d\n", msgS);
    int32_t cycle = (msgS*8)/bw;
    if(cycle==0) cycle++;
    return getNextFreeCycle(cycle);
#endif
}

bool SMPNOC::canAcceptStore(PAddr addr) const
{
    return true;
}

void SMPNOC::access(MemRequest *mreq)
{
    GMSG(mreq->getPAddr() < 1024,
         "mreq dinst=0x%p paddr=0x%x vaddr=0x%x memOp=%d",
         mreq->getDInst(),
         (uint32_t) mreq->getPAddr(),
         (uint32_t) mreq->getVaddr(),
         mreq->getMemOperation());

    I(mreq->getPAddr() > 1024);

    read(mreq);
#if 0
#ifdef SESC_ENERGY
    busEnergy->inc();
#endif

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

    // for reqs coming from upper level:
    // MemRead means I need to read the data, but I don't have it
    // MemReadW means I need to write the data, but I don't have it
    // MemWrite means I need to write the data, but I don't have permission
    // MemPush means I don't have space to keep the data, send it to memory
#endif
}

#if 0
void SMPNOC::doInject(MemRequest *mreq)
{
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    int nDst = sreq->numDstNode();
	IJ(nDst==1);

    int32_t from = sreq->getSrcNode();
	int32_t to = sreq->getFirstDstNode();
	IJ(from>=0 && to>=0);

	MeshOperation meshOp = sreq->getMeshOperation();
	PAddr addr = sreq->getPAddr();
	
	if(!trafficManager->InjectPacket(from, 0)) {
		doInjectCB::scheduleAbs(globalClock+1, this, mreq);
	}
}
#endif

void SMPNOC::read(MemRequest *mreq)
{
    //doReadCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);


    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);


    int nDst = sreq->numDstNode();
    if(nDst>1) {
        fprintf(stderr, "No support for multicast. NoC can have only one destination.\n");
        exit(1);
    }

    int32_t from = sreq->getSrcNode();
	// FIXME
    //routers[from]->send(sreq);
	
	int32_t to = -1;
	if(nDst==0) {
		to = -1;
	} else {
		to = sreq->getFirstDstNode();
	}
	
	MeshOperation meshOp = sreq->getMeshOperation();
	PAddr addr = sreq->getPAddr();
    int32_t msgSize = sreq->getSize();  // Size in bytes

	if(to<0) {
		DEBUGPRINT("\t\t\tNoC access from %d to MEM msg %x (size %d) for %x at %lld  (%p)\n"
				, from, meshOp, msgSize, addr, globalClock, sreq);
        DEBUGPRINT("         =Send to memory from %d for %x at %lld \n", sreq->getSrcNode(), sreq->getPAddr(), globalClock);
    	IJ(meshOp == MeshMemAccess|| meshOp == MeshMemPush);
        goToMem(mreq);
	} else {

		IJ(from>=0 && to>=0);

		DEBUGPRINT("\t\t\tNoC access from %d to %d msg %x (size %d) for %x at %lld  (%p)\n"
				, from, to, meshOp, msgSize, addr, globalClock, sreq);

		SMPPacket *p = SMPPacket::Get(mreq, from, to, msgSize, meshOp, addr, globalClock);
		trafficManager->BufferPacket(from, to, 0, msgSize, (void *)p);

	//doInject(mreq);
	}
	

    return;

#if 0
    if(nDst>0) {
        // distribute requests to other caches, wait for responses
        std::set<int32_t> dstCopy;
        sreq->getDstNodes(dstCopy);
        int32_t srcCopy = sreq->getSrcNode();

        for(uint32_t i = 0; i<upperLevel.size(); i++) {
            //if(sreq->isDstNode(upperLevel[i]->getNodeID())) {
            if(dstCopy.find(upperLevel[i]->getNodeID())!=dstCopy.end()) {
                if(upperLevel[i]->getNodeID() != srcCopy) {
                    upperLevel[i]->returnAccess(mreq);
                }
                if(nDst==1) {
                    break;
                } else {
                    fprintf(stderr, "No support for multicast. NoC can have only one destination.\n");
                    exit(1);
                }
            }
        }
    } else { // go to memory
        DEBUGPRINT("         =Send to memory from %d for %x at %lld \n", sreq->getSrcNode(), sreq->getPAddr(), globalClock);
        goToMem(mreq);
    }
#endif

    /*
      if(pendReqsTable.find(mreq) == pendReqsTable.end()) {
        doReadCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
      } else {
        doRead(mreq);
      }
      */
}

void SMPNOC::write(MemRequest *mreq)
{
	assert(false);
#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    if(pendReqsTable.find(mreq) == pendReqsTable.end()) {
        doWriteCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
    } else {
        doWrite(mreq);
    }
#endif
}

void SMPNOC::push(MemRequest *mreq)
{
	assert(false);
#if 0
    doPushCB::scheduleAbs(nextSlot(mreq)+delay, this, mreq);
#endif
}

void SMPNOC::specialOp(MemRequest *mreq)
{
	assert(false);
}


void SMPNOC::doRead(MemRequest *mreq)
{
	assert(false);
//    finalizeRead(mreq);

#if 0
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    DEBUGPRINT("         NoC access from %d to (", sreq->getSrcNode());
    //DEBUGDO(sreq->dumpDstNodes());
    DEBUGDO(sreq->dumpDstNodes());
    DEBUGPRINT(") for %x at %lld  (%p)\n",
               sreq->getPAddr(), globalClock, mreq);

    int nDst = sreq->numDstNode();
    if(nDst>0) {
        // distribute requests to other caches, wait for responses
        std::set<int32_t> dstCopy;
        sreq->getDstNodes(dstCopy);
        int32_t srcCopy = sreq->getSrcNode();

        for(uint32_t i = 0; i<upperLevel.size(); i++) {
            //if(sreq->isDstNode(upperLevel[i]->getNodeID())) {
            if(dstCopy.find(upperLevel[i]->getNodeID())!=dstCopy.end()) {
                if(upperLevel[i]->getNodeID() != srcCopy) {
                    upperLevel[i]->returnAccess(mreq);
                }
                if(nDst==1) {
                    break;
                } else {
                    fprintf(stderr, "No support for multicast. NoC can have only one destination.\n");
                    exit(1);
                }
            }
        }
    } else { // go to memory
        DEBUGPRINT("         =Send to memory from %d for %x at %lld \n", sreq->getSrcNode(), sreq->getPAddr(), globalClock);
        goToMem(mreq);
    }
#endif

#if 0
    // no need to snoop, go straight to memory
    // JJO
    //if(!sreq->needsSnoop()) {
    if(sreq->getMemOperation() == MemPush) {
        goToMem(mreq);
        return;
    }

    if(pendReqsTable.find(mreq) == pendReqsTable.end()) {

        unsigned numSnoops = sreq->numDstNode();

        // operation is starting now, add it to the pending requests buffer
        pendReqsTable[mreq] = numSnoops;

        if(!numSnoops) {
            // nothing to snoop on this chip

            IJ(0);
            //finalizeRead(mreq);
            //return;

            // TODO: even if there is only one processor on each chip,
            // request is doing two rounds: snoop and memory
        }

        printf("   Meshbus access");
        printf(" from %d , %d for %x at %lld\n", sreq->getSrcNode(),sreq->numDstNode(), sreq->getPAddr(), globalClock);
        sreq->dumpDstNodes();

        // distribute requests to other caches, wait for responses
        for(uint32_t i = 0; i<upperLevel.size(); i++) {
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
        finalizeRead(mreq);
    }
#endif
}

void SMPNOC::finalizeRead(MemRequest *mreq)
{
	assert(false);
    //finalizeAccess(mreq);
}

void SMPNOC::doWrite(MemRequest *mreq)
{
	assert(false);
    //finalizeWrite(mreq);
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

void SMPNOC::finalizeWrite(MemRequest *mreq)
{
	assert(false);
    //finalizeAccess(mreq);
}

void SMPNOC::finalizeAccess(MemRequest *mreq)
{
	assert(false);
#if 0
    PAddr addr  = mreq->getPAddr();
    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    pendReqsTable.erase(mreq);

    if(sreq->meshOp==MeshReadRequest) {
        sreq->meshOp = MeshReadReply;
    }

    // request completed, respond to requestor
    // (may have to come back later to go to memory)
    sreq->goUpAbs(nextSlot(mreq)+delay);
#endif
}

void SMPNOC::goToMem(MemRequest *mreq)
{
    mreq->goDown(1, lowerLevel[0]);
    //mreq->goDown(delay, lowerLevel[0]);
}

void SMPNOC::doPush(MemRequest *mreq)
{
	assert(false);
    //mreq->goDown(delay, lowerLevel[0]);
}

void SMPNOC::invalidate(PAddr addr, ushort size, MemObj *oc)
{
	assert(false);
    //invUpperLevel(addr, size, oc);
}

void SMPNOC::doInvalidate(PAddr addr, ushort size)
{
	assert(false);
}

void SMPNOC::returnAccess(MemRequest *mreq)
{

    SMPMemRequest *sreq = static_cast<SMPMemRequest *>(mreq);

    // Measure
    if( (sreq->hops>=0) ) {
        //IJ(sreq->routerTime!=0);
        Time_t msgLat = sreq->plat;
        //Time_t msgLat = globalClock - sreq->routerTime;
        int msgSize = sreq->getSize();

        if (msgSize>16) {
            DATAmsgLatCntHist.sample(sreq->hops);
            DATAmsgLatS1Hist.sample(sreq->hops, msgLat);
            DATAmsgLatS2Hist.sample(sreq->hops, msgLat*msgLat);
        } else {
            CTRLmsgLatCntHist.sample(sreq->hops);
            CTRLmsgLatS1Hist.sample(sreq->hops, msgLat);
            CTRLmsgLatS2Hist.sample(sreq->hops, msgLat*msgLat);
        }

        DEBUGPRINT(" \t\t\tNETdistance %d latency %lld size %d at %lld\n", sreq->hops, msgLat, msgSize, globalClock);
    }

    int nDst = sreq->numDstNode();
    if(nDst>1) {
        fprintf(stderr, "No support for multicast. NoC can have only one destination.\n");
        exit(1);
    }

    int32_t from = sreq->getSrcNode();
	
	int32_t to = -1;
	if(nDst==0) {
		to = -1;
	} else {
		to = sreq->getFirstDstNode();
	}
	
	MeshOperation meshOp = sreq->getMeshOperation();
	PAddr addr = sreq->getPAddr();
    int32_t msgSize = sreq->getSize();  // Size in bytes

	if(to<0) {
		IJ(meshOp==MeshMemAccessReply);

		DEBUGPRINT("         MESH Reply from memory for %x (src %d) at %lld\n",
				sreq->getPAddr(), sreq->getSrcNode(), globalClock);

		mreq->goUp(1);

	} else {

		IJ(from>=0 && to>=0);


		DEBUGPRINT("\t\t\tNoC recieve from %d to %d msg %x (size %d) for %x at %lld  (%p)\n"
				, from, to, meshOp, msgSize, addr, globalClock, sreq);

		for(uint32_t i = 0; i<upperLevel.size(); i++) {
			if(upperLevel[i]->getNodeID()==to) {
				upperLevel[i]->returnAccess(mreq);
				break;
			}
		}
	}
}

