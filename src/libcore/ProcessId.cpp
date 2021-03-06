/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Milos Prvulovic

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

#include "ProcessId.h"
#include "OSSim.h"
#include "GProcessor.h"

pool<ProcessId, true> ProcessId::pidPool;
std::vector<ProcessId *> ProcessId::pidTable;
ProcessId::ProcessQueue ProcessId::processQueue;

// Stats gathered for each thread
long long ProcessId::nSwitchs = 0;
long long ProcessId::nSpawns = 0;
long long ProcessId::nGradInsts = 0;       // Graduated instructions
long long ProcessId::nWPathInsts = 0;      // wrong path instructions

void ProcessId::boot() {
    for (size_t i = 0; i <= MAXPROC; i++) {
        pidTable.push_back(nullptr);
    }
}

// Returns the highest-priority process that can run on a given processor
ProcessId *ProcessId::queueGet(const CPU_t cpu) {
    for (auto proc : processQueue) {

        if ((proc->getState() == ReadyState) && ((!proc->pinned) || (proc->cpu == cpu))) {
            return proc;
        }

        // If the process not runnable, it should not be in the queue at all
        I((proc->getState() == ReadyState && proc->pinned && proc->cpu != cpu) || (proc->getState() == RunningState));
    }

    return nullptr; // no compatible process
}

ProcessId *ProcessId::queuePromote() {
    // Should be in the queue
    I(queuePosition != processQueue.end());

    ProcessId *retVal = nullptr;

    auto whereIt = queuePosition;

    while (whereIt != processQueue.begin()) {
        // Move toward the front of the queue
        whereIt--;
        // Get the ProcessId at the current position
        ProcessId *where = *whereIt;
        // Who has the priority
        if (where->getPriority() <= getPriority()) {
            // The other process should be ahead of this one
            whereIt++;
            break;
        } else {
            // The priority of the other process is lower If no process to return has
            // been found yet, check if this one satisfies the requirements
            if (!retVal)
                // Is it currently running on a compatible processor
                if ((where->state == RunningState) && ((!pinned) || (cpu == where->cpu)))
                    retVal = where;
        }
    }

    // If need to be moved in the queue, move it
    if (whereIt != queuePosition) {
        // Remove this process from its scurrent position
        processQueue.erase(queuePosition);
        // Insert at the new position
        queuePosition = processQueue.insert(whereIt, this);
    }
    // Should still be in the queue
    I(queuePosition != processQueue.end());
    return retVal;
}

// Moves the process backward in the queue until it is at the end of its priority group
ProcessId *ProcessId::queueDemote() {
    // Should be in the queue
    I(queuePosition != processQueue.end());
    // Initially, no process to return
    ProcessId *retVal = nullptr;
    // Start at current position in the queue
    auto whereIt = queuePosition;
    // Move one entry toward the end
    whereIt++;
    // Remove this process from its scurrent position
    processQueue.erase(queuePosition);
    // Now go backward, stop if at the end of the queue
    while (whereIt != processQueue.end()) {
        // Get the ProcessId at the current position
        ProcessId *where = *whereIt;
        // Who has the priority
        if (where->getPriority() > getPriority()) {
            // Demoted process has the priority, stop
            break;
        } else {
            // The priority of the other process is higher
            // If no process to return has been found yet,
            // check if this one satisfies the requirements
            if ((!retVal) && (where->state == ReadyState) &&
                ((!where->pinned) || (where->cpu == cpu)))
                retVal = where;
        }
        // Move toward the end of the queue
        whereIt++;
    }
    // Insert at the new position
    queuePosition = processQueue.insert(whereIt, this);
    // Should still be in the queue
    I(queuePosition != processQueue.end());
    return retVal;
}

bool ProcessId::sysconf(int32_t flags) {
    // Reject if running and it must be pinned to another processor
    if (((state == RunningState) || (state == ReadyState)) && (flags & SESC_FLAG_NOMIGRATE) &&
        (flags & SESC_FLAG_MAP) && ((flags & SESC_FLAG_DMASK) != cpu)) {
        return false;
    }
    migrable = !(flags & SESC_FLAG_NOMIGRATE);
    // By default it is not pinned
    pinned = false;
    // Is the process running
    if (state == RunningState) {
        // If it is not migrable, pin it to where it is running
        if (!migrable) {
            pinned = true;
            // Must not map to another processor
            I((!(flags & SESC_FLAG_MAP)) || ((flags & SESC_FLAG_DMASK) == cpu));
        }
        // If already running and migrable, leave it where it is
    } else {
        // The proces is not running, check if it has a mapping
        if (flags & SESC_FLAG_MAP) {
            cpu = flags & SESC_FLAG_DMASK;
            // If it can not migrate, pin it
            if (!migrable)
                pinned = true;

        } else {
            // No mapping, leave processor undecided
            cpu = -1;
        }
    }

    return true;
}

