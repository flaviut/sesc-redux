/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  James Tuck
                  Wei Liu
                  Milos Prvulovic
                  Luis Ceze
                  Smruti Sarangi
                  Paul Sack
                  Karin Strauss

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

#include <cctype>
#include <csignal>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>
#include <alloca.h>

#include "SescConf.h"
#include "libll/Instruction.h"
#include "GStats.h"
#include "GMemorySystem.h"
#include "GProcessor.h"
#include "FetchEngine.h"

#if (defined SESC_CMP)

#include "libcmp/SMPCache.h"
#include "libcmp/SMPNOC.h"

#endif
#if (defined DRAMSIM2)
#include "libDRAMSim2/DRAM.h"
#endif

#ifdef SESC_THERM
#include "ReportTherm.h"
#endif

#include "libll/ThreadContext.h"
#include "OSSim.h"

OSSim *osSim = nullptr;

/**********************
 * OSSim
 */

char *OSSim::benchName = nullptr;

static void sescConfSignal(int32_t sig) {
    static bool sigFaulting = false;

    if (sigFaulting)
        abort();

    sigFaulting = true;

    MSG("signal %d received. Dumping partial statistics\n", sig);

    osSim->reportOnTheFly();

    Report::close();

    abort();
}

void signalCatcher(int32_t sig) {
    osSim->reportOnTheFly();

    //  signal(SIGSEGV,sescConfSignal);

    signal(SIGUSR1, signalCatcher);
    signal(SIGQUIT, signalCatcher);
}

void OSSim::reportOnTheFly(const char *file) {
    char *tmp;

    if (!file)
        file = reportFile;

    tmp = (char *) malloc(strlen(file));
    strcpy(tmp, file);

    Report::openFile(tmp);

    SescConf->dump();

    report("Signal");

    Report::close();

    free(tmp);
}

OSSim::OSSim(int32_t argc, char **argv, char **envp)
        : traceFile(nullptr), snapshotGlobalClock(0), finishWorkNowCB(&cpus) {
    I(osSim == 0);
    osSim = this;

    //  signal(SIGSEGV,sescConfSignal);

    signal(SIGUSR1, signalCatcher);
    signal(SIGQUIT, signalCatcher);

    char *tmp = (char *) malloc(argc * 50 + 4096);
    tmp[0] = 0;
    for (int32_t i = 0; i < argc; i++) {
        strcat(tmp, argv[i]);
        strcat(tmp, " ");
    }

    benchRunning = tmp;

    benchSection = nullptr;

#ifndef SESC_PASS_ENVIRONMENT
    // The problem of passign the environment is that the stack size is
    // different depending of the machine where the simulator is
    // running. This produces different results. Incredible but true
    // (ask Basilio if you don't believe it)

    // JJ
    // hack for parsec
    char *nenv[3];
    int envpassed = 0;
    char *threads = getenv("OMP_NUM_THREADS");
    if (threads) {
        int bufsize = strlen("OMP_NUM_THREADS") + 1 + strlen(threads);
        char *buf = new char[bufsize];
        sprintf(buf, "OMP_NUM_THREADS=%s", threads);
        nenv[0] = buf;
        envpassed++;
        //printf("Setting omp: %s threads\n", threads);
    }
    //fix for vips
    threads = getenv("IM_CONCURRENCY");
    if (threads) {
        int bufsize = strlen("IM_CONCURRENCY") + 1 + strlen(threads);
        char *buf = new char[bufsize];
        sprintf(buf, "IM_CONCURRENCY=%s", threads);
        nenv[0] = buf;
        envpassed++;
    }
    nenv[envpassed] = nullptr;

//  char *nenv[2];
//  nenv[0] = 0;
    processParams(argc, argv, nenv);
#else
    processParams(argc,argv,envp);
#endif

#ifdef SESC_ENERGY
    // initialize the energy subsystem
    EnergyMgr::init();
#endif
}

