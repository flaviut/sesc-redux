// $Id: trafficmanager.cpp 5223 2012-09-05 19:45:06Z dub $

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

#include <sstream>
#include <cmath>
#include <fstream>
#include <limits>
#include <cstdlib>

#include "booksim.hpp"
#include "booksim_config.hpp"
#include "trafficmanager.hpp"
#include "random_utils.hpp"
#include "vc.hpp"
#include "packet_reply_info.hpp"

pool<TrafficManager::SESCPacket> TrafficManager::SESCPacket::rPool(5192, "SESCPacket");

TrafficManager::SESCPacket *TrafficManager::SESCPacket::Get(int from, int to, int cls, int msgSize, void *pkt) {
    SESCPacket *SESCpkt = rPool.out();
    SESCpkt->Set(from, to, cls, msgSize, pkt);
    return SESCpkt;
}

void TrafficManager::SESCPacket::Set(int from, int to, int cls, int msgSize, void *pkt) {
    _from = from;
    _to = to;
    _cls = cls;
    _msgSize = msgSize;
    _pkt = pkt;
}

void TrafficManager::SESCPacket::Free() {
    _from = -1;
    _to = -1;
    _cls = -1;
    _msgSize = -1;
    _pkt = nullptr;
    rPool.in(this);
}

TrafficManager *TrafficManager::New(Configuration const &config,
                                    vector<Network *> const &net) {
    TrafficManager *result = nullptr;
    string sim_type = config.GetStr("sim_type");
    if ((sim_type == "latency") || (sim_type == "throughput")) {
        result = new TrafficManager(config, net);
    } else {
        cerr << "Unknown simulation type: " << sim_type << endl;
    }
    return result;
}