int32_t ProcessId::getconf() const {
    int32_t flags = 0;
    if (!migrable)
        flags |= SESC_FLAG_NOMIGRATE;
    if (cpu != -1) {
        flags |= SESC_FLAG_MAP;
        flags |= (SESC_FLAG_DMASK & cpu);
    }
    return flags;
}

ProcessId *ProcessId::create(Pid_t ppid, Pid_t id, int32_t flags) {
    ProcessId *proc = pidPool.out();
    int32_t parentId = -1;

    if ((ppid >= 0) && (pidTable[ppid])) {
        ProcessId *pproc = pidTable[ppid];
        pproc->nChilds++;
        pproc->stats.nSpawns++;
        parentId = pproc->myId;
    }

    static int32_t uniqueId = 0;
    proc->myId = uniqueId++;

    proc->priority = 0;
    proc->ppid = ppid;
    proc->parentId = parentId;
    proc->pid = id;
    proc->nChilds = 0;
    proc->state = InvalidState;
    proc->queuePosition = processQueue.end();
    proc->suspendedCounter = 0;

    proc->stats.reset();

    proc->spawnTime = globalClock;
    proc->startTime = globalClock;

    I(id < (Pid_t) pidTable.size());
    I(pidTable[id] == 0);
    pidTable[id] = proc;

    proc->sysconf(flags);

    if (!proc->isMigrable()) {
        size_t numCPU = osSim->getNumCPUs();
        proc->pin(id % numCPU);
    }

    return proc;
}

void ProcessId::destroyAll() {
    for (auto queueProc : processQueue) {
        if (queueProc->getState() == RunningState) {
            osSim->switchOut(queueProc->getCPU(), queueProc);
        }
        queueProc->destroy();
    }
}

void ProcessId::destroy() {
    // Should not be in the process queue
    I(queuePosition == processQueue.end());
    // Should not be runnable
    I(state != RunningState);
    I(state != ReadyState);
    I(pidTable[pid] == this);

    // Copy stats
    nSwitchs += stats.nSwitchs;
    nSpawns += stats.nSpawns;
    nGradInsts += stats.nGradInsts;
    nWPathInsts += stats.nWPathInsts;

    reportId();
    if (ppid >= 0)
        if (pidTable[ppid])
            pidTable[ppid]->nChilds--;

    pidTable[pid] = nullptr;

    pidPool.in(this);
}

bool ProcessId::isSafeId(Pid_t pid) {
    if (pid >= (Pid_t) pidTable.size())
        return false;

    if (pid < 0)
        return false;

    if (pidTable[pid]) {
        if (pidTable[pid]->pid != pid)
            return false;
    }
    return true;
}

ProcessId *ProcessId::getProcessId(Pid_t pid) {
    I(isSafeId(pid));
    return pidTable[pid];
}

uint32_t ProcessId::getNumThreads() {
    return ThreadContext::getPidUb();
}

void ProcessId::report(const char *str) {
    for (auto & i : pidTable) {
        if (i == nullptr)
            continue;
        i->reportId();
    }

    // Dump global stats

    Report::field("ProcessId:nSwitchs=%lld:nSpawns=%lld:nGradInsts=%lld:nWPathInsts=%lld", nSwitchs, nSpawns,
                  nGradInsts, nWPathInsts);

}

void ProcessId::reportId() const {
    Report::field("ProcessId(%d):totalTime=%lld:waitTime=%lld:spawnTime=%lld:exitTime=%lld:Switchs=%ld", myId,
                  (long long) stats.totalTime, (long long) stats.waitTime, (long long) spawnTime,
                  (long long) globalClock, stats.nSwitchs
    );
    Report::field("ProcessId(%d):cpu=%ld:migrable=%s:pinned=%s:pid=%d:ppid=%d:parentId=%d", myId, cpu,
                  migrable ? "true" : "false", pinned ? "true" : "false", pid, ppid, parentId
    );
}

void ProcessId::printQueue(char *where) {
    printf("ProcessQueue %s Begin", where);

    for (auto queueProc : processQueue) {
        I(queueProc->queuePosition == queueIt);

        printf(" %3d(%d)", queueProc->getPid(), queueProc->getPriority());

        if (queueProc->isMigrable()) {
            printf("@");
        } else {
            printf("P");
        }
        printf("%d", queueProc->getCPU());
        switch (queueProc->getState()) {
            case InvalidState:
                printf(" INV ");
                break;
            case RunningState:
                printf(" RUN ");
                break;
            case ReadyState:
                printf(" RDY ");
                break;
            case SuspendedState:
                printf(" SUSP");
                break;
            case WaitingState:
                printf(" WAIT");
                break;
        }
    }
    printf(" End\n");
}

void ProcessId::decSuspendedCounter() {
    I(suspendedCounter > 0);
    if (suspendedCounter <= 0) {
        int32_t j = rand();
    }
    suspendedCounter--;
}