void OSSim::processParams(int32_t argc, char **argv, char **envp) {
    const char *x6 = "XXXXXX";
    bool trace_flag = false;

    // Change, add parameters to mint
    char **nargv = (char **) malloc((20 + argc) * sizeof(char *));
    int32_t nargc;

    nargv[0] = strdup(argv[0]);

    int32_t i = 1;
    int32_t ni = 1;

    nInst2Skip = 0;
    nInst2Sim = 0;

    bool useMTMarks = false;
    int32_t mtId = 0;

    simMarks.total = 0;
    simMarks.begin = 0;
    simMarks.end = (~0UL) - 1;
    simMarks.mtMarks = false;

    const char *xtraPat = nullptr;
    const char *reportTo = nullptr;
    const char *confName = nullptr;
    const char *extension = nullptr;
    justTest = false;
    fastForward = false;

    if (argc < 2) {
        fprintf(stderr, "%s usage:\n", argv[0]);
        fprintf(stderr, "\t-cTEXT      ; Configuration file. Overrides sesc.conf and SESCCONF\n");
        fprintf(stderr, "\t-xTEXT      ; Extra key added in the report file name\n");
        fprintf(stderr, "\t-dTEXT      ; Change the name of the report file\n");
        fprintf(stderr, "\t-fTEXT      ; Fix the extension of the report file\n");
        fprintf(stderr, "\t-t          ; Do not execute, just test the configuration file\n");
        fprintf(stderr, "\t-yINT       ; Number of instructions to simulate\n");
        fprintf(stderr, "\t-bTEXT      ; Benchmark specific configuration section\n");

        fprintf(stderr, "\t-wINT       ; Number of instructions to skip in Rabbit Mode (-w1 means forever)\n");
        fprintf(stderr, "\t-1INT -2INT ; Simulate between marks -1 and -2 (start in rabbitmode)\n");
        fprintf(stderr, "\t-sINT       ; Total amount of shared memory reserved\n");
        fprintf(stderr, "\t-hINT       ; Total amount of heap memory reserved\n");
        fprintf(stderr, "\t-kINT       ; Stack size per thread\n");
        fprintf(stderr, "\t-T          ; Generate trace-file\n");
        fprintf(stderr, "\n\nExamples:\n");
        fprintf(stderr, "%s -k65536 -dreportName ./simulation \n", argv[0]);
        fprintf(stderr, "%s -h0x8000000 -xtest ../tests/crafty <../tests/tt.in\n", argv[0]);
        exit(0);
    }

    for (; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'w') {
                if (isdigit(argv[i][2]))
                    nInst2Skip = strtoll(&argv[i][2], nullptr, 0);
                else {
                    i++;
                    nInst2Skip = strtoll(argv[i], nullptr, 0);
                }
            } else if (argv[i][1] == 'y') {
                if (isdigit(argv[i][2]))
                    nInst2Sim = strtoll(&argv[i][2], nullptr, 0);
                else {
                    i++;
                    nInst2Sim = strtoll(argv[i], nullptr, 0);
                }
            } else if (argv[i][1] == 'm') {
                useMTMarks = true;
                simMarks.mtMarks = true;
                if (argv[i][2] != 0)
                    mtId = strtol(&argv[i][2], nullptr, 0);
                else {
                    i++;
                    mtId = strtol(argv[i], nullptr, 0);
                }
                idSimMarks[mtId].total = 0;
                idSimMarks[mtId].begin = 0;
                idSimMarks[mtId].end = (~0UL) - 1;
                idSimMarks[mtId].mtMarks = false;
            } else if (argv[i][1] == '1') {
                if (argv[i][2] != 0) {
                    if (useMTMarks)
                        idSimMarks[mtId].begin = strtol(&argv[i][2], nullptr, 0);
                    else
                        simMarks.begin = strtol(&argv[i][2], nullptr, 0);
                } else {
                    i++;
                    if (useMTMarks)
                        idSimMarks[mtId].begin = strtol(argv[i], nullptr, 0);
                    else
                        simMarks.begin = strtol(argv[i], nullptr, 0);
                }
                if (!useMTMarks)
                    simMarks.total = 0;
            } else if (argv[i][1] == '2') {
                if (argv[i][2] != 0) {
                    if (useMTMarks)
                        idSimMarks[mtId].end = strtol(&argv[i][2], nullptr, 0);
                    else
                        simMarks.end = strtol(&argv[i][2], nullptr, 0);
                } else {
                    i++;
                    if (useMTMarks)
                        idSimMarks[mtId].end = strtol(argv[i], nullptr, 0);
                    else
                        simMarks.end = strtol(argv[i], nullptr, 0);
                }
                if (!useMTMarks)
                    simMarks.total = 0;
            } else if (argv[i][1] == 'b') {
                if (argv[i][2] != 0)
                    benchSection = &argv[i][2];
                else {
                    i++;
                    benchSection = argv[i];
                }
            } else if (argv[i][1] == 'c') {
                if (argv[i][2] != 0)
                    confName = &argv[i][2];
                else {
                    i++;
                    confName = argv[i];
                }
            } else if (argv[i][1] == 'x') {
                if (argv[i][2] != 0)
                    xtraPat = &argv[i][2];
                else {
                    i++;
                    xtraPat = argv[i];
                }
            } else if (argv[i][1] == 'd') {
                I(reportTo == 0);
                if (argv[i][2] != 0)
                    reportTo = &argv[i][2];
                else {
                    i++;
                    reportTo = argv[i];
                }
            } else if (argv[i][1] == 'f') {
                I(extension == 0);
                if (argv[i][2] != 0)
                    extension = &argv[i][2];
                else {
                    i++;
                    extension = argv[i];
                }
            } else if (argv[i][1] == 'P') {
                justTest = true;
            } else if (argv[i][1] == 'F') {
                fastForward = true;
            } else if (argv[i][1] == 't') {
                justTest = true;
            } else if (argv[i][1] == 'T') {
                trace_flag = true;
            } else {
                nargv[ni] = strdup(argv[i]);
                ni++;
            }
            continue;
        }
        if (isdigit(argv[i][0])) {
            nargv[ni] = strdup(argv[i]);
            continue;
        }

        break;
    }

    char *name = (i == argc) ? argv[0] : argv[i];
    {
        char *p = strrchr(name, '/');
        if (p)
            name = p + 1;
    }
    benchName = strdup(name);

    I(nInst2Skip >= 0);

    nargv[ni++] = strdup("--");

    for (; i < argc; i++) {
        nargv[ni] = strdup(argv[i]);
        ni++;
    }
    nargc = ni;

    SescConf = new SConfig(confName);   // First thing to do

    Instruction::initialize(nargc, nargv, envp);

    if (reportTo) {
        reportFile = (char *) malloc(30 + strlen(reportTo));
        sprintf(reportFile, "%s.%s", reportTo, extension ? extension : x6);
    } else {
        if (getenv("REPORTFILE")) {
            reportFile = strdup(getenv("REPORTFILE"));
        } else {
            reportFile = (char *) malloc(30 + 2 * (strlen(benchName) + strlen(xtraPat ? xtraPat : benchName)));
            if (xtraPat)
                sprintf(reportFile, "sesc_%s_%s.%s", xtraPat, benchName, extension ? extension : x6);
            else
                sprintf(reportFile, "sesc_%s.%s", benchName, extension ? extension : x6);
        }
    }

    char *finalReportFile = (char *) strdup(reportFile);
    Report::openFile(finalReportFile);

