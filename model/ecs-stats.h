/// \file ecs-stats.h
/// \author Marshall Asch <masch@uoguelph.ca>
///
/// Copyright (c) 2021 by Marshall Asch <masch@uoguelph.ca>
/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted, provided that the above
/// copyright notice and this permission notice appear in all copies.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
/// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
///
#ifndef __ECS_STATS_H
#define __ECS_STATS_H

#define TYPE_ENUM_SIZE 8

#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "ns3/uinteger.h"

struct CH_Event {
    std::string node_status;
    uint32_t node_address;
    double event_time;
    std::string event;
};
struct Open_Claim {
    uint32_t node_address;
    double event_time;
};

struct Member_Event {
    std::string node_status;
    uint32_t node_address;
    double event_time;
    std::string event;
    uint32_t ch_address;
};
struct Open_Membership {
    uint32_t node_address;
    double event_time;
    uint32_t ch_address;
};

namespace ecs {
using namespace ns3;

class Stats {
    public:
        enum class Type {
        UNKOWN = 0,
        PING,
        MODE_CHANGE,
        ELECTION_REQUEST,
        STORE,
        LOOKUP,
        LOOKUP_RESPONSE,
        TRANSFER
        };

        Stats();
        ~Stats();
        void Reset();

        void incPing();
        void incClaim();
        void incStatus();
        void incMeeting();
        void incResign();
        
        void PrintMessageTotals();

        void IncreaseClusteringMessages();
        void IncreaseClusterChangeMessages();
        void IncreaseCHCount();
        void DecreaseCHCount();
        void IncreaseCMemCount();
        void IncreaseGateCount();
        void IncreaseGuestCount();
        void IncreaseAverageClusterHeadCount();
        void IncreaseClusterSizeCount(uint64_t cluster_size);
        void IncreaseGateCoverageCount(uint64_t num_heads_covering);
        void IncreaseAccessPointCount(uint64_t num_access_points);
        double CalculateAverageClusterSize(double runtime);

        void WriteFinalStats(double runtime, uint16_t num_nodes, double node_speed, uint32_t seed);

        void PrintClusterAverage(uint32_t seed, double node_speed, uint16_t num_nodes);
        void PrintCHEvents();
        void PrintMembershipEvents();

        void OutputCHEventsToCSV(int sim_number);
        void OutputMembershipToCSV(int sim_number);

        void recordCHClaim(uint32_t node_address, double event_time);
        void recordCHRecieveStatus(uint32_t node_address, double event_time);
        void recordCHResign(uint32_t node_address, double event_time);
        void recordMembershipStart(std::string node_status, uint32_t node_address, double event_time, uint32_t ch_address);
        void recordMembershipEnd(std::string node_status, uint32_t node_address, double event_time, uint32_t ch_address);
        void recordMemberBecomeStandalone(uint32_t node_address, double event_time);

        double CalculateCHLifetime();
        double CalculateMembershipLifetime();

};
}; //namespace ecs

#endif