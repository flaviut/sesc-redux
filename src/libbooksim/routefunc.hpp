// $Id: routefunc.hpp 5188 2012-08-30 00:31:31Z dub $

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

#ifndef _ROUTEFUNC_HPP_
#define _ROUTEFUNC_HPP_

#include "flit.hpp"
#include "router.hpp"
#include "outputset.hpp"
#include "config_utils.hpp"

typedef void (*tRoutingFunction)( const Router *, const Flit *, int in_channel, OutputSet *, bool );

void InitializeRoutingMap( const Configuration & config );

extern map<string, tRoutingFunction> gRoutingFunctionMap;

extern int gNumVCs;
extern int gReadReqBeginVC, gReadReqEndVC;
extern int gWriteReqBeginVC, gWriteReqEndVC;
extern int gReadReplyBeginVC, gReadReplyEndVC;
extern int gWriteReplyBeginVC, gWriteReplyEndVC;

void qtree_nca( const Router *r, const Flit *f,
                int in_channel, OutputSet* outputs, bool inject);
void tree4_anca( const Router *r, const Flit *f,
                 int in_channel, OutputSet* outputs, bool inject);
void tree4_nca( const Router *r, const Flit *f,
                int in_channel, OutputSet* outputs, bool inject);
void fattree_nca( const Router *r, const Flit *f,
                  int in_channel, OutputSet* outputs, bool inject);
void fattree_anca( const Router *r, const Flit *f,
                   int in_channel, OutputSet* outputs, bool inject);
void adaptive_xy_yx_mesh( const Router *r, const Flit *f,
                          int in_channel, OutputSet *outputs, bool inject );
void xy_yx_mesh( const Router *r, const Flit *f,
                 int in_channel, OutputSet *outputs, bool inject );
void dor_next_torus( int cur, int dest, int in_port,
                     int *out_port, int *partition,
                     bool balance);
void dim_order_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void dim_order_ni_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void dim_order_pni_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void romm_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void romm_ni_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void min_adapt_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void planar_adapt_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void limited_adapt_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void valiant_mesh( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void valiant_torus( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void valiant_ni_torus( const Router *r, const Flit *f, int in_channel,
                       OutputSet *outputs, bool inject );
void dim_order_torus( const Router *r, const Flit *f, int in_channel,
                      OutputSet *outputs, bool inject );
void dim_order_ni_torus( const Router *r, const Flit *f, int in_channel,
                         OutputSet *outputs, bool inject );
void dim_order_bal_torus( const Router *r, const Flit *f, int in_channel,
                          OutputSet *outputs, bool inject );
void min_adapt_torus( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject );
void dest_tag_fly( const Router *r, const Flit *f, int in_channel,
                   OutputSet *outputs, bool inject );
void chaos_torus( const Router *r, const Flit *f,
                  int in_channel, OutputSet *outputs, bool inject );
void chaos_mesh( const Router *r, const Flit *f,
                 int in_channel, OutputSet *outputs, bool inject );

int rand_min_intr_mesh( int src, int dest );

#endif