TrafficManager::TrafficManager(const Configuration &config, const vector<Network *> &net)
        : Module(nullptr, "traffic_manager"), _net(net), _empty_network(false), _deadlock_timer(0), _reset_time(0),
          _drain_time(-1), _cur_id(0), _cur_pid(0), _time(0) {

    _nodes = _net[0]->NumNodes();
    _routers = _net[0]->NumRouters();

    _vcs = config.GetInt("num_vcs");
    _subnets = config.GetInt("subnets");

    _channel_width = (double) config.GetInt("channel_width");
    _channel_width_byte = _channel_width / 8.0;

    _subnet.resize(Flit::NUM_FLIT_TYPES);
    _subnet[Flit::READ_REQUEST] = config.GetInt("read_request_subnet");
    _subnet[Flit::READ_REPLY] = config.GetInt("read_reply_subnet");
    _subnet[Flit::WRITE_REQUEST] = config.GetInt("write_request_subnet");
    _subnet[Flit::WRITE_REPLY] = config.GetInt("write_reply_subnet");

    // ============ Message priorities ============

    string priority = config.GetStr("priority");

    if (priority == "class") {
        _pri_type = class_based;
    } else if (priority == "age") {
        _pri_type = age_based;
    } else if (priority == "network_age") {
        _pri_type = network_age_based;
    } else if (priority == "local_age") {
        _pri_type = local_age_based;
    } else if (priority == "queue_length") {
        _pri_type = queue_length_based;
    } else if (priority == "hop_count") {
        _pri_type = hop_count_based;
    } else if (priority == "sequence") {
        _pri_type = sequence_based;
    } else if (priority == "none") {
        _pri_type = none;
    } else {
        Error("Unkown priority value: " + priority);
    }

    // ============ Routing ============

    string rf = config.GetStr("routing_function") + "_" + config.GetStr("topology");
    map<string, tRoutingFunction>::const_iterator rf_iter = gRoutingFunctionMap.find(rf);
    if (rf_iter == gRoutingFunctionMap.end()) {
        Error("Invalid routing function: " + rf);
    }
    _rf = rf_iter->second;

    _lookahead_routing = !config.GetInt("routing_delay");
    _noq = config.GetInt("noq");
    if (_noq) {
        if (!_lookahead_routing) {
            Error("NOQ requires lookahead routing to be enabled.");
        }
    }

    // ============ Traffic ============

    _classes = config.GetInt("classes");
    assert(_classes % 2 == 0 || _classes == 1);

    _use_read_write = config.GetIntArray("use_read_write");
    if (_use_read_write.empty()) {
        _use_read_write.push_back(config.GetInt("use_read_write"));
    }
    _use_read_write.resize(_classes, _use_read_write.back());

    _write_fraction = config.GetFloatArray("write_fraction");
    if (_write_fraction.empty()) {
        _write_fraction.push_back(config.GetFloat("write_fraction"));
    }
    _write_fraction.resize(_classes, _write_fraction.back());

    _read_request_size = config.GetIntArray("read_request_size");
    if (_read_request_size.empty()) {
        _read_request_size.push_back(config.GetInt("read_request_size"));
    }
    _read_request_size.resize(_classes, _read_request_size.back());

    _read_reply_size = config.GetIntArray("read_reply_size");
    if (_read_reply_size.empty()) {
        _read_reply_size.push_back(config.GetInt("read_reply_size"));
    }
    _read_reply_size.resize(_classes, _read_reply_size.back());

    _write_request_size = config.GetIntArray("write_request_size");
    if (_write_request_size.empty()) {
        _write_request_size.push_back(config.GetInt("write_request_size"));
    }
    _write_request_size.resize(_classes, _write_request_size.back());

    _write_reply_size = config.GetIntArray("write_reply_size");
    if (_write_reply_size.empty()) {
        _write_reply_size.push_back(config.GetInt("write_reply_size"));
    }
    _write_reply_size.resize(_classes, _write_reply_size.back());

    string packet_size_str = config.GetStr("packet_size");
    if (packet_size_str.empty()) {
        _packet_size.push_back(vector<int>(1, config.GetInt("packet_size")));
    } else {
        vector<string> packet_size_strings = tokenize_str(packet_size_str);
        for (auto & packet_size_string : packet_size_strings) {
            _packet_size.push_back(tokenize_int(packet_size_string));
        }
    }
    _packet_size.resize(_classes, _packet_size.back());

    string packet_size_rate_str = config.GetStr("packet_size_rate");
    if (packet_size_rate_str.empty()) {
        int rate = config.GetInt("packet_size_rate");
        assert(rate >= 0);
        for (int c = 0; c < _classes; ++c) {
            int size = _packet_size[c].size();
            _packet_size_rate.push_back(vector<int>(size, rate));
            _packet_size_max_val.push_back(size * rate - 1);
        }
    } else {
        vector<string> packet_size_rate_strings = tokenize_str(packet_size_rate_str);
        packet_size_rate_strings.resize(_classes, packet_size_rate_strings.back());
        for (int c = 0; c < _classes; ++c) {
            vector<int> rates = tokenize_int(packet_size_rate_strings[c]);
            rates.resize(_packet_size[c].size(), rates.back());
            _packet_size_rate.push_back(rates);
            int size = rates.size();
            int max_val = -1;
            for (int i = 0; i < size; ++i) {
                int rate = rates[i];
                assert(rate >= 0);
                max_val += rate;
            }
            _packet_size_max_val.push_back(max_val);
        }
    }

    for (int c = 0; c < _classes; ++c) {
        if (_use_read_write[c]) {
            _packet_size[c] =
                    vector<int>(1, (_read_request_size[c] + _read_reply_size[c] +
                                    _write_request_size[c] + _write_reply_size[c]) / 2);
            _packet_size_rate[c] = vector<int>(1, 1);
            _packet_size_max_val[c] = 0;
        }
    }

    _load = config.GetFloatArray("injection_rate");
    if (_load.empty()) {
        _load.push_back(config.GetFloat("injection_rate"));
    }
    _load.resize(_classes, _load.back());

    if (config.GetInt("injection_rate_uses_flits")) {
        for (int c = 0; c < _classes; ++c)
            _load[c] /= _GetAveragePacketSize(c);
    }

    _traffic = config.GetStrArray("traffic");
    _traffic.resize(_classes, _traffic.back());

    _traffic_pattern.resize(_classes);

    _class_priority = config.GetIntArray("class_priority");
    if (_class_priority.empty()) {
        _class_priority.push_back(config.GetInt("class_priority"));
    }
    _class_priority.resize(_classes, _class_priority.back());

    vector<string> injection_process = config.GetStrArray("injection_process");
    injection_process.resize(_classes, injection_process.back());

    _injection_process.resize(_classes);

    for (int c = 0; c < _classes; ++c) {
        _traffic_pattern[c] = TrafficPattern::New(_traffic[c], _nodes, &config);
        _injection_process[c] = InjectionProcess::New(injection_process[c], _nodes, _load[c], &config);
    }

    // ============ Injection VC states  ============

    _buf_states.resize(_nodes);
    _last_vc.resize(_nodes);
    _last_class.resize(_nodes);

    for (int source = 0; source < _nodes; ++source) {
        _buf_states[source].resize(_subnets);
        _last_class[source].resize(_subnets, 0);
        _last_vc[source].resize(_subnets);
        for (int subnet = 0; subnet < _subnets; ++subnet) {
            ostringstream tmp_name;
            tmp_name << "terminal_buf_state_" << source << "_" << subnet;
            auto *bs = new BufferState(config, this, tmp_name.str());
            int vc_alloc_delay = config.GetInt("vc_alloc_delay");
            int sw_alloc_delay = config.GetInt("sw_alloc_delay");
            int router_latency = config.GetInt("routing_delay") +
                                 (config.GetInt("speculative") ? max(vc_alloc_delay, sw_alloc_delay) : (vc_alloc_delay +
                                                                                                        sw_alloc_delay));
            int min_latency = 1 + _net[subnet]->GetInject(source)->GetLatency() + router_latency +
                              _net[subnet]->GetInjectCred(source)->GetLatency();
            bs->SetMinLatency(min_latency);
            _buf_states[source][subnet] = bs;
            _last_vc[source][subnet].resize(_classes, -1);
        }
    }

#ifdef TRACK_FLOWS
    _outstanding_credits.resize(_classes);
    for(int c = 0; c < _classes; ++c)
    {
        _outstanding_credits[c].resize(_subnets, vector<int>(_nodes, 0));
    }
    _outstanding_classes.resize(_nodes);
    for(int n = 0; n < _nodes; ++n)
    {
        _outstanding_classes[n].resize(_subnets, vector<queue<int> >(_vcs));
    }
#endif

    // ============ Injection queues ============

    _qtime.resize(_nodes);
    _qdrained.resize(_nodes);
    _partial_packets.resize(_nodes);
    _packetBuffer.resize(_nodes);

    for (int s = 0; s < _nodes; ++s) {
        _qtime[s].resize(_classes);
        _qdrained[s].resize(_classes);
        _partial_packets[s].resize(_classes);
        _packetBuffer[s].resize(_classes);
    }

    _total_in_flight_flits.resize(_classes);
    _measured_in_flight_flits.resize(_classes);
    _retired_packets.resize(_classes);

    _packet_seq_no.resize(_nodes);
    //_repliesPending.resize(_nodes);
    _requestsOutstanding.resize(_nodes);

    _hold_switch_for_packet = config.GetInt("hold_switch_for_packet");

    // ============ Simulation parameters ============

    _total_sims = config.GetInt("sim_count");

    _router.resize(_subnets);
    for (int i = 0; i < _subnets; ++i) {
        _router[i] = _net[i]->GetRouters();
    }

    //seed the network
    RandomSeed(config.GetInt("seed"));

    _measure_latency = (config.GetStr("sim_type") == "latency");

    _sample_period = config.GetInt("sample_period");
    _max_samples = config.GetInt("max_samples");
    _warmup_periods = config.GetInt("warmup_periods");

    _measure_stats = config.GetIntArray("measure_stats");
    if (_measure_stats.empty()) {
        _measure_stats.push_back(config.GetInt("measure_stats"));
    }
    _measure_stats.resize(_classes, _measure_stats.back());
    _pair_stats = (config.GetInt("pair_stats") == 1);

    _latency_thres = config.GetFloatArray("latency_thres");
    if (_latency_thres.empty()) {
        _latency_thres.push_back(config.GetFloat("latency_thres"));
    }
    _latency_thres.resize(_classes, _latency_thres.back());

    _warmup_threshold = config.GetFloatArray("warmup_thres");
    if (_warmup_threshold.empty()) {
        _warmup_threshold.push_back(config.GetFloat("warmup_thres"));
    }
    _warmup_threshold.resize(_classes, _warmup_threshold.back());

    _acc_warmup_threshold = config.GetFloatArray("acc_warmup_thres");
    if (_acc_warmup_threshold.empty()) {
        _acc_warmup_threshold.push_back(config.GetFloat("acc_warmup_thres"));
    }
    _acc_warmup_threshold.resize(_classes, _acc_warmup_threshold.back());

    _stopping_threshold = config.GetFloatArray("stopping_thres");
    if (_stopping_threshold.empty()) {
        _stopping_threshold.push_back(config.GetFloat("stopping_thres"));
    }
    _stopping_threshold.resize(_classes, _stopping_threshold.back());

    _acc_stopping_threshold = config.GetFloatArray("acc_stopping_thres");
    if (_acc_stopping_threshold.empty()) {
        _acc_stopping_threshold.push_back(config.GetFloat("acc_stopping_thres"));
    }
    _acc_stopping_threshold.resize(_classes, _acc_stopping_threshold.back());

    _include_queuing = config.GetInt("include_queuing");

    _print_csv_results = config.GetInt("print_csv_results");
    _deadlock_warn_timeout = config.GetInt("deadlock_warn_timeout");

    string watch_file = config.GetStr("watch_file");
    if ((watch_file != "") && (watch_file != "-")) {
        _LoadWatchList(watch_file);
    }

    vector<int> watch_flits = config.GetIntArray("watch_flits");
    for (int & watch_flit : watch_flits) {
        _flits_to_watch.insert(watch_flit);
    }

    vector<int> watch_packets = config.GetIntArray("watch_packets");
    for (int & watch_packet : watch_packets) {
        _packets_to_watch.insert(watch_packet);
    }

    string stats_out_file = config.GetStr("stats_out");
    if (stats_out_file == "") {
        _stats_out = nullptr;
    } else if (stats_out_file == "-") {
        _stats_out = &cout;
    } else {
        _stats_out = new ofstream(stats_out_file.c_str());
        config.WriteMatlabFile(_stats_out);
    }

#ifdef TRACK_FLOWS
    _injected_flits.resize(_classes, vector<int>(_nodes, 0));
    _ejected_flits.resize(_classes, vector<int>(_nodes, 0));
    string injected_flits_out_file = config.GetStr( "injected_flits_out" );
    if(injected_flits_out_file == "")
    {
        _injected_flits_out = NULL;
    }
    else
    {
        _injected_flits_out = new ofstream(injected_flits_out_file.c_str());
    }
    string received_flits_out_file = config.GetStr( "received_flits_out" );
    if(received_flits_out_file == "")
    {
        _received_flits_out = NULL;
    }
    else
    {
        _received_flits_out = new ofstream(received_flits_out_file.c_str());
    }
    string stored_flits_out_file = config.GetStr( "stored_flits_out" );
    if(stored_flits_out_file == "")
    {
        _stored_flits_out = NULL;
    }
    else
    {
        _stored_flits_out = new ofstream(stored_flits_out_file.c_str());
    }
    string sent_flits_out_file = config.GetStr( "sent_flits_out" );
    if(sent_flits_out_file == "")
    {
        _sent_flits_out = NULL;
    }
    else
    {
        _sent_flits_out = new ofstream(sent_flits_out_file.c_str());
    }
    string outstanding_credits_out_file = config.GetStr( "outstanding_credits_out" );
    if(outstanding_credits_out_file == "")
    {
        _outstanding_credits_out = NULL;
    }
    else
    {
        _outstanding_credits_out = new ofstream(outstanding_credits_out_file.c_str());
    }
    string ejected_flits_out_file = config.GetStr( "ejected_flits_out" );
    if(ejected_flits_out_file == "")
    {
        _ejected_flits_out = NULL;
    }
    else
    {
        _ejected_flits_out = new ofstream(ejected_flits_out_file.c_str());
    }
    string active_packets_out_file = config.GetStr( "active_packets_out" );
    if(active_packets_out_file == "")
    {
        _active_packets_out = NULL;
    }
    else
    {
        _active_packets_out = new ofstream(active_packets_out_file.c_str());
    }
#endif

#ifdef TRACK_CREDITS
    string used_credits_out_file = config.GetStr( "used_credits_out" );
    if(used_credits_out_file == "")
    {
        _used_credits_out = NULL;
    }
    else
    {
        _used_credits_out = new ofstream(used_credits_out_file.c_str());
    }
    string free_credits_out_file = config.GetStr( "free_credits_out" );
    if(free_credits_out_file == "")
    {
        _free_credits_out = NULL;
    }
    else
    {
        _free_credits_out = new ofstream(free_credits_out_file.c_str());
    }
    string max_credits_out_file = config.GetStr( "max_credits_out" );
    if(max_credits_out_file == "")
    {
        _max_credits_out = NULL;
    }
    else
    {
        _max_credits_out = new ofstream(max_credits_out_file.c_str());
    }
#endif


    // ============ Statistics ============

    _plat_stats.resize(_classes);
    _overall_min_plat.resize(_classes, 0.0);
    _overall_avg_plat.resize(_classes, 0.0);
    _overall_max_plat.resize(_classes, 0.0);

    _nlat_stats.resize(_classes);
    _overall_min_nlat.resize(_classes, 0.0);
    _overall_avg_nlat.resize(_classes, 0.0);
    _overall_max_nlat.resize(_classes, 0.0);

    _flat_stats.resize(_classes);
    _overall_min_flat.resize(_classes, 0.0);
    _overall_avg_flat.resize(_classes, 0.0);
    _overall_max_flat.resize(_classes, 0.0);

    _frag_stats.resize(_classes);
    _overall_min_frag.resize(_classes, 0.0);
    _overall_avg_frag.resize(_classes, 0.0);
    _overall_max_frag.resize(_classes, 0.0);

    if (_pair_stats) {
        _pair_plat.resize(_classes);
        _pair_nlat.resize(_classes);
        _pair_flat.resize(_classes);
    }

    _hop_stats.resize(_classes);
    _overall_hop_stats.resize(_classes, 0.0);

    _sent_packets.resize(_classes);
    _overall_min_sent_packets.resize(_classes, 0.0);
    _overall_avg_sent_packets.resize(_classes, 0.0);
    _overall_max_sent_packets.resize(_classes, 0.0);
    _accepted_packets.resize(_classes);
    _overall_min_accepted_packets.resize(_classes, 0.0);
    _overall_avg_accepted_packets.resize(_classes, 0.0);
    _overall_max_accepted_packets.resize(_classes, 0.0);

    _sent_flits.resize(_classes);
    _overall_min_sent.resize(_classes, 0.0);
    _overall_avg_sent.resize(_classes, 0.0);
    _overall_max_sent.resize(_classes, 0.0);
    _accepted_flits.resize(_classes);
    _overall_min_accepted.resize(_classes, 0.0);
    _overall_avg_accepted.resize(_classes, 0.0);
    _overall_max_accepted.resize(_classes, 0.0);

#ifdef TRACK_STALLS
    _buffer_busy_stalls.resize(_classes);
    _buffer_conflict_stalls.resize(_classes);
    _buffer_full_stalls.resize(_classes);
    _buffer_reserved_stalls.resize(_classes);
    _crossbar_conflict_stalls.resize(_classes);
    _overall_buffer_busy_stalls.resize(_classes, 0);
    _overall_buffer_conflict_stalls.resize(_classes, 0);
    _overall_buffer_full_stalls.resize(_classes, 0);
    _overall_buffer_reserved_stalls.resize(_classes, 0);
    _overall_crossbar_conflict_stalls.resize(_classes, 0);
#endif

    for (int c = 0; c < _classes; ++c) {
        ostringstream tmp_name;

        tmp_name << "plat_stat_" << c;
        _plat_stats[c] = new Stats(this, tmp_name.str(), 1.0, 1000);
        _stats[tmp_name.str()] = _plat_stats[c];
        tmp_name.str("");

        tmp_name << "nlat_stat_" << c;
        _nlat_stats[c] = new Stats(this, tmp_name.str(), 1.0, 1000);
        _stats[tmp_name.str()] = _nlat_stats[c];
        tmp_name.str("");

        tmp_name << "flat_stat_" << c;
        _flat_stats[c] = new Stats(this, tmp_name.str(), 1.0, 1000);
        _stats[tmp_name.str()] = _flat_stats[c];
        tmp_name.str("");

        tmp_name << "frag_stat_" << c;
        _frag_stats[c] = new Stats(this, tmp_name.str(), 1.0, 100);
        _stats[tmp_name.str()] = _frag_stats[c];
        tmp_name.str("");

        tmp_name << "hop_stat_" << c;
        _hop_stats[c] = new Stats(this, tmp_name.str(), 1.0, 20);
        _stats[tmp_name.str()] = _hop_stats[c];
        tmp_name.str("");

        if (_pair_stats) {
            _pair_plat[c].resize(_nodes * _nodes);
            _pair_nlat[c].resize(_nodes * _nodes);
            _pair_flat[c].resize(_nodes * _nodes);
        }

        _sent_packets[c].resize(_nodes, 0);
        _accepted_packets[c].resize(_nodes, 0);
        _sent_flits[c].resize(_nodes, 0);
        _accepted_flits[c].resize(_nodes, 0);

#ifdef TRACK_STALLS
        _buffer_busy_stalls[c].resize(_subnets*_routers, 0);
        _buffer_conflict_stalls[c].resize(_subnets*_routers, 0);
        _buffer_full_stalls[c].resize(_subnets*_routers, 0);
        _buffer_reserved_stalls[c].resize(_subnets*_routers, 0);
        _crossbar_conflict_stalls[c].resize(_subnets*_routers, 0);
#endif
        if (_pair_stats) {
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    tmp_name << "pair_plat_stat_" << c << "_" << i << "_" << j;
                    _pair_plat[c][i * _nodes + j] = new Stats(this, tmp_name.str(), 1.0, 250);
                    _stats[tmp_name.str()] = _pair_plat[c][i * _nodes + j];
                    tmp_name.str("");

                    tmp_name << "pair_nlat_stat_" << c << "_" << i << "_" << j;
                    _pair_nlat[c][i * _nodes + j] = new Stats(this, tmp_name.str(), 1.0, 250);
                    _stats[tmp_name.str()] = _pair_nlat[c][i * _nodes + j];
                    tmp_name.str("");

                    tmp_name << "pair_flat_stat_" << c << "_" << i << "_" << j;
                    _pair_flat[c][i * _nodes + j] = new Stats(this, tmp_name.str(), 1.0, 250);
                    _stats[tmp_name.str()] = _pair_flat[c][i * _nodes + j];
                    tmp_name.str("");
                }
            }
        }
    }

    _slowest_flit.resize(_classes, -1);
    _slowest_packet.resize(_classes, -1);


    // JJ
    _returnPackets = nullptr;
    prev_latency.resize(_classes, 0.0);
    prev_accepted.resize(_classes, 0.0);
    int total_phases = 0;


}

