
/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela

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

#ifndef MEMOBJ_H
#define MEMOBJ_H

#include <vector>

#include "nanassert.h"
#include "callback.h"

#include "libll/ThreadContext.h"

#ifndef DEBUGCONDITION
#ifdef DEBUG
#define DEBUGCONDITION 0
#else
#define DEBUGCONDITION 0
#endif
#endif

#if (defined SESC_CMP)
#define IJ(aC)    do{                 if(!(aC)) doassert(); }while(0)
#endif

class MemRequest;      // Memory Request (from processor to cache)

class MemObj {
public:
    typedef std::vector<MemObj *> LevelType;
private:
    bool highest;

protected:

    uint32_t nUpperCaches;
    LevelType upperLevel;
    LevelType lowerLevel;

    const char *descrSection;
    const char *symbolicName;

#if (defined SESC_CMP)
    // JJO
    int32_t nodeID;
    // JJO
    MemObj *sideLowerLevel;
    MemObj *sideUpperLevel;

    bool dataCache;

    void addSideLowerLevel(MemObj *obj) {
        I(obj);
        sideLowerLevel = obj;
        obj->addSideUpperLevel(this);
    }

    void addSideUpperLevel(MemObj *obj) {
        sideUpperLevel = obj;
    }

#endif

    void addLowerLevel(MemObj *obj) {
        I(obj);
        lowerLevel.push_back(obj);
        obj->addUpperLevel(this);
    }

    void addUpperLevel(MemObj *obj) {
        upperLevel.push_back(obj);
    }

    void invUpperLevel(PAddr addr, ushort size, MemObj *oc) {

        I(oc);

        for (auto & i : upperLevel)
            i->invalidate(addr, size, oc);
    }

public:
    MemObj(const char *section, const char *sName);

    MemObj();

    virtual ~MemObj();

    bool isHighestLevel() const {
        return highest;
    }

    void setHighestLevel() {
        I(!highest);
        highest = true;
    }

    const char *getDescrSection() const {
        return descrSection;
    }

    const char *getSymbolicName() const {
        return symbolicName;
    }

#if (defined SESC_CMP)

    // JJO
    virtual int32_t getNodeID() {
        IJ(0);
        return 0;
    }

    virtual int32_t getMaxNodeID() {
        IJ(0);
        return 0;
    }

    virtual void goToMem(MemRequest *mreq) {
        IJ(0);
    }

#endif

    const LevelType *getLowerLevel() const {
        return &lowerLevel;
    }

    const LevelType *getUpperLevel() const {
        return &upperLevel;
    }

    uint32_t getUpperLevelSize() const {
        return upperLevel.size();
    }
    //const uint32_t getUpperLevelSize() const { return upperLevel.size(); }

    virtual bool isCache() const {
        return false;
    }
    //virtual const bool isCache() const { return false; }

    //const uint32_t getNumCachesInUpperLevels() const {
    uint32_t getNumCachesInUpperLevels() const {
        return nUpperCaches;
    }

    void computenUpperCaches();

    //This assumes single entry point for object, which I do not like,
    //but it is still something that is worthwhile.
    virtual Time_t getNextFreeCycle() const = 0;

    virtual void access(MemRequest *mreq) = 0;

    virtual void returnAccess(MemRequest *mreq) = 0;

    virtual void invalidate(PAddr addr, ushort size, MemObj *oc) = 0;

    virtual void doInvalidate(PAddr addr, ushort size) {
        I(0);
    }

    typedef CallbackMember2<MemObj, PAddr, ushort,
            &MemObj::doInvalidate> doInvalidateCB;

    // When the buffers in the cache are full and it does not accept more requests
    virtual bool canAcceptStore(PAddr addr) = 0;

    virtual bool canAcceptLoad(PAddr addr) {
        return true;
    }

    // Print stats
    virtual void dump() const;

#if (defined SESC_CMP)

    // JJO
    virtual uint32_t getPendingReqs() {
        IJ(0);
        return 0;
    }

    virtual uint32_t getPendingReqsProc(int32_t pID) {
        IJ(0);
        return 0;
    }

    virtual Time_t getPendingCycles() {
        IJ(0);
        return 0;
    }

    virtual int getOcc() {
        IJ(0);
        return 0;
    }

#endif

};

class DummyMemObj : public MemObj {
private:
protected:
    Time_t getNextFreeCycle() const override;

    void access(MemRequest *req) override;

    bool canAcceptStore(PAddr addr) override;

    void invalidate(PAddr addr, ushort size, MemObj *oc) override;

    void doInvalidate(PAddr addr, ushort size) override;

    void returnAccess(MemRequest *req) override;

public:
    DummyMemObj();

    DummyMemObj(const char *section, const char *sName);
};

#endif // MEMOBJ_H
