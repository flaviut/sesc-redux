/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Milos Prvulovic
                  Luiz Ceze

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

#ifndef PROCESSID_H
#define PROCESSID_H

#include <vector>
#include <list>

#include "nanassert.h"
#include "Snippets.h"
#include "callback.h"

#include "pool.h"

//#include "sescapi.h"
#include "ReportGen.h"

#define SESC_FLAG_NOMIGRATE  0x10000

/* The tid parameter (the lower 16 bits) indicate in which CPU this
 * thread is going to be mapped. If the flag SESC_SPAWN_NOMIGRATE is
 * also provided, the thread would always be executed in the same
 * CPU. (unless sesc_sysconf is modifies the flags status)
 */
#define SESC_FLAG_MAP        0x20000

#define SESC_FLAG_FMASK      0x8fff0000
#define SESC_FLAG_DMASK      0xffff

/* The absolute maximum number of processes that can be simulated. */
#define MAXPROC 1024

enum ProcessIdState {
    InvalidState = 0,
    RunningState,   // Running
    ReadyState,     // Runnable, but not running
    SuspendedState, // Removed from the runQ (suspended)
    WaitingState    // Removed from the runQ and waiting until a child finishes
};

// This class stores the information about a thread (pid). It includes
// the flags, the current running processor....
class ProcessId {
public:

    // Stats gathered for each thread
    static long long nSwitchs;
    static long long nSpawns;
    static long long nGradInsts;       // Graduated instructions
    static long long nWPathInsts;      // wrong path instructions

    class Stats {
    public:
        void reset() {
            nSwitchs = 0;
            nSpawns = 0;
            totalTime = 0;
            waitTime = 0;
            nGradInsts = 0;
            nWPathInsts = 0;
        }

        // Stats gathered for each thread
        long long nSwitchs;
        long long nSpawns;
        Time_t totalTime;
        Time_t waitTime;
        long long nGradInsts;       // Graduated instructions
        long long nWPathInsts;      // wrong path instructions
    };

private:
    typedef std::list<ProcessId *> ProcessQueue;
    static ProcessQueue processQueue;

    static void printQueue(char *where);

    static pool<ProcessId, true> pidPool;

    friend class pool<ProcessId, true>;

    static std::vector<ProcessId *> pidTable;

    ProcessIdState state;

    // The priority of the process (lower is better, 0 by default)
    int32_t priority;

    // Position of the process in the process queue, if it is in there
    ProcessQueue::iterator queuePosition;

    int32_t suspendedCounter;

    CPU_t cpu;
    int32_t myId;
    int32_t parentId;
    Pid_t ppid; // Parent pid
    Pid_t pid;
    uint32_t nChilds;

    Time_t spawnTime;
    Time_t startTime;

    Stats stats;

    // Set if process can move to another processor once it starts executing
    bool migrable;
    // Set if the process must map to the processor "cpu"
    bool pinned;

    // Inserts the process into the process queue and updates its position
    ProcessId *queueInsert() {
        // Should not already be in the queue
        I(queuePosition == processQueue.end());
        // Insert at end of queue
        queuePosition = processQueue.insert(processQueue.end(), this);
        // Update the position to reflect priority
        return queuePromote();
    }

    // Moves the process forward in the queue until it is at the end of its priority group
    // Returns the lowest priority process that has all the following properties:
    //   1) Currently running
    //   2) On a processor compatible with the promoted process
    //   3) Has priority lower than the promoted process
    // Effectivelly, if the promoted process is not already running and there is no
    // available compatible processor, the returned process is the one that should be
    // preempted in favor of running the promoted process
    ProcessId *queuePromote();

    // Moves the process backward in the queue until it is at the end of its priority group
    ProcessId *queueDemote();

    // Removes the process from the process queue
    void queueRemove() {
        // The process should still be in the queue
        I(queuePosition != processQueue.end());
        // Delete the process from the queue
        processQueue.erase(queuePosition);
        // Set current position in the queue to invalid
        queuePosition = processQueue.end();
    }

protected:
    ProcessId() {
    }

public:
    static void boot();

    // Returns the highest-priority process that can run on a given processor
    static ProcessId *queueGet(const CPU_t cpu);

    bool sysconf(int32_t flags);

    int32_t getconf() const;

    static ProcessId *create(Pid_t ppid, Pid_t id, int32_t flags);

    void destroy();

