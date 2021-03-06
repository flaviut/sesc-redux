#include "nanassert.h"
#include "SignalHandling.h"
#include "libll/ThreadContext.h"
#include "libcore/OSSim.h"

SignalAction getDflSigAction(SignalID sig) {
    switch (sig) {
        case SigNone:
            return SigActCore;
        case SigChld:
        case SigIO:
            return SigActIgnore;
        case SigAlrm:
            return SigActTerm;
        default:
            I(sig <= NumSignals);
    }
    return SigActTerm;
}

void SigInfo::save(ChkWriter &out) const {
    out << "Sig " << signo << " Code " << code << " pid " << pid << " uid " << uid << "data" << data << endl;
}

void SigInfo::restore(ChkReader &in) {
    size_t _signo, _code;
    in >> "Sig " >> _signo >> " Code " >> _code >> " pid " >> pid >> " uid " >> uid >> "data" >> data >> endl;
    signo = static_cast<SignalID>(_signo);
    code = static_cast<SigCode>(_code);
}

SignalTable::~SignalTable() {
}

void SignalTable::save(ChkWriter &out) const {
    for (const auto & s : table)
        out << "Handler " << hex << s.handler << dec << " Mask " << s.mask << endl;
}

void SignalTable::restore(ChkReader &in) {
    for (auto & s : table)
        in >> "Handler " >> hex >> s.handler >> dec >> " Mask " >> s.mask >> endl;
}