#ifdef SESC_THERM
    {
        thermFile = (char*)malloc(strlen(finalReportFile) + 7);
        char *pp = strrchr(finalReportFile,'.');
        *pp = 0;
        sprintf(thermFile, "%s.therm.%s",finalReportFile, pp + 1);
        ReportTherm::openFile(thermFile);
        strcpy(pp, thermFile + ((pp - finalReportFile) +6));
    }
#endif

    if (trace_flag) {
        traceFile = (char *) malloc(strlen(finalReportFile) + 7);
        char *p = strrchr(finalReportFile, '.');
        *p = 0;
        sprintf(traceFile, "%s.trace.%s", finalReportFile, p + 1);
        Report::openFile(traceFile);
        strcpy(p, traceFile + ((p - finalReportFile) + 6));
    }

    free(finalReportFile);

#ifdef SESC_THERM
    free(thermFile);
#endif

    for (i = 0; i < nargc; i++)
        free(nargv[i]);

    free(nargv);

}

OSSim::~OSSim() {
    Instruction::finalize();

#ifdef SESC_THERM
    ReportTherm::stopCB();
#endif

    free(benchRunning);
    free(reportFile);

    delete SescConf;

    free(benchName);
    if (trace())
        free(traceFile);

    //printf("destructed..\n");
}

