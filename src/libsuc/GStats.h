/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Basilio Fraguela
                  Smruti Sarangi
		  Luis Ceze
		  James Tuck

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

#ifndef GSTATSD_H
#define GSTATSD_H

#include <list>
#include <cstdarg>
#include <vector>
#include <set>

#include "estl.h" // hash_map
#include "callback.h"
#include "nanassert.h"

struct compare_unsigned_long_long {
    bool operator () (unsigned long long one, unsigned long long two) const {
        return one == two;
    }
};

struct hash_unsigned_long_long {
    size_t operator () (unsigned long long key) const {
        return key;
    }
};

class GStats {
private:
    typedef std::list < GStats * >Container;
    typedef std::list < GStats * >::iterator ContainerIter;
    static Container *store;
    // Points to this GStats object's position in the GStats store
    ContainerIter cpos;
protected:
    char *name;
    char *getText(const char *format,
                  va_list ap);
    void subscribe();
    void unsubscribe();

    virtual void prepareReport() {}

public:
    int32_t gd;

    static void report(const char *str);
    static GStats *getRef(const char *str);

    static void reset();

    GStats() {
    }
    virtual ~GStats();

    void prepareTrace();

    virtual void reportValue() const =0;
    virtual void resetValue() =0;

    virtual double getDouble() const {
        MSG("getDouble Not supported by this class %s",name);
        return 0;
    }

    const char *getName() const {
        return name;
    }
    virtual long long getSamples() const {
        return 1;
    }
};

class GStatsCntr : public GStats {
private:
    long long data;
protected:
public:
    GStatsCntr(const char *format,...);

    GStatsCntr & operator += (const int32_t v) {
        data += v;
        return *this;
    }

    void add(const int32_t v) {
        data += v;
    }
    // TODO? (Volunteer wanted!)
    // change that for operator++()
    void inc() {
        data++;
    }
    void cinc(bool cond) {
        data += cond ? 1 : 0;
    }

    void dec() {
        data--;
    }

    long long getValue() const {
        return data;
    }

    double getDouble() const override;
    void reportValue() const override;
    void resetValue() override {
        data = 0;
    }
};

class GStatsAvg : public GStats {
private:
protected:
    long long data;
    long long nData;
public:
    GStatsAvg(const char *format,...);
    GStatsAvg() { }

    virtual void sample(const int32_t v) {
        data += v;
        nData++;
    }
    // Merge two GStatsAvg together
    void sample(GStatsAvg &g) {
        data += g.data;
        nData += g.nData;
    }

    virtual void msamples(const long long v, long long n) {
        data  += v;
        nData += n;
    }

    double getDouble() const override;

    long long getSamples() const override {
        return nData;
    }

    void reportValue() const override;
    void resetValue() override {
        data = 0;
        nData = 0;
    }
};

class GStatsPDF : public GStatsAvg {
private:
protected:
    HASH_MAP<int32_t,int> density;
public:
    GStatsPDF(const char *format,...);
    GStatsPDF() { }

    void sample(const int32_t v) override;

    // Merge two GStatsPDF together
    void sample(GStatsPDF &g);

    void msamples(const long long v, long long n) override;

    double getStdDev() const;
    double getSpread(double p) const;

    void reportValue() const override;
    void resetValue() override {
        data = 0;
        nData = 0;
        density.clear();
    }
};

class GStatsProfiler : public GStats {
private:
protected:

    typedef HASH_MAP<uint32_t, int> ProfHash;

    ProfHash p;

public:
    GStatsProfiler(const char *format,...);

    void reportValue() const override;
    void resetValue() override {
        p.clear();
    }

    void sample(uint32_t key);
};

class GStatsMax : public GStats {
private:
protected:
    int32_t maxValue;
    long long nData;
public:
    GStatsMax(const char *format,...);

    void sample(const int32_t v) {
        maxValue = v > maxValue ? v : maxValue;
        nData++;
    }

    void reportValue() const override;
    void resetValue() override {
        maxValue = 0;
        nData = 0;
    }

};


class GStatsTimingAvg : public GStatsAvg {
private:
    Time_t lastUpdate;
    int32_t lastValue;
protected:
public:
    GStatsTimingAvg(const char *format,...);

    void sample(const int32_t v) override;
};

class GStatsHist : public GStats {
private:
protected:

    typedef HASH_MAP<uint32_t, unsigned long long> Histogram;

    unsigned long long numSample;
    unsigned long long cumulative;

    Histogram H;

public:
    GStatsHist(const char *format,...);
    GStatsHist() { }

    void reportValue() const override;
    void resetValue() override {
        H.clear();
        numSample = 0;
        cumulative = 0;
    }


    void sample(uint32_t key, unsigned long long weight=1);
};

class GStatsTimingHist : public GStatsHist {
private:
    Time_t lastUpdate;
    uint32_t lastKey;
    bool reportWholeHist;
public:
    GStatsTimingHist(const char *format,...);

    void reportValue() const override;
    void resetValue() override {
        H.clear();
        numSample = 0;
        cumulative = 0;

        lastUpdate = 0;
        lastKey = 0;
        reportWholeHist = true;
    }


    //Call on each update, it remembes what the last (key,time) pair was
    //and uses that to update the histogram.
    void sample(uint32_t key);

    // Call if you want only the timing average to be reported
    void disableLongOutput() {
        reportWholeHist = false;
    }
};

class GStatsChangeHist : public GStatsHist {
private:
    Time_t lastUpdate;
public:
    GStatsChangeHist(const char *format,...);
    void reportValue() const override;
    void resetValue() override {
        H.clear();
        numSample = 0;
        cumulative = 0;

        lastUpdate = 0;
    }

    //Call on each update, it remembes what the last (key,time) pair was
    //and uses that to update the histogram.
    void sample(uint32_t key=1);
};

class GStatsEventTimingHist : protected GStatsHist {
private:
    int32_t currentSum;

protected:

    class PendingEvent {
    public:
        PendingEvent(): start(0) {}
        unsigned long long start;
    };

    typedef std::set<unsigned long long> EventTimes;
    typedef HASH_MAP<unsigned long long, int32_t,
            hash_unsigned_long_long, compare_unsigned_long_long> EventHistory;
    typedef HASH_MAP<unsigned long long, PendingEvent,
            hash_unsigned_long_long, compare_unsigned_long_long> PendingEventsHash;

    EventTimes   evT;
    EventHistory evH;

    PendingEventsHash evPending;
    EventTimes   beginT;
    EventHistory beginH;

    Time_t lastSample;
    Time_t lastHistEvent;

    void buildHistogram(bool limit);
    void prepareReport() override {
        buildHistogram(false);
    }

public:
    GStatsEventTimingHist(const char *format,...);

    void reportValue() const override;
    void resetValue() override {
        H.clear();
        numSample = 0;
        cumulative = 0;

        currentSum = 0;
    }

    void begin_sample(unsigned long long id);
    void commit_sample(unsigned long long id);
    void remove_sample(unsigned long long id);
};

class GStatsPeriodicHist : public GStatsHist {
private:
    Time_t lastUpdate;
    Time_t period;

    long long data;
public:
    GStatsPeriodicHist(int32_t p, const char* format, ...);


    void reportValue() const override;
    void resetValue() override {
        H.clear();
        numSample = 0;
        cumulative = 0;
    }
    void inc();
};

#endif   // GSTATSD_H
