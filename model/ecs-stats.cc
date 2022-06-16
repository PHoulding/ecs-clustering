/// \file rhpman-stats.cc
/// \author Marshall Asch <masch@uoguelph.ca>
/// \brief The class that is used to collect the statistics from the rhpman scheme.
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
#include "ecs-stats.h"

// static uint64_t saves;
// static uint64_t lookups;
// static uint64_t lookupSuccess;
// static uint64_t lookupFailed;
static std::list<CH_Event> CH_Event_List;
static std::list<Member_Event> Membership_List;
static uint64_t numClusterHeads;
static uint64_t numClusterMembers;
static uint64_t numClusterGateways;
static uint64_t numClusterGuests;
static uint64_t numClusterSize;
static uint64_t numHeadsCoveringGates;
static uint64_t numAccessPoints;
static uint64_t numClusteringMessages;
static uint64_t numClusterChangeMessages;


namespace ecs {
using namespace ns3;

Stats::Stats() {
  static bool alreadyInitalized = false;

  if (!alreadyInitalized) {
    alreadyInitalized = true;
    Reset();
  }
}

Stats::~Stats() {}

void Stats::Reset() {
  CH_Event_List.clear();
  Membership_List.clear();
  numClusterHeads = 0;
  numClusterMembers = 0;
  numClusterGateways = 0;
  numClusterGuests = 0;
  numClusterSize = 0;
  numHeadsCoveringGates = 0;
  numAccessPoints = 0;
  numClusterChangeMessages = 0;
  numClusteringMessages = 0;
}

void Stats::IncreaseClusterChangeMessages() {
  numClusterChangeMessages++;
}

void Stats::IncreaseClusteringMessages() {
  numClusteringMessages++;
}

void Stats::IncreaseClusterSizeCount(uint64_t cluster_size) {
  numClusterSize += cluster_size;
}
void Stats::IncreaseCHCount() {
  numClusterHeads++;
}
void Stats::DecreaseCHCount() {
  numClusterHeads--;
}
void Stats::IncreaseCMemCount() {
  numClusterMembers++;
}
void Stats::IncreaseGateCount() {
  numClusterGateways++;
}
void Stats::IncreaseGuestCount() {
  numClusterGuests++;
}
void Stats::IncreaseGateCoverageCount(uint64_t num_heads_covering) {
  numHeadsCoveringGates+=num_heads_covering;
}
void Stats::IncreaseAccessPointCount(uint64_t num_access_points) {
  numAccessPoints+=num_access_points;
}

double Stats::CalculateAverageClusterSize(double runtime) {
  // a = #clusters (cluster heads), b = #cluster members
  //double a_b = (numClusterHeads/(runtime/60) + numClusterMembers/(runtime/60));
  double a_b = numClusterHeads + numClusterMembers;
  // ni = # clusterheads that cover a cluster gateway
  //double n_i = numHeadsCoveringGates/(runtime/60);
  double n_i = numHeadsCoveringGates;
  // mj = # access points that a guest is connected to
  //double m_j = numAccessPoints/(runtime/60);
  double m_j = numAccessPoints;
  double numerator = a_b + n_i + m_j;
  //double avgClSize = numerator/(numClusterHeads/(runtime/60));
  double avgClSize = numerator/(numClusterHeads);
  return avgClSize;
}

void Stats::PrintClusterAverage(double runtime) {
  std::cout << "Number of clusters at finish: " << numClusterHeads << " runtime: " << runtime << "\n";
  std::cout << "Average Number of Clusters: " << numClusterHeads/(runtime/60) << "\n";
  std::cout << "Average Cluster Size (table): " << numClusterSize/(runtime/60) << "\n";
  std::cout << "Average Cluster Members: " << numClusterMembers/(runtime/60) << "\n";
  std::cout << "Average Cluster Gateways: " << numClusterGateways/(runtime/60) << "\n";
  std::cout << "Average Cluster Guests: " << numClusterGuests/(runtime/60) << "\n";
  double avgClSizeFormula = CalculateAverageClusterSize(runtime);
  std::cout << "Average Cluster Size (formula): " << avgClSizeFormula << "\n";
  std::cout << "Number of Cluster change messages: " << numClusterChangeMessages << "\n";
  std::cout << "Total clustering messages: " << numClusteringMessages << "\n";
}

void Stats::PrintCHEvents() {
  std::cout << "Printing CH Events for list of size " << CH_Event_List.size() << "\n";
  std::list<CH_Event>::iterator it;
  for (it = CH_Event_List.begin(); it != CH_Event_List.end(); it++) {
    std::cout << it->node_status << " " << it->node_address << " " << it->event_time << " " << it->event << "\n";
  }
}
void Stats::PrintMembershipEvents() {
  std::cout << "Printing CH Events for list of size " << Membership_List.size() << "\n";
  std::list<Member_Event>::iterator it;
  for (it = Membership_List.begin(); it != Membership_List.end(); it++) {
    std::cout << it->node_status << " " << it->node_address << " " << it->event_time << " " << it->event << " " << it->ch_address << "\n";
  }
}

void Stats::OutputCHEventsToCSV(int sim_number) {
  std::ofstream file;
  std::string filename = "CHEvents_" + std::to_string(sim_number) + ".csv";
  file.open(filename);
  std::list<CH_Event>::iterator it;
  for (it = CH_Event_List.begin(); it != CH_Event_List.end(); it++) {
    file << it->node_status << "," << it->node_address << "," << it->event_time << "," << it->event << "\n";
  }
  file.close();
}
void Stats::OutputMembershipToCSV(int sim_number) {
  std::ofstream file;
  std::string filename = "MembershipEvents_" + std::to_string(sim_number) + ".csv";
  file.open(filename);
  std::list<Member_Event>::iterator it;
  for (it = Membership_List.begin(); it != Membership_List.end(); it++) {
    file << it->node_status << "," << it->node_address << "," << it->event_time << "," << it->event << "," << it->ch_address << "\n";
  }
  file.close();
}


void Stats::recordCHClaim(uint32_t node_address, double event_time) {
  CH_Event event = {"CH", node_address, event_time, "CH_Claim"};
  CH_Event_List.push_back(event);
  //IncreaseCHCount();
}
// Adds a CH reieve status event to the event list
void Stats::recordCHRecieveStatus(uint32_t node_address, double event_time) {
  CH_Event event = {"CH", node_address, event_time, "Recieve_Status"};
  CH_Event_List.push_back(event);
}
void Stats::recordCHResign(uint32_t node_address, double event_time) {
  CH_Event event = {"CH", node_address, event_time, "Resign"};
  CH_Event_List.push_back(event);
  //DecreaseCHCount();
}

void Stats::recordMembershipStart(std::string node_status, uint32_t node_address, double event_time, uint32_t ch_address) {
  Member_Event event = {node_status, node_address, event_time, "Join Cluster", ch_address};
  Membership_List.push_back(event);
}
void Stats::recordMembershipEnd(std::string node_status, uint32_t node_address, double event_time, uint32_t ch_address) {
  Member_Event event = {node_status, node_address, event_time, "Leave Cluster", ch_address};
  Membership_List.push_back(event);
}
//Should be deleted? Members don't become standalone, only cluster heads
void Stats::recordMemberBecomeStandalone(uint32_t node_address, double event_time) {
  Member_Event event = {"Standalone", node_address, event_time, "Becomes Standalone", 0};
  Membership_List.push_back(event);
}

};