TrafficManager::~TrafficManager() {

    for (int source = 0; source < _nodes; ++source) {
        for (int subnet = 0; subnet < _subnets; ++subnet) {
            delete _buf_states[source][subnet];
        }
    }

    for (int c = 0; c < _classes; ++c) {
        delete _plat_stats[c];
        delete _nlat_stats[c];
        delete _flat_stats[c];
        delete _frag_stats[c];
        delete _hop_stats[c];

        delete _traffic_pattern[c];
        delete _injection_process[c];

        if (_pair_stats) {
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    delete _pair_plat[c][i * _nodes + j];
                    delete _pair_nlat[c][i * _nodes + j];
                    delete _pair_flat[c][i * _nodes + j];
                }
            }
        }
    }

    if (gWatchOut && (gWatchOut != &cout)) delete gWatchOut;
    if (_stats_out && (_stats_out != &cout)) delete _stats_out;

#ifdef TRACK_FLOWS
    if(_injected_flits_out) delete _injected_flits_out;
    if(_received_flits_out) delete _received_flits_out;
    if(_stored_flits_out) delete _stored_flits_out;
    if(_sent_flits_out) delete _sent_flits_out;
    if(_outstanding_credits_out) delete _outstanding_credits_out;
    if(_ejected_flits_out) delete _ejected_flits_out;
    if(_active_packets_out) delete _active_packets_out;
#endif

#ifdef TRACK_CREDITS
    if(_used_credits_out) delete _used_credits_out;
    if(_free_credits_out) delete _free_credits_out;
    if(_max_credits_out) delete _max_credits_out;
#endif

    PacketReplyInfo::FreeAll();
    Flit::FreeAll();
    Credit::FreeAll();
}


void TrafficManager::_RetireFlit(Flit *f, int dest) {
    _deadlock_timer = 0;

    assert(_total_in_flight_flits[f->cl].count(f->id) > 0);
    _total_in_flight_flits[f->cl].erase(f->id);

    if (f->record) {
        assert(_measured_in_flight_flits[f->cl].count(f->id) > 0);
        _measured_in_flight_flits[f->cl].erase(f->id);
    }

    if (f->watch) {
        *gWatchOut << GetSimTime() << " | "
                   << "node" << dest << " | "
                   << "Retiring flit " << f->id
                   << " (packet " << f->pid
                   << ", src = " << f->src
                   << ", dest = " << f->dest
                   << ", hops = " << f->hops
                   << ", flat = " << f->atime - f->itime
                   << ")." << endl;
    }

    if (f->head && (f->dest != dest)) {
        ostringstream err;
        err << "Flit " << f->id << " arrived at incorrect output " << dest;
        Error(err.str());
    }

    if ((_slowest_flit[f->cl] < 0) ||
        (_flat_stats[f->cl]->Max() < (f->atime - f->itime)))
        _slowest_flit[f->cl] = f->id;
    _flat_stats[f->cl]->AddSample((double) (f->atime - f->itime));
    if (_pair_stats) {
        _pair_flat[f->cl][f->src * _nodes + dest]->AddSample((double) (f->atime - f->itime));
    }

    if (f->tail) {
        Flit *head;
        if (f->head) {
            head = f;
        } else {
            auto iter = _retired_packets[f->cl].find(f->pid);
            assert(iter != _retired_packets[f->cl].end());
            head = iter->second;
            _retired_packets[f->cl].erase(iter);
            assert(head->head);
            assert(f->pid == head->pid);
        }
        if (f->watch) {
            *gWatchOut << GetSimTime() << " | "
                       << "node" << dest << " | "
                       << "Retiring packet " << f->pid
                       << " (plat = " << f->atime - head->ctime
                       << ", nlat = " << f->atime - head->itime
                       << ", frag = " << (f->atime - head->atime) - (f->id -
                                                                     head->id) // NB: In the spirit of solving problems using ugly hacks, we compute the packet length by taking advantage of the fact that the IDs of flits within a packet are contiguous.
                       << ", src = " << head->src
                       << ", dest = " << head->dest
                       << ")." << endl;
        }

        //code the source of request, look carefully, its tricky ;)
        if (f->type == Flit::READ_REQUEST || f->type == Flit::WRITE_REQUEST) {
            assert(false);
        } else {
            if (f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY) {
                _requestsOutstanding[dest]--;
            } else if (f->type == Flit::ANY_TYPE) {
                _requestsOutstanding[f->src]--;
            }

        }

        // Only record statistics once per packet (at tail)
        // and based on the simulation state
        if ((_sim_state == warming_up) || f->record) {

            _hop_stats[f->cl]->AddSample(f->hops);

            if ((_slowest_packet[f->cl] < 0) ||
                (_plat_stats[f->cl]->Max() < (f->atime - head->itime)))
                _slowest_packet[f->cl] = f->pid;
            _plat_stats[f->cl]->AddSample((double) (f->atime - head->ctime));
            _nlat_stats[f->cl]->AddSample((double) (f->atime - head->itime));
            _frag_stats[f->cl]->AddSample((double) (f->atime - head->atime) - (f->id - head->id));

            if (_pair_stats) {
                _pair_plat[f->cl][f->src * _nodes + dest]->AddSample((double) (f->atime - head->ctime));
                _pair_nlat[f->cl][f->src * _nodes + dest]->AddSample((double) (f->atime - head->itime));
            }

        }
        // JJ
        // Return to SESC
        assert(_returnPackets != nullptr);
        _returnPackets->push_back(make_pair(f->SESCPkt, make_pair(f->hops, f->atime - head->ctime)));

        if (f != head) {
            head->Free();
        }

    }

    if (f->head && !f->tail) {
        _retired_packets[f->cl].insert(make_pair(f->pid, f));
    } else {
        f->Free();
    }
}

