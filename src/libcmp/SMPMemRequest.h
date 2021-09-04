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

#ifndef SMPMEMREQUEST_H
#define SMPMEMREQUEST_H

#include "pool.h"
#include "libcore/MemRequest.h"
#include "SMPDebug.h"
#include "SMPDirectory.h"

enum MeshOperation {
    ReadRequest 		= 0x00010008,

    ExclusiveReply		= 0x00030048,

    //ForwardRequest		= 0x00040008,
    //ForwardRequestNAK	= 0x00041008,
    IntervSharedRequest	= 0x00050008,

    SpeculativeReply	= 0x00060048,
    SharedReply			= 0x00070048,

    SharedResponse		= 0x00080048,
    SharingWriteBack	= 0x00090048,
    SharedAck			= 0x000A0008,
    SharingTransfer		= 0x000B0008,

    WriteRequest		= 0x00100008,
    Invalidation		= 0x00200008,
    InvalidationAck		= 0x00300008,
    //InvalidationAckData	= 0x00400048,
    ExclusiveReplyInv	= 0x00500048,

    IntervExRequest		= 0x00600008,
    ExclusiveResponse	= 0x00700048,
    DirtyTransfer		= 0x00800008,
    ExclusiveAck		= 0x00900008,

    UpgradeRequest		= 0x00A00008,
    ExclusiveReplyInvND	= 0x00B00008,

    //WriteBackRequest	= 0x01000008,
    WriteBackExAck		= 0x02000008,
    WriteBackBusyAck	= 0x03000008,
    //SharedResponseDummy	= 0x04000048,
    //ExclusiveResponseDummy = 0x05000048,
    WriteBackRequest	= 0x06000048,
    TokenBackRequest	= 0x07000008,

    NAK					= 0x10000008,

    MeshMemPush			= 0x21000048,
    MeshMemAccess		= 0x22000008,
    MeshMemAccessReply	= 0x23000048,
//	MeshMemWriteBack	= 0x24000048,

    NOP					= 0xF0000000

};

class SMPMemRequest : public MemRequest {
private:

    static pool<SMPMemRequest> rPool;
    friend class pool<SMPMemRequest>;

protected:

    MemRequest *oreq;
    uint32_t state;
    MemObj *requestor;
    MemObj *supplier;
    bool found;
    bool needSnoop;
    bool needData;
    bool writeDown;

    // recycling counter
    unsigned nUses;

    // JJO
    //MemReqSrc mreqSrc;


    // callback, if needed; should be only used when there is no oreq
    CallbackBase *cb;

public:
    SMPMemRequest();
    ~SMPMemRequest() {
    }

    // JJO
    std::set<int32_t> dst;
    std::set<MemObj*> dstObj;
    int32_t src;
    MeshOperation meshOp;
    //MeshOperation saveMeshOp;
    DirectoryEntry *dentry;
    //int32_t saveSrc;
    MemObj *msgDst;
    bool writeBack;
    bool isExcl;
    //bool isOwner;
    bool dataBack;
    CallbackBase *invCB;
    PAddr newAddr;

    int nInv;

    // For NOC...
    int32_t NoCTo;
    int32_t flitS;
    //int32_t srcMCTRL;
    Time_t NoCTime;
    Time_t NetTime[2];
    int hopCnt;

	// Recent 10/16
    int hops;
	int plat;
    Time_t routerTime;
	
    SMPMemRequest *saveReq;

	void clearValue();

    SMPMemRequest *duplicate();

    MemObj *msgOwner;
    MeshOperation getMeshOperation() {
        return meshOp;
    }
    MeshOperation mutateMemAccess() {
        IJ(meshOp == MeshMemAccess);
        meshOp = MeshMemAccessReply;
        return meshOp;
    }

    typedef std::map<MeshOperation, const char *> MESHSTRMAP;
    static MESHSTRMAP SMPMemReqStrMap;

