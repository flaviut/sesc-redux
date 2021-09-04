#include <set>
#include <vector>

#include <malloc.h>

enum DirStatus {
    EXCLUSIVE = 0,
    SHARED,
    UNOWNED
};

class DirectoryEntry {
public:
    DirectoryEntry() {
        status = UNOWNED;
#if 0
        int32_t nMax = pCache->getMaxNodeID();
        dinfo.resize(nMax);
        for(int i=0; i<nMax; i++) {
            dinfo[i] = 0;
        }
#endif
        busy = false;
        owner = NULL;
        TS = true;
    }
    ~DirectoryEntry() {
    }

    void setBusy() {
        busy = true;
    }
    void unsetBusy() {
        busy = false;
    }
    bool isBusy() {
        return busy;
    }


    DirStatus getStatus() {
        return status;
    }

    void setStatus(DirStatus s) {
        status = s;
    }

    uint32_t getNum() {
        return dinfo.size();
    }

    void addSharer(MemObj *obj) {
        //IJ(dinfo.find(obj)==dinfo.end());
        dinfo.insert(obj);
    }

    void removeSharer(MemObj *obj) {
        //IJ(dinfo.find(obj)!=dinfo.end());
        if(dinfo.find(obj)!=dinfo.end()) {
            dinfo.erase(obj);
        }
#if 0
        if(dinfo.empty()) {
            DEBUGPRINT("   +++ In directory, only %d shared, changed to unowned after removing\n", 1);
            setStatus(UNOWNED);
            owner = NULL;
        } else {
            if(owner==obj) {
                owner = *(dinfo.begin());
                DEBUGPRINT("   +++ In directory, owner changed to %s\n", owner->getSymbolicName());
            }
        }
#endif
    }

    void addOwner(MemObj *obj) {
        IJ(dinfo.find(obj)==dinfo.end());
        dinfo.insert(obj);
        owner = obj;
    }

    void setOwner(MemObj *obj) {
        IJ(dinfo.find(obj)!=dinfo.end());
        owner = obj;
    }

    MemObj* getOwner() {
        IJ(owner!=NULL);
        return owner;
    }

    void clearSharers() {
        dinfo.clear();
        owner = NULL;
    }

#if 0
    void setWrite(MemObj *obj) {
        status = MODIFIED;

        dinfo.clear();
        dinfo.insert(obj);
    }

    void doInval(MemObj *obj) {
        IJ(dinfo.find(obj)!=dinfo.end());
        dinfo.erase(obj);
        uint32_t n_d = dinfo.size();
        if(status==EXCLUSIVE||status==MODIFIED) {
            IJ(n_d==0);
            status = INVALID;
        } else {
            if(n_d==1) {
                status = EXCLUSIVE;
            } else if(n_d>1) {
                IJ(status==SHARED);
            } else {
                IJ(0);
                DEBUGPRINT(" what is this? status: %d, %d\n", status, (int)dinfo.size());
            }
        }
    }
#endif

    bool hasThis(MemObj *obj) {
        return (dinfo.find(obj)!=dinfo.end());
    }

    bool fillDst(std::set<int32_t> &d, std::set<MemObj*> &l, MemObj *ob) {
        IJ(d.size()==0);
        IJ(l.size()==0);
        bool found = false;
        for(std::set<MemObj*>::iterator it = dinfo.begin(); it!=dinfo.end(); it++) {
            if((*it)!=ob) {
                d.insert((*it)->getNodeID());
                l.insert((*it));
            } else {
                found = true;
            }
        }
        return found;
    }

    std::set<MemObj*> dinfo;
private:
    MemObj *owner;
    DirStatus status;
    //std::vector<uint8_t> dinfo;   //0x01 : inst cache, 0x10: data cache
    bool busy;
    bool TS;
};

class Directory {
public:
    Directory(const char *section) {

        const int STR_BUF_SIZE=1000;

        char bsize[STR_BUF_SIZE];
        char addrUnit[STR_BUF_SIZE];
        snprintf(bsize,STR_BUF_SIZE,"Bsize");
        snprintf(addrUnit,STR_BUF_SIZE,"AddrUnit");
        int32_t b = SescConf->getInt(section, bsize);
        int32_t u;
        if ( SescConf->checkInt(section,addrUnit) ) {
            if ( SescConf->isBetween(section, addrUnit, 0, b) &&
                    SescConf->isPower2(section, addrUnit) )
                u = SescConf->getInt(section,addrUnit);
            else
                u = 1;
        } else {
            u = 1;
        }
        log2AddrLs = log2i(b/u);
        //printf("%d %d %d\n", b, u , log2AddrLs);
#if (defined DEBUG_LEAK)
        lastClock = 1000000;
#endif
    }
    ~Directory() {
        for(std::map<PAddr, DirectoryEntry*>::iterator it = dirMap.begin(); it!=dirMap.end(); it++) {
            if((*it).second) {
                delete (*it).second;
            }
        }
    }

    DirectoryEntry* find(PAddr fullAddr) {
        PAddr addr = calcTag(fullAddr);
        //printf ("%x %x\n", fullAddr, addr);
        //
#if (defined DEBUG_LEAK)
        if(globalClock>lastClock) {
            struct mallinfo mem_info;
            mem_info = mallinfo();
            printf("\t(%12d) This is the total size of memory allocated with sbrk by malloc, in bytes.\n", mem_info.arena);
            printf("\t(%12d) This is the number of chunks not in use.\n", mem_info.ordblks);
            printf("\t(%12d) This field is unused.\n", mem_info.smblks);
            printf("\t(%12d) This is the total number of chunks allocated with mmap.\n", mem_info.hblks);
            printf("\t(%12d) This is the total size of memory allocated with mmap, in bytes.\n", mem_info.hblkhd);
            printf("\t(%12d) This field is unused.\n", mem_info.usmblks);
            printf("\t(%12d) This field is unused.\n", mem_info.fsmblks);
            printf("\t(%12d) This is the total size of memory occupied by chunks handed out by malloc.\n", mem_info.uordblks);
            printf("\t(%12d) This is the total size of memory occupied by free (not in use) chunks.\n", mem_info.fordblks);
            printf("\t(%12d) This is the size of the top-most releasable chunk that normally borders the end of the heap.\n", mem_info.keepcost);
            printf(" [%llu] %lu\n", globalClock, totCnt);
            printf("\n");
            fflush(stdout);
            lastClock = globalClock+10000000;
        }
#endif
        std::map<PAddr, DirectoryEntry*>::iterator it = dirMap.find(addr);
        if(it==dirMap.end()) {
            DirectoryEntry *de = new DirectoryEntry();
            dirMap[addr] = de;
#if (defined DEBUG_LEAK)
            totCnt++;
#endif
            return de;
        }
        return (*it).second;
    }
protected:
private:
#if (defined DEBUG_LEAK)
    static Time_t lastClock;
    static uint64_t totCnt;
#endif
    uint64_t log2AddrLs;
    PAddr calcTag(PAddr addr)       const {
        return (addr >> log2AddrLs);
    }

    std::map<PAddr, DirectoryEntry*> dirMap;
};