    static void destroyAll();

    void incSuspendedCounter() {
        suspendedCounter++;
    }

    void decSuspendedCounter();

    int32_t getSuspendedCounter() const {
        return suspendedCounter;
    }

    static ProcessId *getProcessId(Pid_t pid);

    static bool isSafeId(Pid_t pid);

    static uint32_t getNumRunningThreads() {
        return processQueue.size();
    }

    static uint32_t getNumThreads();

    static void report(const char *str);

    void reportId() const;

    bool isMigrable() const {
        return migrable;
    }

    bool isPinned() const {
        return pinned;
    }

    void pin(int p) {
        migrable = false;
        pinned = true;
        cpu = p;
    }

    CPU_t getCPU() const {
        return cpu;
    }

    void setCPU(CPU_t nCPU) {
        cpu = nCPU;
    }

    uint32_t getNChilds() const {
        return nChilds;
    }

    Pid_t getPid() const {
        return pid;
    }

    Pid_t getPPid() const {
        return ppid;
    }

    void setPPid(Pid_t newPPid) {
        // If new ppid the same as old one, do nothing
        if (ppid == newPPid)
            return;
        // If there is an old ppid
        if (ppid >= 0) {
            // Reduce the number of children it has
            ProcessId *oldParent = getProcessId(ppid);
            if (oldParent) {
                oldParent->nChilds--;
            }
        }
        // Now the parent is newPPid
        ppid = newPPid;
        // If there is such a process
        if (ppid >= 0) {
            // Increase the number of children it has
            ProcessId *newParent = getProcessId(ppid);
            if (newParent) {
                newParent->nChilds++;
            }
        }
    }

    // Makes the process ready to run
    ProcessId *becomeReady() {
        // The process should be in InvalidState
        I(state == InvalidState);
        // The new state of the process is ReadyState
        state = ReadyState;
        // Insert into the queue if not already in it
        return queueInsert();
    }

    void becomeNonReady() {
        // The process should be in ReadyState
        I(state == ReadyState);
        // The new state of the process is InvalidState
        state = InvalidState;
        // Remove from the process queue
        queueRemove();
    }

    ProcessIdState getState() const {
        return state;
    }

    void setState(ProcessIdState nstate) {
        // Can onlychange from InvalidState to one of the non-runnable states, or
        // from one of the non-runnable states to the InvalidState
        // Note:
        //       To change between InvalidState and ReadyState, use becomeReady and becomeNonReady
        //       To change between ReadyState and RunningState, use switchIn and switchOut
        I(((state == InvalidState) && (nstate != InvalidState) && (nstate != ReadyState) && (nstate != RunningState)) ||
          ((nstate == InvalidState) && (state != InvalidState) && (state != ReadyState) && (state != RunningState)));
        // Change to the new state
        state = nstate;
    }

    // Called when this process starts running on a processor
    void switchIn(CPU_t nCPU) {
        // The process should be runnable but not running
        I(state == ReadyState);
        GI(!migrable && pinned, cpu == nCPU);
        cpu = nCPU;
        // If can not migrate, pin it to this processor
        if (!migrable)
            pinned = true;

        // waittime includes the time between spawn and execution started
        stats.waitTime += globalClock - startTime;
        stats.nSwitchs++;
        startTime = globalClock;
        // The process is running now
        state = RunningState;
    }

    // Called when this thread stops running on a processor
    void switchOut(long long nGradInsts_, long long nWPathInsts_) {
        stats.totalTime += globalClock - startTime;
        stats.nGradInsts += nGradInsts_;
        stats.nWPathInsts += nWPathInsts_;

        startTime = globalClock;

        I(getState() == RunningState);

        state = ReadyState;
    }

    int32_t getPriority() const {
        return priority;
    }

    ProcessId *setPriority(int32_t newPriority) {
        // By default, return nothing
        ProcessId *retVal = nullptr;
        int32_t oldPriority = priority;
        priority = newPriority;
        // If in the process queue
        if (queuePosition != processQueue.end()) {
            // Update position in process queue
            if (newPriority < oldPriority) {
                // New priority is better than the old one
                retVal = queuePromote();
            } else {
                // New priority is worse than the old one
                retVal = queueDemote();
            }
        }
        return retVal;
    }

    long long getNGradInsts() const {
        return stats.nGradInsts;
    }
};


#endif //  PROCESSID_H