void OSSim::eventSysconf(Pid_t ppid, Pid_t fid, int32_t flags) {
    LOG("OSSim::sysconf(%d,%d,0x%x)", ppid, fid, flags);
    ProcessId *myProcessId = ProcessId::getProcessId(fid);
    // Can it be reconfigured on-the-fly?
    if (!myProcessId->sysconf(flags)) {
        // Tre process can not be reconfigured while running where it is running
        // Make it non-runnable
        cpus.makeNonRunnable(myProcessId);
        // Now reconfigure it
        bool secondTry = myProcessId->sysconf(flags);
        // Should always succeed
        I(secondTry);
        // Make it runnable again
        cpus.makeRunnable(myProcessId);
    }
}

int32_t OSSim::eventGetconf(Pid_t curPid, Pid_t targPid) {
    ProcessId *myProcessId = ProcessId::getProcessId(targPid);
    return myProcessId->getconf();
}

void OSSim::eventSpawn(Pid_t ppid, Pid_t fid, int32_t flags, bool stopped) {
    if (NoMigration)
        flags |= SESC_FLAG_NOMIGRATE;

    LOG("OSSim::spawn(%d,%d,0x%x,%d)", ppid, fid, flags, stopped);
    ProcessId *procId = ProcessId::create(ppid, fid, flags);
    if (!stopped)
        cpus.makeRunnable(procId);
}

void OSSim::stop(Pid_t pid) {
    // Get the procss descriptor
    ProcessId *proc = ProcessId::getProcessId(pid);
    // The descriptor should exist, and the process should be runnable
    I(proc && ((proc->getState() == ReadyState) || (proc->getState() == RunningState)));
    // Make the process non-runnable
    cpus.makeNonRunnable(proc);
}

void OSSim::unstop(Pid_t pid) {
    // Get the procss descriptor
    ProcessId *proc = ProcessId::getProcessId(pid);
    // The descriptor should exist, and the process should be in the InvalidState
    I(proc && (proc->getState() == InvalidState));
    // Make the process runnable
    cpus.makeRunnable(proc);
}

void OSSim::setPriority(Pid_t pid, int32_t newPrio) {
    ProcessId *proc = ProcessId::getProcessId(pid);
    I(proc);

    int32_t oldPrio = proc->getPriority();

    if (newPrio == oldPrio)
        return;

    // Set the new priority of the process
    ProcessId *otherProc = proc->setPriority(newPrio);
    if (newPrio > oldPrio) {
        // Priority is better now, check if still running
        if (proc->getState() == RunningState) {
            // Is there a process we need to swap with
            if (otherProc) {
                // Get the cpu where the demoted process is running
                CPU_t cpu = proc->getCPU();
                // Switch the demoted process out
                cpus.switchOut(cpu, proc);
                // Switch the new process in
                cpus.switchIn(cpu, otherProc);
            }
        }
    } else {
        // Priority is worse now, check if ready but not already running
        if (proc->getState() == ReadyState) {
            // Is there a process we need to swap with
            if (otherProc) {
                // Get the cpu where the other process is running
                CPU_t cpu = otherProc->getCPU();
                // Switch the victim process out
                cpus.switchOut(cpu, otherProc);
                // Switch the promoted process in
                cpus.switchIn(cpu, proc);
            }
        }
    }
}

