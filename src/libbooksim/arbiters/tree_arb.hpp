// $Id: tree_arb.hpp 5188 2012-08-30 00:31:31Z dub $

/*
 Copyright (c) 2007-2012, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// ----------------------------------------------------------------------
//
//  TreeArbiter
//
// ----------------------------------------------------------------------

#ifndef _TREE_ARB_HPP_
#define _TREE_ARB_HPP_

#include "arbiter.hpp"

class TreeArbiter : public Arbiter {

    int _group_size;

    vector<Arbiter *> _group_arbiters;
    Arbiter *_global_arbiter;

    vector<int> _group_reqs;

public:

    // Constructors
    TreeArbiter(Module *parent, const string &name, int size, int groups, const string &arb_type);

    ~TreeArbiter() override;

    // Print priority matrix to standard output
    void PrintState() const override;

    // Update priority matrix based on last aribtration result
    void UpdateState() override;

    // Arbitrate amongst requests. Returns winning input and
    // updates pointers to metadata when valid pointers are passed
    int Arbitrate(int *id = nullptr, BPri_t *pri = nullptr) override;

    void AddRequest(int input, int id, BPri_t pri) override;

    void Clear() override;

};

#endif
