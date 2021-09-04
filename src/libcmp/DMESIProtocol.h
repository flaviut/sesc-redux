/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Karin Strauss
                  Paul Sack

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

#ifndef DMESIPROTOCOL_H
#define DMESIPROTOCOL_H

#include "SMPProtocol.h"
#include "SMPCache.h"

enum DMESIState_t {
    DMESI_INVALID          = 0x00000000, // data is invalid
    DMESI_TRANS            = 0x10000000, // all transient states start w/ 0x1
    DMESI_TRANS_RD         = 0x12000000, // just started rd
    DMESI_TRANS_RD_MEM     = 0x16000000, // had to go to memory for rd
    DMESI_TRANS_WR         = 0x13000000, // just started wr
    DMESI_TRANS_WR_MEM     = 0x17000000, // had to go to memory for wr
    DMESI_TRANS_RSV        = 0x14000000, // line has just been allocated (reserved)
    DMESI_TRANS_INV        = 0x18000000, // used while invalidating upper levels
    DMESI_TRANS_INV_D      = 0x18000001, // used while invalidating upper levels,
    // and data is dirty (needs writeback)

    DMESI_VALID_BIT        = 0x00100000, // data is valid
    DMESI_DIRTY_BIT        = 0x00000001, // data is dirty (different from memory)

    DMESI_MODIFIED         = 0x00300001, // data is dirty and writable
    DMESI_EXCLUSIVE        = 0x00300010, // data is present only in local cache
    //DMESI_OWNER	         = 0x00101000, // data is present only in local cache
    DMESI_SHARED           = 0x00100100,  // data is shared
};

class DMESIProtocol : public SMPProtocol {

protected:

public:

    DMESIProtocol(SMPCache *cache, const char *name);
    ~DMESIProtocol();

    void changeState(Line *l, unsigned newstate);
    void makeDirty(Line *l);
    void preInvalidate(Line *l);

    void read(MemRequest *mreq);
    void doRead(MemRequest *mreq);
    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doRead> doReadCB;

    void write(MemRequest *mreq);
    void doWrite(MemRequest *mreq);
    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doWrite> doWriteCB;

    void doWriteCheck(MemRequest *mreq);
    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doWriteCheck> doWriteCheckCB;

    //  void returnAccess(SMPMemRequest *sreq);
    //
    // JJO
    void freeResource(MemRequest *mreq);

    void sendReadMiss(MemRequest *mreq);
    void sendWriteMiss(MemRequest *mreq);
    void sendInvalidate(MemRequest *mreq);

    // JJO
    void doSendUpgradeMiss(MemRequest *mreq);

    void doSendReadMiss(MemRequest *mreq);
    void doSendWriteMiss(MemRequest *mreq);
    void doSendInvalidate(MemRequest *mreq);

    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doSendReadMiss> doSendReadMissCB;
    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doSendWriteMiss> doSendWriteMissCB;
    typedef CallbackMember1<DMESIProtocol, MemRequest *,
            &DMESIProtocol::doSendInvalidate> doSendInvalidateCB;

    void sendReadMissAck(SMPMemRequest *sreq);
    void sendWriteMissAck(SMPMemRequest *sreq);
    void sendInvalidateAck(SMPMemRequest *sreq);

    //void sendDisplaceNotify(PAddr addr, CallbackBase *cb);

    void readMissHandler(SMPMemRequest *sreq);
    void writeMissHandler(SMPMemRequest *sreq);
    void invalidateHandler(SMPMemRequest *sreq);
    void invalidateReplyHandler(SMPMemRequest *sreq);
    void finalizeInvReply(SMPMemRequest *sreq);

    typedef CallbackMember1<DMESIProtocol, SMPMemRequest *,
            &DMESIProtocol::sendWriteMissAck> sendWriteMissAckCB;
    typedef CallbackMember1<DMESIProtocol, SMPMemRequest *,
            &DMESIProtocol::sendInvalidateAck> sendInvalidateAckCB;
    typedef CallbackMember1<DMESIProtocol, SMPMemRequest *,
            &DMESIProtocol::writeMissHandler> writeMissHandlerCB;
    typedef CallbackMember1<DMESIProtocol, SMPMemRequest *,
            &DMESIProtocol::invalidateHandler> invalidateHandlerCB;

    void readMissAckHandler(SMPMemRequest *sreq);
    // JJO
    //void dirReplyHandler(SMPMemRequest *sreq);
    void writeMissAckHandler(SMPMemRequest *sreq);
    //void invalidateAckHandler(SMPMemRequest *sreq);

    // data related
    void sendData(SMPMemRequest *sreq);
    void dataHandler(SMPMemRequest *sreq);

    void combineResponses(SMPMemRequest *sreq, DMESIState_t localState);

};

#endif //DMESIPROTOCOL_H