int32_t OSSim::getPriority(Pid_t pid) {
    // Get the process descriptor
    ProcessId *proc = ProcessId::getProcessId(pid);
    // It should exist
    I(proc);
    // Return the priority of the process
    return proc->getPriority();

}

void OSSim::tryWakeupParent(Pid_t cpid) {
    ProcessId *proc = ProcessId::getProcessId(cpid);
    I(proc);
    Pid_t ppid = proc->getPPid();
    if (ppid < 0)
        return;

    ProcessId *pproc = ProcessId::getProcessId(ppid);
    // Does the parent process still exist?
    if (pproc == nullptr)
        return;

    if (pproc->getState() == WaitingState) {
        LOG("Waiting pid(%d) is awaked (child %d call)", ppid, cpid);
        pproc->setState(InvalidState);
        cpus.makeRunnable(pproc);
    }
}

void OSSim::eventExit(Pid_t cpid, int32_t err) {
    LOG("OSSim::exit err[%d] (cpid %d)", err, cpid);
    ProcessId *proc = ProcessId::getProcessId(cpid);
    I(proc);
    // If not in InvalidState, removefrom the running queue
    if (proc->getState() == RunningState || proc->getState() == ReadyState)
        cpus.makeNonRunnable(proc);
    // Try to wakeup parent
    tryWakeupParent(cpid);
    // Destroy the process
    proc->destroy();

#ifdef SESC_THERM
    ReportTherm::stopCB();
#endif
}

void OSSim::eventWait(Pid_t cpid) {
    // All the threads have already finished
    if (cpid < 0)
        return;
    LOG("OSSim::wait (cpid %d)", cpid);
    ProcessId *proc = ProcessId::getProcessId(cpid);
    if (proc->getNChilds() == 0) {
        // No child pending
        return;
    }
    // Should be still running
    I(proc->getState() == RunningState);
    // Make it non-runnable
    cpus.makeNonRunnable(proc);
    // Set state to WaitingState
    proc->setState(WaitingState);
}

ThreadContext *OSSim::getContext(Pid_t pid) {
    return ThreadContext::getContext(pid);
}

Pid_t OSSim::eventGetPPid(Pid_t pid) {
    ProcessId *proc = ProcessId::getProcessId(pid);
    I(proc);
    return proc->getPPid();
}

void OSSim::eventSetPPid(Pid_t pid, Pid_t ppid) {
    ProcessId *proc = ProcessId::getProcessId(pid);
    I(proc);
    proc->setPPid(ppid);
}

int32_t OSSim::eventSuspend(Pid_t cpid, Pid_t pid) {
    LOG("OSSim::suspend(%d) Received from pid %d", pid, cpid);
    ProcessId *proc = ProcessId::getProcessId(pid);
    if (proc == nullptr) {
        LOG("OSSim::suspend(%d) non existing process???", pid);
        return 0;
    }
    // Increment the suspend counter
    proc->incSuspendedCounter();
    // Check if process already suspended
    if (proc->getState() == SuspendedState) {
        I(0);
        LOG("OSSim::suspend(%d) already suspended (recursive=%d)", pid, proc->getSuspendedCounter());
        return 0;
    }
    // The process should be ready or running
    I((proc->getState() == ReadyState) || (proc->getState() == RunningState));
    // Need to suspend only if suspended counter is positive
    if (proc->getSuspendedCounter() > 0) {
        // The process is no longer runnable
        cpus.makeNonRunnable(proc);
        // Set the state to SuspendedState
        proc->setState(SuspendedState);
    } else {
        I(0);
        LOG("OSSim::suspend(%d,%d) OOO suspend/resume (%d)", cpid, pid, proc->getSuspendedCounter());
    }
    return 1;
}