void TrafficManager::_InjectPacket() {
    for (int input = 0; input < _nodes; ++input) {
        for (int c = 0; c < _classes; ++c) {
            // Potentially generate packets for any (input,class)
            // that is currently empty
            if (_partial_packets[input][c].empty()) {
                // Generate Packet
                while (!_packetBuffer[input][c].empty()) {
                    SESCPacket *p = _packetBuffer[input][c].front();
                    assert(p != nullptr);
                    _packetBuffer[input][c].pop_front();

                    _packet_seq_no[input]++;
                    _requestsOutstanding[input]++;

                    _GenerateSESCPacket(input, c, p, _time);

                    p->Free();
                    if (_include_queuing == 1) {
                        break;
                    }
                }
            }
        }
    }

}

void TrafficManager::_GenerateSESCPacket(int source, int cl, SESCPacket *p, BTime_t time) {
    Flit::FlitType packet_type = Flit::ANY_TYPE;
    double sizeD = p->GetSize() / _channel_width_byte;
    int size = (int) ceil(sizeD);    //_GetNextPacketSize(cl); //input size
    int pid = _cur_pid++;
    assert(_cur_pid);
    int packet_destination = p->GetDest(); //_traffic_pattern[cl]->dest(source);
    bool record = false;
    //bool watch = gWatchOut && (_packets_to_watch.count(pid) > 0);
    bool watch = false;

    if ((packet_destination < 0) || (packet_destination >= _nodes)) {
        ostringstream err;
        err << "Incorrect packet destination " << packet_destination
            << " for stype " << packet_type;
        Error(err.str());
    }

    record = _measure_stats[cl];

    int subnetwork = 0;

    if (watch) {
        *gWatchOut << GetSimTime() << " | "
                   << "node" << source << " | "
                   << "Enqueuing packet " << pid
                   << " at time " << time
                   << "." << endl;
    }

    for (int i = 0; i < size; ++i) {
        Flit *f = Flit::New();
        f->id = _cur_id++;
        assert(_cur_id);
        f->pid = pid;
        //f->watch  = watch | (gWatchOut && (_flits_to_watch.count(f->id) > 0));
        f->watch = false;
        f->subnetwork = subnetwork;
        f->src = source;
        f->ctime = time;
        f->record = record;
        f->cl = cl;
        // JJ
        f->SESCPkt = p->GetSESCPkt();

        _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        if (record) {
            _measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        }

        if (gTrace) {
            cout << "New Flit " << f->src << endl;
        }
        f->type = packet_type;

        if (i == 0)   // Head flit
        {
            f->head = true;
            //packets are only generated to nodes smaller or equal to limit
            f->dest = packet_destination;
        } else {
            f->head = false;
            f->dest = -1;
        }
        switch (_pri_type) {
            case class_based:
                f->pri = _class_priority[cl];
                assert(f->pri >= 0);
                break;
            case age_based:
                f->pri = numeric_limits<BPri_t>::max() - time;
                //f->pri = numeric_limits<int>::max() - time;
                assert(f->pri >= 0);
                break;
            case sequence_based:
                f->pri = numeric_limits<BPri_t>::max() - _packet_seq_no[source];
                //f->pri = numeric_limits<int>::max() - _packet_seq_no[source];
                assert(f->pri >= 0);
                break;
            default:
                f->pri = 0;
        }
        if (i == (size - 1))   // Tail flit
        {
            f->tail = true;
        } else {
            f->tail = false;
        }

        f->vc = -1;

        if (f->watch) {
            *gWatchOut << GetSimTime() << " | "
                       << "node" << source << " | "
                       << "Enqueuing flit " << f->id
                       << " (packet " << f->pid
                       << ") at time " << time
                       << "." << endl;
        }

        _partial_packets[source][cl].push_back(f);
    }

}

void TrafficManager::BufferPacket(int f, int t, int c, int msgSize, void *pkt) {
    SESCPacket *p = SESCPacket::Get(f, t, c, msgSize, pkt);
    _packetBuffer[f][c].push_back(p);
}

bool TrafficManager::_PacketsOutstanding() const {
    for (int c = 0; c < _classes; ++c) {
        if (_measure_stats[c]) {
            if (_measured_in_flight_flits[c].empty()) {

                for (int s = 0; s < _nodes; ++s) {
                    if (!_qdrained[s][c]) {
#ifdef DEBUG_DRAIN
                        cout << "waiting on queue " << s << " class " << c;
                        cout << ", time = " << _time << " qtime = " << _qtime[s][c] << endl;
#endif
                        return true;
                    }
                }
            } else {
#ifdef DEBUG_DRAIN
                cout << "in flight = " << _measured_in_flight_flits[c].size() << endl;
#endif
                return true;
            }
        }
    }
    return false;
}

void TrafficManager::_ClearStats() {
    _slowest_flit.assign(_classes, -1);
    _slowest_packet.assign(_classes, -1);

    for (int c = 0; c < _classes; ++c) {

        _plat_stats[c]->Clear();
        _nlat_stats[c]->Clear();
        _flat_stats[c]->Clear();

        _frag_stats[c]->Clear();

        _sent_packets[c].assign(_nodes, 0);
        _accepted_packets[c].assign(_nodes, 0);
        _sent_flits[c].assign(_nodes, 0);
        _accepted_flits[c].assign(_nodes, 0);

#ifdef TRACK_STALLS
        _buffer_busy_stalls[c].assign(_subnets*_routers, 0);
        _buffer_conflict_stalls[c].assign(_subnets*_routers, 0);
        _buffer_full_stalls[c].assign(_subnets*_routers, 0);
        _buffer_reserved_stalls[c].assign(_subnets*_routers, 0);
        _crossbar_conflict_stalls[c].assign(_subnets*_routers, 0);
#endif

        if (_pair_stats) {
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    _pair_plat[c][i * _nodes + j]->Clear();
                    _pair_nlat[c][i * _nodes + j]->Clear();
                    _pair_flat[c][i * _nodes + j]->Clear();
                }
            }
        }

        _hop_stats[c]->Clear();

    }

    _reset_time = _time;
}

void TrafficManager::_ComputeStats(const vector<int> &stats, int *sum, int *min, int *max, int *min_pos,
                                   int *max_pos) const {
    int const count = stats.size();
    assert(count > 0);

    if (min_pos) {
        *min_pos = 0;
    }
    if (max_pos) {
        *max_pos = 0;
    }

    if (min) {
        *min = stats[0];
    }
    if (max) {
        *max = stats[0];
    }

    *sum = stats[0];

    for (int i = 1; i < count; ++i) {
        int curr = stats[i];
        if (min && (curr < *min)) {
            *min = curr;
            if (min_pos) {
                *min_pos = i;
            }
        }
        if (max && (curr > *max)) {
            *max = curr;
            if (max_pos) {
                *max_pos = i;
            }
        }
        *sum += curr;
    }
}

void TrafficManager::_DisplayRemaining(ostream &os) const {
    for (int c = 0; c < _classes; ++c) {

        map<BId_t, Flit *>::const_iterator iter;
        int i;

        os << "Class " << c << ":" << endl;

        os << "Remaining flits: ";
        for (iter = _total_in_flight_flits[c].begin(), i = 0;
             (iter != _total_in_flight_flits[c].end()) && (i < 10);
             iter++, i++) {
            os << iter->first << " ";
        }
        if (_total_in_flight_flits[c].size() > 10)
            os << "[...] ";

        os << "(" << _total_in_flight_flits[c].size() << " flits)" << endl;

        os << "Measured flits: ";
        for (iter = _measured_in_flight_flits[c].begin(), i = 0;
             (iter != _measured_in_flight_flits[c].end()) && (i < 10);
             iter++, i++) {
            os << iter->first << " ";
        }
        if (_measured_in_flight_flits[c].size() > 10)
            os << "[...] ";

        os << "(" << _measured_in_flight_flits[c].size() << " flits)" << endl;

    }
}

void TrafficManager::Init(deque<pair<void *, pair<int, int> > > *returnPackets, ofstream *os_out) {
    //_time = 2147483000;
    _time = 0;

    _os_out = os_out;

    _returnPackets = returnPackets;

    //remove any pending request from the previous simulations
    _requestsOutstanding.assign(_nodes, 0);

    //reset queuetime for all sources
    for (int s = 0; s < _nodes; ++s) {
        _qtime[s].assign(_classes, 0);
        _qdrained[s].assign(_classes, false);
    }

    // warm-up ...
    // reset stats, all packets after warmup_time marked
    // converge
    // draing, wait until all packets finish
    _sim_state = warming_up;

    _ClearStats();

    for (int c = 0; c < _classes; ++c) {
        _traffic_pattern[c]->reset();
        _injection_process[c]->reset();
    }

    _sim_state = running;
}