    const char* getMeshOperationStr(MeshOperation op) {
        return SMPMemReqStrMap[op];
    }
    //MemReqSrc getMemReqSrc() { return mreqSrc; }
    void addDstNode(int32_t d) {
        dst.insert(d);
    }
    void addDst(MemObj *obj) {
        dst.insert(obj->getNodeID());
        dstObj.insert(obj);
        msgDst = obj;
    }
    void clearDstNodes() {
        dst.clear();
    }
    int numDstNode() {
        return dst.size();
    }
    bool isDstNode(int32_t d) {
        return (dst.find(d)!=dst.end());
    }
    void getDstNodes(std::set<int32_t> &l) {
        IJ(l.size()==0);
        if(dst.empty()) {
            l.clear();
        } else {
            l.insert(dst.begin(), dst.end());
        }
    }
    void getDstObjs(std::set<MemObj *> &l) {
        IJ(l.size()==0);
        if(dstObj.empty()) {
            l.clear();
        } else {
            l.insert(dstObj.begin(), dstObj.end());
        }
    }

    int32_t getFirstDstNode() {
        if(dst.size()>0) {
            return (*dst.begin());
        }
        IJ(0);
        return -1;
    }

    void setDirInfo(DirectoryEntry *de);
    //void setDstInfo(DirectoryEntry *de);
    void dumpDstNodes() {
        if(dst.empty()) {
            printf("MEM ");
        } else {
            for(std::set<int32_t>::iterator it = dst.begin(); it!=dst.end(); it++) {
                printf("%d, ", (*it));
            }
        }
    }
    // JJO
    typedef std::map<MeshOperation, uint64_t> MESHCNT;
    static MESHCNT nSMPMsg;
#if (defined SIGDEBUG)
    static MESHCNT nDEBUGSMPMsg;
#endif

    void setSrcNode(int32_t s) {
        src = s;
    }
    int32_t getSrcNode(void) {
        return src;
    }

    int32_t getSize()	{ // in Bytes
        return (meshOp&0xFF);
    }
	bool isDataPacket() {
		return (getSize()>16);
	}

	void dump() {
		printf("\t\t%s from %d to ", SMPMemRequest::SMPMemReqStrMap[meshOp], getSrcNode());
		dumpDstNodes();
	}


    // BEGIN: MemRequest interface
    static SMPMemRequest *create(SMPMemRequest *sreq,
                                 MeshOperation msh);

    static SMPMemRequest *create(SMPMemRequest *sreq,
                                 MemObj *reqCache,
                                 MeshOperation msh);

    static SMPMemRequest *create(MemRequest *mreq,
                                 MemObj *reqCache,
                                 bool sendData, MeshOperation msh);

    static SMPMemRequest *create(MemObj *reqCache,
                                 PAddr addr,
                                 MemOperation mOp,
                                 bool needsWriteDown,
                                 CallbackBase *cb, MeshOperation msh);


    void incUses();
    void destroy();
// void destroyAll();

    VAddr getVaddr() const;
    PAddr getPAddr() const;
    void  ack(TimeDelta_t lat);

    // END: MemRequest interface

    void setOriginalRequest(MemRequest *mreq) {
        oreq = mreq;
    } ;
    void setState(uint32_t st);
    void setRequestor(MemObj *reqCache);
    void setSupplier(MemObj *supCache);

    MemRequest  *getOriginalRequest();
    MemOperation getMemOperation();

    uint32_t getState();
    MemObj      *getRequestor();
    MemObj      *getSupplier();

    //bool         needsData()  { return needData; }
    bool         needsSnoop() {
        return needSnoop;
    }
    void         noSnoop()    {
        needSnoop = false;
    }


    bool         isFound()  {
        return found;
    }
    void         setFound() {
        found = true;
    }

    bool         needsWriteDown() {
        return writeDown;
    }
    void         setWriteDown()   {
        writeDown = true;
    }
};

class SMPMemReqHashFunc {
public:
    size_t operator()(const MemRequest *mreq) const {
        HASH<const char *> H;
        return H((const char *)mreq);
    }
};


#endif // SMPMEMREQUEST_H