int32_t OSSim::eventResume(Pid_t cpid, Pid_t pid) {
    LOG("OSSim::resume(%d,%d)", cpid, pid);
    ProcessId *proc = ProcessId::getProcessId(pid);
    if (proc == nullptr) {
        LOG("OSSim::resume(%d,%d) non existing process???", cpid, pid);
        return 0;
    }
    // Decrement the suspend counter
    proc->decSuspendedCounter();
    // If process is in SuspendedState
    if (proc->getState() == SuspendedState) {
        // If the suspend count is not positive
        if (proc->getSuspendedCounter() <= 0) {
            // Make the process runnable
            proc->setState(InvalidState);
            cpus.makeRunnable(proc);
        }
        return 1;
    } else {
        I(0);
        LOG("OSSim::resume(%d,%d) OOO suspend/resume (%d)", cpid, pid, proc->getSuspendedCounter());
    }
    return 0;
}

int32_t OSSim::eventYield(Pid_t curPid, Pid_t yieldPid) {
    //  LOG("OSSim::yield(%d,%d)", curPid, yieldPid);
    ProcessId *curProc = ProcessId::getProcessId(curPid);
    // The current process should be running
    I(curProc->getState() == RunningState);
    // get the CPU where the current process is running
    CPU_t cpu = curProc->getCPU();
    // Get the ProcessId of the new process
    ProcessId *yieldProc;
    if (yieldPid < 0) {
        // No specific new process, get next ready process
        yieldProc = ProcessId::queueGet(cpu);
    } else {
        // Specific ready process, get its ProcessId
        yieldProc = ProcessId::getProcessId(yieldPid);
        // there should be such a process
        if (!yieldProc) {
            LOG("OSSim::yield(%d) to non existing process???", yieldPid);
            return 0;
        }
    }
    // Do nothing if no process to yield to
    if (!yieldProc)
        return 1;
    // Do nothing if the new process already running
    if (yieldProc->getState() == RunningState)
        return 1;
    // The new process should not be suspended
    if (yieldProc->getState() == SuspendedState) {
        LOG("OSSim::yield(%d) to a suspended process???", yieldPid);
        return 0;
    }
    // The new process should not be pinned to a other processor
    if (yieldProc->isPinned() && (yieldProc->getCPU() != cpu)) {
        LOG("OSSim::yield(%d) to a NOMIGRABLE process in another CPU", yieldPid);
        return 0;
    }
    // Remove current process from the processor
    cpus.switchOut(cpu, curProc);
    // Put the new process on that processor
    cpus.switchIn(cpu, yieldProc);
    return 1;
}

void OSSim::initBoot() {
    static bool alreadyBoot = false;
    if (alreadyBoot)
        return;
    alreadyBoot = true;

    ProcessId::boot();

    // FIXME2: Change this for a static method in MemoryOS so that MemoryOS::boot
    // is called instead (it would call all the reserved memorysystem). Once this
    // is done, remove GMemorySystem from GProcessor.
    for (size_t i = 0; i < cpus.size(); i++) {
        I(cpus.getProcessor(i));
        I(cpus.getProcessor(i)->getMemorySystem());
        I(cpus.getProcessor(i)->getMemorySystem()->getMemoryOS());

        cpus.getProcessor(i)->getMemorySystem()->getMemoryOS()->boot();
    }

    // read it so it gets dumped
    const char *technology = SescConf->getCharPtr("", "technology");
    frequency = SescConf->getDouble(technology, "frequency");

    if (SescConf->checkBool("", "NoMigration"))
        NoMigration = SescConf->getBool("", "NoMigration");
    else
        NoMigration = false;

    // this is only necessary when running execution-driven

    // Launch the boot flow
    // -1 is the parent pid
    // 0 is the current thread, and it has no flags

    eventSpawn(-1, 0, 0);

}