bool TrafficManager::Checkpoint() {
    assert(_sim_state == running);

    UpdateStats();
    DisplayStats(*_os_out);

    int lat_exc_class = -1;

    bool rval = true;

    for (int c = 0; c < _classes; ++c) {

        if (_measure_stats[c] == 0) {
            continue;
        }

        double cur_latency = _plat_stats[c]->Average();

        int total_accepted_count;
        _ComputeStats(_accepted_flits[c], &total_accepted_count);
        double total_accepted_rate = (double) total_accepted_count / (double) (_time - _reset_time);
        double cur_accepted = total_accepted_rate / (double) _nodes;

        double latency_change = fabs((cur_latency - prev_latency[c]) / cur_latency);
        prev_latency[c] = cur_latency;

        double accepted_change = fabs((cur_accepted - prev_accepted[c]) / cur_accepted);
        prev_accepted[c] = cur_accepted;

        auto latency = (double) _plat_stats[c]->Sum();
        auto count = (double) _plat_stats[c]->NumSamples();

        map<BId_t, Flit *>::const_iterator iter;
        for (iter = _total_in_flight_flits[c].begin();
             iter != _total_in_flight_flits[c].end();
             iter++) {
            latency += (double) (_time - iter->second->ctime);
            count++;
        }

        if ((lat_exc_class < 0) &&
            (_latency_thres[c] >= 0.0) &&
            ((latency / count) > _latency_thres[c])) {
            lat_exc_class = c;
        }

        *_os_out << "latency change    = " << latency_change << endl;
        *_os_out << "throughput change = " << accepted_change << endl;
    }

    // Fail safe for latency mode, throughput will ust continue
    if (_measure_latency && (lat_exc_class >= 0)) {
        cout << "Average latency for class " << lat_exc_class << " exceeded " << _latency_thres[lat_exc_class]
             << " cycles. Aborting simulation." << endl;
        rval = false;
    }

    *_os_out << endl;

    ++total_phases;

    return rval;
}

void TrafficManager::Finish() {

    assert(_sim_state == running);

    _sim_state = draining;
    _drain_time = _time;

    // Empty any remaining packets

    _empty_network = true;
    int empty_steps = 0;

    bool packets_left = false;
    for (int c = 0; c < _classes; ++c) {
        packets_left |= !_total_in_flight_flits[c].empty();
    }

    *_os_out << "Draining remaining packets ..." << endl;
    *_os_out << "Time " << _time << endl;

    while (packets_left) {
        CallEveryCycle();

        ++empty_steps;

        packets_left = false;
        for (int c = 0; c < _classes; ++c) {
            packets_left |= !_total_in_flight_flits[c].empty();
        }
    }
    //wait until all the credits are drained as well
    while (Credit::OutStanding() != 0) {
        CallEveryCycle();
    }

    _empty_network = false;



    //for the love of god don't ever say "Time taken" anywhere else
    //the power script depend on it
    *_os_out << "Total phase " << total_phases << endl;
    *_os_out << "Time taken is " << _time << " cycles" << endl;

    if (_stats_out) {
        WriteStats(*_stats_out);
    }
    _UpdateOverallStats();


    DisplayOverallStats(*_os_out);
    if (_print_csv_results) {
        DisplayOverallStatsCSV();
    }

}

bool TrafficManager::IsFlitInFlight() const {
    bool flits_in_flight = false;
    for (int c = 0; c < _classes; ++c) {
        flits_in_flight |= !_total_in_flight_flits[c].empty();
    }
    return flits_in_flight;
}

void TrafficManager::CallEveryCycle() {
    assert(_sim_state == running || _sim_state == draining);
    if (_classes % 2 != 0 || _classes == 1) __builtin_unreachable();

    bool flits_in_flight = false;
    for (int c = 0; c < _classes; ++c) {
        flits_in_flight |= !_total_in_flight_flits[c].empty();
    }
    if (flits_in_flight && (_deadlock_timer++ >= _deadlock_warn_timeout)) {
        _deadlock_timer = 0;
        cout << "WARNING: Possible network deadlock.\n";
    }

    vector<map<int, Flit *> > flits(_subnets);

    for (int subnet = 0; subnet < _subnets; ++subnet) {
        for (int n = 0; n < _nodes; ++n) {
            Flit *const f = _net[subnet]->ReadFlit(n);
            if (f) {
                if (f->watch) {
                    *gWatchOut << GetSimTime() << " | "
                               << "node" << n << " | "
                               << "Ejecting flit " << f->id
                               << " (packet " << f->pid << ")"
                               << " from VC " << f->vc
                               << "." << endl;
                }
                flits[subnet].insert(make_pair(n, f));
                if ((_sim_state == warming_up) || (_sim_state == running)) {
                    ++_accepted_flits[f->cl][n];
                    if (f->tail) {
                        ++_accepted_packets[f->cl][n];
                    }
                }
            }

            Credit *const c = _net[subnet]->ReadCredit(n);
            if (c) {
#ifdef TRACK_FLOWS
                for(set<int>::const_iterator iter = c->vc.begin(); iter != c->vc.end(); ++iter)
                {
                    int const vc = *iter;
                    assert(!_outstanding_classes[n][subnet][vc].empty());
                    int cl = _outstanding_classes[n][subnet][vc].front();
                    _outstanding_classes[n][subnet][vc].pop();
                    assert(_outstanding_credits[cl][subnet][n] > 0);
                    --_outstanding_credits[cl][subnet][n];
                }
#endif
                _buf_states[n][subnet]->ProcessCredit(c);
                c->Free();
            }
        }
        _net[subnet]->ReadInputs();
    }

    if (!_empty_network) {
        _InjectPacket();
    }

    for (int subnet = 0; subnet < _subnets; ++subnet) {

        for (int n = 0; n < _nodes; ++n) {

            Flit *f = nullptr;

            BufferState *const dest_buf = _buf_states[n][subnet];

            int const last_class = _last_class[n][subnet];

            int class_limit = _classes;

            if (_hold_switch_for_packet) {
                deque<Flit *> const &pp = _partial_packets[n][last_class];
                if (!pp.empty() && !pp.front()->head &&
                    !dest_buf->IsFullFor(pp.front()->vc)) {
                    f = pp.front();
                    assert(f->vc == _last_vc[n][subnet][last_class]);

                    // if we're holding the connection, we don't need to check that class
                    // again in the for loop
                    --class_limit;
                }
            }

            for (int i = 1; i <= class_limit; ++i) {

                int const c = _classes == 1 ? 0 : (last_class + i) % _classes;

                deque<Flit *> const &pp = _partial_packets[n][c];

                if (pp.empty()) {
                    continue;
                }

                Flit *const cf = pp.front();
                assert(cf);
                assert(cf->cl == c);

                if (cf->subnetwork != subnet) {
                    continue;
                }

                if (f && (f->pri >= cf->pri)) {
                    continue;
                }

                if (cf->head && cf->vc == -1)   // Find first available VC
                {

                    OutputSet route_set;
                    _rf(nullptr, cf, -1, &route_set, true);
                    set<OutputSet::sSetElement> const &os = route_set.GetSet();
                    assert(os.size() == 1);
                    OutputSet::sSetElement const &se = *os.begin();
                    assert(se.output_port == -1);
                    int vc_start = se.vc_start;
                    int vc_end = se.vc_end;
                    int vc_count = vc_end - vc_start + 1;
                    if (_noq) {
                        assert(_lookahead_routing);
                        const FlitChannel *inject = _net[subnet]->GetInject(n);
                        const Router *router = inject->GetSink();
                        assert(router);
                        int in_channel = inject->GetSinkPort();

                        // NOTE: Because the lookahead is not for injection, but for the
                        // first hop, we have to temporarily set cf's VC to be non-negative
                        // in order to avoid seting of an assertion in the routing function.
                        cf->vc = vc_start;
                        _rf(router, cf, in_channel, &cf->la_route_set, false);
                        cf->vc = -1;

                        if (cf->watch) {
                            *gWatchOut << GetSimTime() << " | "
                                       << "node" << n << " | "
                                       << "Generating lookahead routing info for flit " << cf->id
                                       << " (NOQ)." << endl;
                        }
                        set<OutputSet::sSetElement> const sl = cf->la_route_set.GetSet();
                        assert(sl.size() == 1);
                        int next_output = sl.begin()->output_port;
                        vc_count /= router->NumOutputs();
                        vc_start += next_output * vc_count;
                        vc_end = vc_start + vc_count - 1;
                        assert(vc_start >= se.vc_start && vc_start <= se.vc_end);
                        assert(vc_end >= se.vc_start && vc_end <= se.vc_end);
                        assert(vc_start <= vc_end);
                    }
                    if (cf->watch) {
                        *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                   << "Finding output VC for flit " << cf->id
                                   << ":" << endl;
                    }
                    for (int i = 1; i <= vc_count; ++i) {
                        int const lvc = _last_vc[n][subnet][c];
                        int const vc =
                                (lvc < vc_start || lvc > vc_end) ?
                                vc_start :
                                (vc_start + (lvc - vc_start + i) % vc_count);
                        assert((vc >= vc_start) && (vc <= vc_end));
                        if (!dest_buf->IsAvailableFor(vc)) {
                            if (cf->watch) {
                                *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                           << "  Output VC " << vc << " is busy." << endl;
                            }
                        } else {
                            if (dest_buf->IsFullFor(vc)) {
                                if (cf->watch) {
                                    *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                               << "  Output VC " << vc << " is full." << endl;
                                }
                            } else {
                                if (cf->watch) {
                                    *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                               << "  Selected output VC " << vc << "." << endl;
                                }
                                cf->vc = vc;
                                break;
                            }
                        }
                    }
                }

                if (cf->vc == -1) {
                    if (cf->watch) {
                        *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                   << "No output VC found for flit " << cf->id
                                   << "." << endl;
                    }
                } else {
                    if (dest_buf->IsFullFor(cf->vc)) {
                        if (cf->watch) {
                            *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                                       << "Selected output VC " << cf->vc
                                       << " is full for flit " << cf->id
                                       << "." << endl;
                        }
                    } else {
                        f = cf;
                    }
                }
            }

            if (f) {

                assert(f->subnetwork == subnet);

                int const c = f->cl;

                if (f->head) {

                    if (_lookahead_routing) {
                        if (!_noq) {
                            const FlitChannel *inject = _net[subnet]->GetInject(n);
                            const Router *router = inject->GetSink();
                            assert(router);
                            int in_channel = inject->GetSinkPort();
                            _rf(router, f, in_channel, &f->la_route_set, false);
                            if (f->watch) {
                                *gWatchOut << GetSimTime() << " | "
                                           << "node" << n << " | "
                                           << "Generating lookahead routing info for flit " << f->id
                                           << "." << endl;
                            }
                        } else if (f->watch) {
                            *gWatchOut << GetSimTime() << " | "
                                       << "node" << n << " | "
                                       << "Already generated lookahead routing info for flit " << f->id
                                       << " (NOQ)." << endl;
                        }
                    } else {
                        f->la_route_set.Clear();
                    }

                    dest_buf->TakeBuffer(f->vc);
                    _last_vc[n][subnet][c] = f->vc;
                }

                _last_class[n][subnet] = c;

                _partial_packets[n][c].pop_front();

#ifdef TRACK_FLOWS
                ++_outstanding_credits[c][subnet][n];
                _outstanding_classes[n][subnet][f->vc].push(c);
#endif

                dest_buf->SendingFlit(f);

                if (_pri_type == network_age_based) {
                    f->pri = numeric_limits<BPri_t>::max() - _time;
                    //f->pri = numeric_limits<int>::max() - _time;
                    assert(f->pri >= 0);
                }

                if (f->watch) {
                    *gWatchOut << GetSimTime() << " | "
                               << "node" << n << " | "
                               << "Injecting flit " << f->id
                               << " into subnet " << subnet
                               << " at time " << _time
                               << " with priority " << f->pri
                               << "." << endl;
                }
                f->itime = _time;

                // Pass VC "back"
                if (!_partial_packets[n][c].empty() && !f->tail) {
                    Flit *const nf = _partial_packets[n][c].front();
                    nf->vc = f->vc;
                }

                if ((_sim_state == warming_up) || (_sim_state == running)) {
                    ++_sent_flits[c][n];
                    if (f->head) {
                        ++_sent_packets[c][n];
                    }
                }

#ifdef TRACK_FLOWS
                ++_injected_flits[c][n];
#endif

                _net[subnet]->WriteFlit(f, n);

            }
        }
    }

    for (int subnet = 0; subnet < _subnets; ++subnet) {
        for (int n = 0; n < _nodes; ++n) {
            map<int, Flit *>::const_iterator iter = flits[subnet].find(n);
            if (iter != flits[subnet].end()) {
                Flit *const f = iter->second;

                f->atime = _time;
                if (f->watch) {
                    *gWatchOut << GetSimTime() << " | "
                               << "node" << n << " | "
                               << "Injecting credit for VC " << f->vc
                               << " into subnet " << subnet
                               << "." << endl;
                }
                Credit *const c = Credit::New();
                c->vc.insert(f->vc);
                _net[subnet]->WriteCredit(c, n);

#ifdef TRACK_FLOWS
                ++_ejected_flits[f->cl][n];
#endif

                _RetireFlit(f, n);
            }
        }
        flits[subnet].clear();
        _net[subnet]->Evaluate();
        _net[subnet]->WriteOutputs();
    }

    ++_time;
    assert(_time);
    //assert(_time==globalClock);
    if (gTrace) {
        cout << "TIME " << _time << endl;
    }

}

