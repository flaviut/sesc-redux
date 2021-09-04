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

#ifndef SMPNOC_H
#define SMPNOC_H

#include "SMemorySystem.h"
#include "libcore/MemObj.h"
#include "Port.h"
#include "estl.h"
#include "pool.h"
//#include "SMPNoCRouter.h"
#include <assert.h>
#include <fstream>
#include <sys/time.h>
#include <vector>

// Booksim
#include <sstream>
#include "booksim.hpp"
#include "routefunc.hpp"
#include "traffic.hpp"
#include "booksim_config.hpp"
#include "trafficmanager.hpp"
#include "random_utils.hpp"
#include "network.hpp"
#include "injection.hpp"
#include "power_module.hpp"


class SMPNOC : public MemObj {
private:
    GStatsHist CTRLmsgLatCntHist; // Latency at Queue Histogram
    GStatsHist CTRLmsgLatS1Hist; // Latency at Queue Histogram
    GStatsHist CTRLmsgLatS2Hist; // Latency at Queue Histogram

    GStatsHist DATAmsgLatCntHist; // Latency at Queue Histogram
    GStatsHist DATAmsgLatS1Hist; // Latency at Queue Histogram
    GStatsHist DATAmsgLatS2Hist; // Latency at Queue Histogram

	class SMPPacket {
		public:
			SMPPacket() {};

			~SMPPacket() {};
			
			static SMPPacket* Get(MemRequest *mreq, int32_t from, int32_t to, int32_t msgSize, MeshOperation meshOp, PAddr addr, Time_t clock);
			void Set(MemRequest *mreq, int32_t from, int32_t to, int32_t msgSize, MeshOperation meshOp, PAddr addr, Time_t clock);

			void destroy();

			int GetFrom() { return _from; };
			int GetTo() { return _to; };

			MemRequest* GetMemRequest() { return _mreq; };

		private:
			static pool<SMPPacket> rPool;
			friend class pool<SMPPacket>;

			MemRequest *_mreq;
			int32_t _from;
			int32_t _to;
			int32_t _msgSize;
			MeshOperation _meshOp;
			PAddr _addr;
			Time_t _clock;
	};

	//std::map<int32_t, SMPPacket *> packetMap;

protected:
    PortGeneric *busPort;

#ifdef SESC_ENERGY
    GStatsEnergy *busEnergy;
#endif

	// Booksim
    struct timeval start_time, end_time; /* Time before/after user code */
    double total_time; /* Amount of time we've run */
	std::vector<Network *> net;
	int subnets;
	BookSimConfig bs_config;

    //typedef HASH_MAP<MemRequest *, int32_t, SMPMemReqHashFunc> PendReqsTable;
    //PendReqsTable pendReqsTable;

    // interface with upper level
    void read(MemRequest *mreq);
    void write(MemRequest *mreq);
    void push(MemRequest *mreq);
    void specialOp(MemRequest *mreq);

    Time_t nextSlot(MemRequest *mreq);

    virtual void doRead(MemRequest *mreq);
    virtual void doWrite(MemRequest *mreq);
    virtual void doPush(MemRequest *mreq);

    typedef CallbackMember1<SMPNOC, MemRequest *, &SMPNOC::doRead>
    doReadCB;
    typedef CallbackMember1<SMPNOC, MemRequest *, &SMPNOC::doWrite>
    doWriteCB;
    typedef CallbackMember1<SMPNOC, MemRequest *, &SMPNOC::doPush>
    doPushCB;

	//void doInject(MemRequest *mreq);
	//typedef CallbackMember1<SMPNOC, MemRequest *, &SMPNOC::doInject>
	//doInjectCB;
    
	//typedef CallbackMember0<SMPNOC, &SMPNOC::doAdvanceNOCCycle>
    //doAdvanceNOCCycleCB;

    virtual void finalizeRead(MemRequest *mreq);
    virtual void finalizeWrite(MemRequest *mreq);
    void finalizeAccess(MemRequest *mreq);
    virtual void goToMem(MemRequest *mreq);
    virtual unsigned getNumSnoopCaches(SMPMemRequest *sreq) {
        return upperLevel.size() - 1;
    }

	ofstream fs_booksim;
	static int bs_sample;

public:
    SMPNOC(SMemorySystem *gms, const char *section, const char *name);
    ~SMPNOC();
	
	static void doAdvanceNOCCycle();
	static std::list<std::pair<void *, std::pair<int, int> > > returnPackets;
	static SMPNOC *myself;
	static void PrintStat();
	void _PrintStat();

    // BEGIN MemObj interface

    // port usage accounting
    Time_t getNextFreeCycle() const;
    Time_t getNextFreeCycle(int32_t cycle);

    // interface with upper level
    bool canAcceptStore(PAddr addr) const;
    void access(MemRequest *mreq);

    // interface with lower level
    virtual void returnAccess(MemRequest *mreq);

    void invalidate(PAddr addr, ushort size, MemObj *oc);
    void doInvalidate(PAddr addr, ushort size);

    bool canAcceptStore(PAddr addr) {
        return true;
    }

    // END MemObj interface
    //

};

#endif // SMPSYSTEMBUS_H