void OSSim::preBoot() {
    static bool alreadyBoot = false;
    if (alreadyBoot)
        return;
    alreadyBoot = true;

    SescConf->dump();

    SescConf->lock();       // All the objects should be loaded

    time_t t = time(nullptr);
    Report::field("OSSim:beginTime=%s", ctime(&t));

    Report::field("OSSim:bench=%s", benchRunning);
    Report::field("OSSim:benchName=%s", benchName);
    if (nInst2Skip)
        Report::field("OSSim:rabbit=%lld", nInst2Skip);

    if (nInst2Sim)
        Report::field("OSSim:nInst2Sim=%lld", nInst2Sim);
    else { // 0 would never stop
        nInst2Sim = ((~0ULL) - 1024) / 2;
    }

    FetchEngine::setnInst2Sim(nInst2Sim);

    if (justTest) {
        MSG("Configuration tested");
        return;
    }

    gettimeofday(&stTime, nullptr);
    if (fastForward) {
        MSG("Begin fastforwarding: skipping instructions\n");
        MSG("End skipping: skipped %lld\n", (long long int) ThreadContext::skipInsts(-1));
    } else {
        MSG("Begin skipping: requested %lld instructions\n", nInst2Skip);
        MSG("End skipping: requested %lld skipped %lld\n", nInst2Skip,
            (long long int) ThreadContext::skipInsts(nInst2Skip));
    }
}

void OSSim::postBoot() {
    // Launch threads
    cpus.run();

    simFinish();
}

void OSSim::simFinish() {
    // Work finished, dump statistics
    report("Final");

    time_t t = time(nullptr);
    Report::field("OSSim:endTime=%s", ctime(&t));

    Report::close();

#ifdef SESC_THERM
    ReportTherm::stopCB();
    ReportTherm::close();
#endif

#if (defined SESC_CMP)
    SMPCache::PrintStat();
    SMPNOC::PrintStat();
#endif
#if (defined DRAMSIM2)
    DRAM::PrintStat();
#endif

    // hein? what is this? merge problems?
    //  if(trace())
    //  Report::close();
}

void OSSim::report(const char *str) const {

    ProcessId::report(str);

    for (size_t i = 0; i < cpus.size(); i++) {
        GProcessor *gproc = cpus.getProcessor(i);
        if (gproc)
            gproc->report(str);
    }

    Report::field("OSSim:reportName=%s", str);

    timeval endTime;
    gettimeofday(&endTime, nullptr);

    double msecs = (endTime.tv_sec - stTime.tv_sec) * 1000
                   + (endTime.tv_usec - stTime.tv_usec) / 1000;

    Report::field("OSSim:msecs=%8.2f:nCPUs=%d:nCycles=%lld", (double) msecs / 1000, cpus.size(), globalClock);

    Report::field("OSSim:pseudoreset=%lld", snapshotGlobalClock);

#ifdef SESC_ENERGY
    const char *procName = SescConf->getCharPtr("","cpucore",0);
    double totPower      = 0.0;
    double totClockPower = 0.0;

    for(size_t i=0; i<cpus.size(); i++) {
        double pPower = EnergyMgr::etop(GStatsEnergy::getTotalProc(i));

        double maxClockEnergy = EnergyMgr::get(procName,"clockEnergy",i);
        double maxEnergy      = EnergyMgr::get(procName,"totEnergy");

#if 0
        // Clock Power as computed by wattch
        double clockPower = (maxClockEnergy/maxEnergy) * pPower;
        double corePower  = pPower + clockPower;
#else
        // More reasonable clock energy. 50% is based on activity, 50% of the clock
        // distributtion is there all the time
        double clockPower = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
        double corePower  = pPower + clockPower;
#endif

        totPower      += corePower;
        totClockPower += clockPower;

        // print the rest of the report fields
        GStatsEnergy::dump(i);
        Report::field("Proc(%d):clockPower=%g",i,clockPower);
        Report::field("Proc(%d):totPower=%g",i,corePower);
    }
    GStatsEnergy::dump();
    Report::field("PowerMgr:clockPower=%g",totClockPower);
    Report::field("PowerMgr:totPower=%g",totPower);
    Report::field("EnergyMgr:clockEnergy=%g",EnergyMgr::ptoe(totClockPower));
    Report::field("EnergyMgr:totEnergy=%g",EnergyMgr::ptoe(totPower));
#endif

    // GStats must be the last to be called because previous ::report
    // can update statistics
    GStats::report(str);

}