void TrafficManager::_UpdateOverallStats() {
    for (int c = 0; c < _classes; ++c) {

        if (_measure_stats[c] == 0) {
            continue;
        }

        _overall_min_plat[c] += _plat_stats[c]->Min();
        _overall_avg_plat[c] += _plat_stats[c]->Average();
        _overall_max_plat[c] += _plat_stats[c]->Max();
        _overall_min_nlat[c] += _nlat_stats[c]->Min();
        _overall_avg_nlat[c] += _nlat_stats[c]->Average();
        _overall_max_nlat[c] += _nlat_stats[c]->Max();
        _overall_min_flat[c] += _flat_stats[c]->Min();
        _overall_avg_flat[c] += _flat_stats[c]->Average();
        _overall_max_flat[c] += _flat_stats[c]->Max();

        _overall_min_frag[c] += _frag_stats[c]->Min();
        _overall_avg_frag[c] += _frag_stats[c]->Average();
        _overall_max_frag[c] += _frag_stats[c]->Max();

        _overall_hop_stats[c] += _hop_stats[c]->Average();

        int count_min, count_sum, count_max;
        double rate_min, rate_sum, rate_max;
        double rate_avg;
        auto time_delta = (double) (_drain_time - _reset_time);
        _ComputeStats(_sent_flits[c], &count_sum, &count_min, &count_max);
        rate_min = (double) count_min / time_delta;
        rate_sum = (double) count_sum / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        _overall_min_sent[c] += rate_min;
        _overall_avg_sent[c] += rate_avg;
        _overall_max_sent[c] += rate_max;
        _ComputeStats(_sent_packets[c], &count_sum, &count_min, &count_max);
        rate_min = (double) count_min / time_delta;
        rate_sum = (double) count_sum / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        _overall_min_sent_packets[c] += rate_min;
        _overall_avg_sent_packets[c] += rate_avg;
        _overall_max_sent_packets[c] += rate_max;
        _ComputeStats(_accepted_flits[c], &count_sum, &count_min, &count_max);
        rate_min = (double) count_min / time_delta;
        rate_sum = (double) count_sum / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        _overall_min_accepted[c] += rate_min;
        _overall_avg_accepted[c] += rate_avg;
        _overall_max_accepted[c] += rate_max;
        _ComputeStats(_accepted_packets[c], &count_sum, &count_min, &count_max);
        rate_min = (double) count_min / time_delta;
        rate_sum = (double) count_sum / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        _overall_min_accepted_packets[c] += rate_min;
        _overall_avg_accepted_packets[c] += rate_avg;
        _overall_max_accepted_packets[c] += rate_max;

#ifdef TRACK_STALLS
        _ComputeStats(_buffer_busy_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        _overall_buffer_busy_stalls[c] += rate_avg;
        _ComputeStats(_buffer_conflict_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        _overall_buffer_conflict_stalls[c] += rate_avg;
        _ComputeStats(_buffer_full_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        _overall_buffer_full_stalls[c] += rate_avg;
        _ComputeStats(_buffer_reserved_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        _overall_buffer_reserved_stalls[c] += rate_avg;
        _ComputeStats(_crossbar_conflict_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        _overall_crossbar_conflict_stalls[c] += rate_avg;
#endif

    }
}

void TrafficManager::WriteStats(ostream &os) const {

    os << "%=================================" << endl;

    for (int c = 0; c < _classes; ++c) {

        if (_measure_stats[c] == 0) {
            continue;
        }

        //c+1 due to matlab array starting at 1
        os << "plat(" << c + 1 << ") = " << _plat_stats[c]->Average() << ";" << endl
           << "plat_hist(" << c + 1 << ",:) = " << *_plat_stats[c] << ";" << endl
           << "nlat(" << c + 1 << ") = " << _nlat_stats[c]->Average() << ";" << endl
           << "nlat_hist(" << c + 1 << ",:) = " << *_nlat_stats[c] << ";" << endl
           << "flat(" << c + 1 << ") = " << _flat_stats[c]->Average() << ";" << endl
           << "flat_hist(" << c + 1 << ",:) = " << *_flat_stats[c] << ";" << endl
           << "frag_hist(" << c + 1 << ",:) = " << *_frag_stats[c] << ";" << endl
           << "hops(" << c + 1 << ",:) = " << *_hop_stats[c] << ";" << endl;
        if (_pair_stats) {
            os << "pair_sent(" << c + 1 << ",:) = [ ";
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    os << _pair_plat[c][i * _nodes + j]->NumSamples() << " ";
                }
            }
            os << "];" << endl
               << "pair_plat(" << c + 1 << ",:) = [ ";
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    os << _pair_plat[c][i * _nodes + j]->Average() << " ";
                }
            }
            os << "];" << endl
               << "pair_nlat(" << c + 1 << ",:) = [ ";
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    os << _pair_nlat[c][i * _nodes + j]->Average() << " ";
                }
            }
            os << "];" << endl
               << "pair_flat(" << c + 1 << ",:) = [ ";
            for (int i = 0; i < _nodes; ++i) {
                for (int j = 0; j < _nodes; ++j) {
                    os << _pair_flat[c][i * _nodes + j]->Average() << " ";
                }
            }
        }

        auto time_delta = (double) (_drain_time - _reset_time);

        os << "];" << endl
           << "sent_packets(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _sent_packets[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "accepted_packets(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _accepted_packets[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "sent_flits(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _sent_flits[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "accepted_flits(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _accepted_flits[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "sent_packet_size(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _sent_flits[c][d] / (double) _sent_packets[c][d] << " ";
        }
        os << "];" << endl
           << "accepted_packet_size(" << c + 1 << ",:) = [ ";
        for (int d = 0; d < _nodes; ++d) {
            os << (double) _accepted_flits[c][d] / (double) _accepted_packets[c][d] << " ";
        }
        os << "];" << endl;
#ifdef TRACK_STALLS
        os << "buffer_busy_stalls(" << c+1 << ",:) = [ ";
        for ( int d = 0; d < _subnets*_routers; ++d )
        {
            os << (double)_buffer_busy_stalls[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "buffer_conflict_stalls(" << c+1 << ",:) = [ ";
        for ( int d = 0; d < _subnets*_routers; ++d )
        {
            os << (double)_buffer_conflict_stalls[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "buffer_full_stalls(" << c+1 << ",:) = [ ";
        for ( int d = 0; d < _subnets*_routers; ++d )
        {
            os << (double)_buffer_full_stalls[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "buffer_reserved_stalls(" << c+1 << ",:) = [ ";
        for ( int d = 0; d < _subnets*_routers; ++d )
        {
            os << (double)_buffer_reserved_stalls[c][d] / time_delta << " ";
        }
        os << "];" << endl
           << "crossbar_conflict_stalls(" << c+1 << ",:) = [ ";
        for ( int d = 0; d < _subnets*_routers; ++d )
        {
            os << (double)_crossbar_conflict_stalls[c][d] / time_delta << " ";
        }
        os << "];" << endl;
#endif
    }
}

void TrafficManager::UpdateStats() {
#if defined(TRACK_FLOWS) || defined(TRACK_STALLS)
    for(int c = 0; c < _classes; ++c)
    {
#ifdef TRACK_FLOWS
        {
            char trail_char = (c == _classes - 1) ? '\n' : ',';
            if(_injected_flits_out) *_injected_flits_out << _injected_flits[c] << trail_char;
            _injected_flits[c].assign(_nodes, 0);
            if(_ejected_flits_out) *_ejected_flits_out << _ejected_flits[c] << trail_char;
            _ejected_flits[c].assign(_nodes, 0);
        }
#endif
        for(int subnet = 0; subnet < _subnets; ++subnet)
        {
#ifdef TRACK_FLOWS
            if(_outstanding_credits_out) *_outstanding_credits_out << _outstanding_credits[c][subnet] << ',';
            if(_stored_flits_out) *_stored_flits_out << vector<int>(_nodes, 0) << ',';
#endif
            for(int router = 0; router < _routers; ++router)
            {
                Router * const r = _router[subnet][router];
#ifdef TRACK_FLOWS
                char trail_char =
                    ((router == _routers - 1) && (subnet == _subnets - 1) && (c == _classes - 1)) ? '\n' : ',';
                if(_received_flits_out) *_received_flits_out << r->GetReceivedFlits(c) << trail_char;
                if(_stored_flits_out) *_stored_flits_out << r->GetStoredFlits(c) << trail_char;
                if(_sent_flits_out) *_sent_flits_out << r->GetSentFlits(c) << trail_char;
                if(_outstanding_credits_out) *_outstanding_credits_out << r->GetOutstandingCredits(c) << trail_char;
                if(_active_packets_out) *_active_packets_out << r->GetActivePackets(c) << trail_char;
                r->ResetFlowStats(c);
#endif
#ifdef TRACK_STALLS
                _buffer_busy_stalls[c][subnet*_routers+router] += r->GetBufferBusyStalls(c);
                _buffer_conflict_stalls[c][subnet*_routers+router] += r->GetBufferConflictStalls(c);
                _buffer_full_stalls[c][subnet*_routers+router] += r->GetBufferFullStalls(c);
                _buffer_reserved_stalls[c][subnet*_routers+router] += r->GetBufferReservedStalls(c);
                _crossbar_conflict_stalls[c][subnet*_routers+router] += r->GetCrossbarConflictStalls(c);
                r->ResetStallStats(c);
#endif
            }
        }
    }
#ifdef TRACK_FLOWS
    if(_injected_flits_out) *_injected_flits_out << flush;
    if(_received_flits_out) *_received_flits_out << flush;
    if(_stored_flits_out) *_stored_flits_out << flush;
    if(_sent_flits_out) *_sent_flits_out << flush;
    if(_outstanding_credits_out) *_outstanding_credits_out << flush;
    if(_ejected_flits_out) *_ejected_flits_out << flush;
    if(_active_packets_out) *_active_packets_out << flush;
#endif
#endif

#ifdef TRACK_CREDITS
    for(int s = 0; s < _subnets; ++s)
    {
        for(int n = 0; n < _nodes; ++n)
        {
            BufferState const * const bs = _buf_states[n][s];
            for(int v = 0; v < _vcs; ++v)
            {
                if(_used_credits_out) *_used_credits_out << bs->OccupancyFor(v) << ',';
                if(_free_credits_out) *_free_credits_out << bs->AvailableFor(v) << ',';
                if(_max_credits_out) *_max_credits_out << bs->LimitFor(v) << ',';
            }
        }
        for(int r = 0; r < _routers; ++r)
        {
            Router const * const rtr = _router[s][r];
            char trail_char =
                ((r == _routers - 1) && (s == _subnets - 1)) ? '\n' : ',';
            if(_used_credits_out) *_used_credits_out << rtr->UsedCredits() << trail_char;
            if(_free_credits_out) *_free_credits_out << rtr->FreeCredits() << trail_char;
            if(_max_credits_out) *_max_credits_out << rtr->MaxCredits() << trail_char;
        }
    }
    if(_used_credits_out) *_used_credits_out << flush;
    if(_free_credits_out) *_free_credits_out << flush;
    if(_max_credits_out) *_max_credits_out << flush;
#endif

}

void TrafficManager::DisplayStats(ostream &os) const {

    for (int c = 0; c < _classes; ++c) {

        if (_measure_stats[c] == 0) {
            continue;
        }

        os << "Phase " << total_phases << " time " << _time << endl;
        os << "Class " << c << ":" << endl;

        os << "Minimum packet latency = " << _plat_stats[c]->Min() << endl
           << "Average packet latency = " << _plat_stats[c]->Average() << endl
           << "Maximum packet latency = " << _plat_stats[c]->Max() << endl
           << "Minimum network latency = " << _nlat_stats[c]->Min() << endl
           << "Average network latency = " << _nlat_stats[c]->Average() << endl
           << "Maximum network latency = " << _nlat_stats[c]->Max() << endl
           << "Slowest packet = " << _slowest_packet[c] << endl
           << "Minimum flit latency = " << _flat_stats[c]->Min() << endl
           << "Average flit latency = " << _flat_stats[c]->Average() << endl
           << "Maximum flit latency = " << _flat_stats[c]->Max() << endl
           << "Slowest flit = " << _slowest_flit[c] << endl
           << "Minimum fragmentation = " << _frag_stats[c]->Min() << endl
           << "Average fragmentation = " << _frag_stats[c]->Average() << endl
           << "Maximum fragmentation = " << _frag_stats[c]->Max() << endl;

        int count_sum, count_min, count_max;
        double rate_sum, rate_min, rate_max;
        double rate_avg;
        int sent_packets, sent_flits, accepted_packets, accepted_flits;
        int min_pos, max_pos;
        auto time_delta = (double) (_time - _reset_time);
        _ComputeStats(_sent_packets[c], &count_sum, &count_min, &count_max, &min_pos, &max_pos);
        rate_sum = (double) count_sum / time_delta;
        rate_min = (double) count_min / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        sent_packets = count_sum;
        os << "Minimum injected packet rate = " << rate_min
           << " (at node " << min_pos << ")" << endl
           << "Average injected packet rate = " << rate_avg << endl
           << "Maximum injected packet rate = " << rate_max
           << " (at node " << max_pos << ")" << endl;
        _ComputeStats(_accepted_packets[c], &count_sum, &count_min, &count_max, &min_pos, &max_pos);
        rate_sum = (double) count_sum / time_delta;
        rate_min = (double) count_min / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        accepted_packets = count_sum;
        os << "Minimum accepted packet rate = " << rate_min
           << " (at node " << min_pos << ")" << endl
           << "Average accepted packet rate = " << rate_avg << endl
           << "Maximum accepted packet rate = " << rate_max
           << " (at node " << max_pos << ")" << endl;
        _ComputeStats(_sent_flits[c], &count_sum, &count_min, &count_max, &min_pos, &max_pos);
        rate_sum = (double) count_sum / time_delta;
        rate_min = (double) count_min / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        sent_flits = count_sum;
        os << "Minimum injected flit rate = " << rate_min
           << " (at node " << min_pos << ")" << endl
           << "Average injected flit rate = " << rate_avg << endl
           << "Maximum injected flit rate = " << rate_max
           << " (at node " << max_pos << ")" << endl;
        _ComputeStats(_accepted_flits[c], &count_sum, &count_min, &count_max, &min_pos, &max_pos);
        rate_sum = (double) count_sum / time_delta;
        rate_min = (double) count_min / time_delta;
        rate_max = (double) count_max / time_delta;
        rate_avg = rate_sum / (double) _nodes;
        accepted_flits = count_sum;
        os << "Minimum accepted flit rate = " << rate_min
           << " (at node " << min_pos << ")" << endl
           << "Average accepted flit rate = " << rate_avg << endl
           << "Maximum accepted flit rate = " << rate_max
           << " (at node " << max_pos << ")" << endl;

        os << "Average injected packet length = " << (double) sent_flits / (double) sent_packets << endl
           << "Average accepted packet length = " << (double) accepted_flits / (double) accepted_packets << endl;

        os << "Total in-flight flits = " << _total_in_flight_flits[c].size()
           << " (" << _measured_in_flight_flits[c].size() << " measured)"
           << endl;

#ifdef TRACK_STALLS
        _ComputeStats(_buffer_busy_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        os << "Buffer busy stall rate = " << rate_avg << endl;
        _ComputeStats(_buffer_conflict_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        os << "Buffer conflict stall rate = " << rate_avg << endl;
        _ComputeStats(_buffer_full_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        os << "Buffer full stall rate = " << rate_avg << endl;
        _ComputeStats(_buffer_reserved_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        os << "Buffer reserved stall rate = " << rate_avg << endl;
        _ComputeStats(_crossbar_conflict_stalls[c], &count_sum);
        rate_sum = (double)count_sum / time_delta;
        rate_avg = rate_sum / (double)(_subnets*_routers);
        os << "Crossbar conflict stall rate = " << rate_avg << endl;
#endif
    }
}

void TrafficManager::DisplayOverallStats(ostream &os) const {

    for (int c = 0; c < _classes; ++c) {

        if (_measure_stats[c] == 0) {
            continue;
        }

        os << "====== Traffic class " << c << " ======" << endl;

        os << "Overall minimum packet latency = " << _overall_min_plat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average packet latency = " << _overall_avg_plat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum packet latency = " << _overall_max_plat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum network latency = " << _overall_min_nlat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average network latency = " << _overall_avg_nlat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum network latency = " << _overall_max_nlat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum flit latency = " << _overall_min_flat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average flit latency = " << _overall_avg_flat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum flit latency = " << _overall_max_flat[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum fragmentation = " << _overall_min_frag[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average fragmentation = " << _overall_avg_frag[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum fragmentation = " << _overall_max_frag[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum injected packet rate = " << _overall_min_sent_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average injected packet rate = " << _overall_avg_sent_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum injected packet rate = " << _overall_max_sent_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum accepted packet rate = " << _overall_min_accepted_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average accepted packet rate = " << _overall_avg_accepted_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum accepted packet rate = " << _overall_max_accepted_packets[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum injected flit rate = " << _overall_min_sent[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average injected flit rate = " << _overall_avg_sent[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum injected flit rate = " << _overall_max_sent[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall minimum accepted flit rate = " << _overall_min_accepted[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall average accepted flit rate = " << _overall_avg_accepted[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;
        os << "Overall maximum accepted flit rate = " << _overall_max_accepted[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall average injected packet size = " << _overall_avg_sent[c] / _overall_avg_sent_packets[c]
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall average accepted packet size = " << _overall_avg_accepted[c] / _overall_avg_accepted_packets[c]
           << " (" << _total_sims << " samples)" << endl;

        os << "Overall average hops = " << _overall_hop_stats[c] / (double) _total_sims
           << " (" << _total_sims << " samples)" << endl;

#ifdef TRACK_STALLS
        os << "Overall buffer busy stall rate = " << (double)_overall_buffer_busy_stalls[c] / (double)_total_sims
           << " (" << _total_sims << " samples)" << endl
           << "Overall buffer conflict stall rate = " << (double)_overall_buffer_conflict_stalls[c] / (double)_total_sims
           << " (" << _total_sims << " samples)" << endl
           << "Overall buffer full stall rate = " << (double)_overall_buffer_full_stalls[c] / (double)_total_sims
           << " (" << _total_sims << " samples)" << endl
           << "Overall buffer reserved stall rate = " << (double)_overall_buffer_reserved_stalls[c] / (double)_total_sims
           << " (" << _total_sims << " samples)" << endl
           << "Overall crossbar conflict stall rate = " << (double)_overall_crossbar_conflict_stalls[c] / (double)_total_sims
           << " (" << _total_sims << " samples)" << endl;
#endif

    }

}

string TrafficManager::_OverallStatsCSV(int c) const {
    ostringstream os;
    os << _traffic[c]
       << ',' << _use_read_write[c]
       << ',' << _load[c]
       << ',' << _overall_min_plat[c] / (double) _total_sims
       << ',' << _overall_avg_plat[c] / (double) _total_sims
       << ',' << _overall_max_plat[c] / (double) _total_sims
       << ',' << _overall_min_nlat[c] / (double) _total_sims
       << ',' << _overall_avg_nlat[c] / (double) _total_sims
       << ',' << _overall_max_nlat[c] / (double) _total_sims
       << ',' << _overall_min_flat[c] / (double) _total_sims
       << ',' << _overall_avg_flat[c] / (double) _total_sims
       << ',' << _overall_max_flat[c] / (double) _total_sims
       << ',' << _overall_min_frag[c] / (double) _total_sims
       << ',' << _overall_avg_frag[c] / (double) _total_sims
       << ',' << _overall_max_frag[c] / (double) _total_sims
       << ',' << _overall_min_sent_packets[c] / (double) _total_sims
       << ',' << _overall_avg_sent_packets[c] / (double) _total_sims
       << ',' << _overall_max_sent_packets[c] / (double) _total_sims
       << ',' << _overall_min_accepted_packets[c] / (double) _total_sims
       << ',' << _overall_avg_accepted_packets[c] / (double) _total_sims
       << ',' << _overall_max_accepted_packets[c] / (double) _total_sims
       << ',' << _overall_min_sent[c] / (double) _total_sims
       << ',' << _overall_avg_sent[c] / (double) _total_sims
       << ',' << _overall_max_sent[c] / (double) _total_sims
       << ',' << _overall_min_accepted[c] / (double) _total_sims
       << ',' << _overall_avg_accepted[c] / (double) _total_sims
       << ',' << _overall_max_accepted[c] / (double) _total_sims
       << ',' << _overall_avg_sent[c] / _overall_avg_sent_packets[c]
       << ',' << _overall_avg_accepted[c] / _overall_avg_accepted_packets[c]
       << ',' << _overall_hop_stats[c] / (double) _total_sims;

#ifdef TRACK_STALLS
    os << ',' << (double)_overall_buffer_busy_stalls[c] / (double)_total_sims
       << ',' << (double)_overall_buffer_conflict_stalls[c] / (double)_total_sims
       << ',' << (double)_overall_buffer_full_stalls[c] / (double)_total_sims
       << ',' << (double)_overall_buffer_reserved_stalls[c] / (double)_total_sims
       << ',' << (double)_overall_crossbar_conflict_stalls[c] / (double)_total_sims;
#endif

    return os.str();
}

void TrafficManager::DisplayOverallStatsCSV(ostream &os) const {
    for (int c = 0; c < _classes; ++c) {
        os << "results:" << c << ',' << _OverallStatsCSV() << endl;
    }
}

//read the watchlist
void TrafficManager::_LoadWatchList(const string &filename) {
    ifstream watch_list;
    watch_list.open(filename.c_str());

    string line;
    if (watch_list.is_open()) {
        while (!watch_list.eof()) {
            getline(watch_list, line);
            if (line != "") {
                if (line[0] == 'p') {
                    _packets_to_watch.insert(atoi(line.c_str() + 1));
                } else {
                    _flits_to_watch.insert(atoi(line.c_str()));
                }
            }
        }

    } else {
        Error("Unable to open flit watch file: " + filename);
    }
}

int TrafficManager::_GetNextPacketSize(int cl) const {
    assert(cl >= 0 && cl < _classes);

    vector<int> const &psize = _packet_size[cl];
    int sizes = psize.size();

    if (sizes == 1) {
        return psize[0];
    }

    vector<int> const &prate = _packet_size_rate[cl];
    int max_val = _packet_size_max_val[cl];

    int pct = RandomInt(max_val);

    for (int i = 0; i < (sizes - 1); ++i) {
        int const limit = prate[i];
        if (limit > pct) {
            return psize[i];
        } else {
            pct -= limit;
        }
    }
    assert(prate.back() > pct);
    return psize.back();
}

double TrafficManager::_GetAveragePacketSize(int cl) const {
    vector<int> const &psize = _packet_size[cl];
    int sizes = psize.size();
    if (sizes == 1) {
        return (double) psize[0];
    }
    vector<int> const &prate = _packet_size_rate[cl];
    int sum = 0;
    for (int i = 0; i < sizes; ++i) {
        sum += psize[i] * prate[i];
    }
    return (double) sum / (double) (_packet_size_max_val[cl] + 1);
}