GProcessor *OSSim::pid2GProcessor(Pid_t pid) const {
    I(ProcessId::getProcessId(pid));
    int32_t cpu = ProcessId::getProcessId(pid)->getCPU();
    // -1 when it has never started to execute
    I(cpu >= 0);

    return cpus.getProcessor(cpu);
}

ProcessIdState OSSim::getState(Pid_t pid) {
    I(ProcessId::getProcessId(pid));
    return ProcessId::getProcessId(pid)->getState();
}

GProcessor *OSSim::id2GProcessor(CPU_t cpu) const {
    I(cpus.getProcessor(cpu));
    I(cpus.getProcessor(cpu)->getId() == cpu);

    return cpus.getProcessor(cpu);
}

void OSSim::registerProc(GProcessor *core) {
    cpus.setProcessor(core->getId(), core);
}

void OSSim::unRegisterProc(GProcessor *core) {
    I(!core->hasWork());
    for (size_t i = 0; i < cpus.size(); i++) {
        if (cpus.getProcessor(i) == core) {
            cpus.setProcessor(i, nullptr);
            return;
        }
    }
}

Pid_t OSSim::contextSwitch(CPU_t cpu, Pid_t nPid) {
    // This is the process we displaced to run the target process
    Pid_t oldPid = -1;
    ProcessId *newProc = ProcessId::getProcessId(nPid);
    I(newProc);
    GProcessor *newCore = cpus.getProcessor(cpu);
    I(newCore);
    // Get the cpu where the target process is already running
    CPU_t runCpu = (newProc->getState() == RunningState) ? newProc->getCPU() : -1;
    // If already running on the target processor, do nothing
    if (runCpu == cpu)
        return -1;
    // If target core has no available flows, make one
    if (!newCore->availableFlows()) {
        oldPid = newCore->findVictimPid();
        ProcessId *oldProc = ProcessId::getProcessId(oldPid);
        cpus.switchOut(cpu, oldProc);
    }
    // Is the target process already running on another cpu?
    if (runCpu != -1) {
        // Get another process to run on original cpu
        ProcessId *repProc = ProcessId::queueGet(runCpu);
        // Get the target process out of old core
        cpus.switchOut(runCpu, newProc);
        // Get the replacement process (if any) into the old core
        if (repProc)
            cpus.switchIn(runCpu, repProc);
        // Get the target process in the target cpu
        cpus.switchIn(cpu, newProc);
    } else {
        // If new process not ready, make it ready
        if (newProc->getState() != ReadyState) {
            // Make it prefer the target cpu
            newProc->setCPU(cpu);
            // Make it ready
            newProc->setState(InvalidState);
            cpus.makeRunnable(newProc);
            // The target cpu is prefered by the target process, and
            // the target cpu has available flows. Thus, the target
            // process should be now running on the target cpu
            I(newProc->getCPU() == cpu);
            I(newProc->getState() == RunningState);
        } else {
            // The new process is already ready, just switch it in
            cpus.switchIn(cpu, newProc);
        }
    }
    return oldPid;
}

