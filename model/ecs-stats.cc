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

static uint64_t pings;
static uint64_t claims;
static uint64_t statuses;
static uint64_t meetings;
static uint64_t resigns;

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
  pings = 0;
  claims = 0;
  statuses = 0;
  meetings = 0;
  resigns = 0;
}

void Stats::incPing() { pings++; }
void Stats::incClaim() { claims++; }
void Stats::incStatus() { statuses++; }
void Stats::incMeeting() { meetings++; }
void Stats::incResign() { resigns++; }

void Stats::PrintMessageTotals() {
  std::cout << "Pings:\t" << pings << "\n";
  std::cout << "Claims:\t" << claims << "\n";
  std::cout << "Statuses:\t" << statuses << "\n";
  std::cout << "Meetings:\t" << meetings << "\n";
  std::cout << "resigns:\t" << resigns << "\n";
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
  double a_b = numClusterHeads + numClusterMembers;
  // ni = # clusterheads that cover a cluster gateway
  double n_i = numHeadsCoveringGates;
  // mj = # access points that a guest is connected to
  double m_j = numAccessPoints;
  double numerator = a_b + n_i + m_j;
  double avgClSize = numerator/numClusterHeads;
  return avgClSize;
}

void Stats::WriteFinalStats(double runtime, uint16_t num_nodes, double node_speed, uint32_t seed) {
  double avgClusterSizeTable = numClusterSize/(runtime/60);
  double avgClusterSizeFormula = CalculateAverageClusterSize(runtime);
  double avgClusterHeads = numClusterHeads/(runtime/60);
  double avgMembers = numClusterMembers/(runtime/60);
  double avgGates = numClusterGateways/(runtime/60);
  double avgGuests = numClusterGuests/(runtime/60);
  uint32_t totalClusterChangeMessages = numClusterChangeMessages;
  uint32_t totalClusterMessages = numClusteringMessages;
  double avgCHLifetime = CalculateCHLifetime();
  double avgMemberLifetime = CalculateMembershipLifetime();
  // Output file cols: (row # in file = sim number)
  //    #nodes, node_speed, avgClusterSizeTable, avgClusterSizeFormula, avgClusterHeads, avgMembers, avgGates, avgGuests, totalClusterChangeMessages, totalClusterMessages, avgCHLifetime, avgMemberLifetime

  std::ofstream file;
  std::string filename = "FinalStats.csv";
  file.open(filename, std::ios::app);
  file << seed << "," << num_nodes << "," << node_speed << "," << avgClusterSizeTable << "," << avgClusterSizeFormula << "," << avgClusterHeads << "," << avgMembers << "," \
    << avgGates << "," << avgGuests << "," << totalClusterChangeMessages << "," << totalClusterMessages << "," << avgCHLifetime << "," << avgMemberLifetime << "\n";
  file.close();
}

void Stats::PrintClusterAverage(uint32_t seed, double node_speed, uint16_t num_nodes) {
  double avgClSizeFormula = CalculateAverageClusterSize(600);
  std::cout << "Seed\t" << unsigned(seed) << "\n";
  std::cout << "#Nodes\t" << unsigned(num_nodes) << "\n";
  std::cout << "Node_Speed\t" << node_speed << "\n";

  std::cout << "Table_CL_Size\t" << numClusterSize/10 << "\n";
  std::cout << "Formula_CL_Size\t" << avgClSizeFormula << "\n";
  std::cout << "Avg_Heads\t" << numClusterHeads/10 << "\n";
  std::cout << "Avg_Mems\t" << numClusterMembers/10 << "\n";
  std::cout << "Avg_Gates\t" << numClusterGateways/10 << "\n";
  std::cout << "Avg_Guests\t" << numClusterGuests/10 << "\n";
  std::cout << "TotalClChangeMessages\t" << numClusterChangeMessages << "\n";
  std::cout << "TotalClusteringMessages\t" << numClusteringMessages << "\n";
  double avgCHLifetime = CalculateCHLifetime();
  double avgMemberLifetime = CalculateMembershipLifetime();
  std::cout << "Avg_CH_Lifetime\t" << avgCHLifetime << "\n";
  std::cout << "Avg_Membership_Lifetime\t" << avgMemberLifetime << "\n";

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

double Stats::CalculateCHLifetime() {
  double avg_lifetime = 0;
  uint32_t num_matches = 0;
  std::list<Open_Claim> open_claims;
  std::list<CH_Event>::iterator it;
  for(it = CH_Event_List.begin(); it != CH_Event_List.end(); it++) {
    if(it->event=="CH_Claim") {
      Open_Claim claim = {it->node_address, it->event_time};
      open_claims.push_back(claim);
    } else if(it->event == "Resign") {
      std::list<Open_Claim>::iterator it2;
      for(it2 = open_claims.begin(); it2 != open_claims.end(); it2++) {
        if(it2->node_address == it->node_address) {
          avg_lifetime += it->event_time - it2->event_time;
          num_matches += 1;
          open_claims.erase(it2++);
        }
      }
    }
  }
  //std::cout << "Average CH Lifetime w/o living nodes: " << avg_lifetime/num_matches << "\n";
  std::list<Open_Claim>::iterator it3;
  for(it3 = open_claims.begin(); it3 != open_claims.end(); it3++) {
    avg_lifetime += 600 - it3->event_time;
    num_matches += 1;
  }  
  //std::cout << "Avg CH Lifetime: " << avg_lifetime/num_matches << "\n";
  return avg_lifetime/num_matches;
}

double Stats::CalculateMembershipLifetime() {
  double avg_lifetime = 0;
  uint32_t num_matches = 0;
  std::list<Open_Membership> open_memberships;
  std::list<Member_Event>::iterator it;
  for(it = Membership_List.begin(); it != Membership_List.end(); it++) {
    std::list<Open_Membership>::iterator it2;
    if(it->event == "Join Cluster") {
      Open_Membership membership = {it->node_address, it->event_time, it->ch_address};
      open_memberships.push_back(membership);
    } else if(it->event == "Leave Cluster") {
      for(it2 = open_memberships.begin(); it2 != open_memberships.end(); it2++) {
        if(it2->node_address == it->node_address && it2->ch_address == it->ch_address) {
          avg_lifetime += it->event_time - it2->event_time;
          num_matches += 1;
          open_memberships.erase(it2++);
        }
      }
    } else if(it->event == "I Resign") {
      // When a CH Resigns, remove all current members
      for(it2 = open_memberships.begin(); it2 != open_memberships.end(); it2++) {
        if(it2->ch_address == it->node_address) {
          avg_lifetime += it->event_time - it2->event_time;
          num_matches += 1;
          open_memberships.erase(it2++);
        }
      }
    }
  }
  //std::cout << "Avg membership lifetime w/o living nodes: " << avg_lifetime/num_matches << "\n";
  // After previous logic, only members in open_memberships are those that exist at the end of
  // the simulation, thus take the difference between the stop time & when they became nodes
  std::list<Open_Membership>::iterator it3;
  for(it3 = open_memberships.begin(); it3 != open_memberships.end(); it3++) {
    avg_lifetime += 600 - it3->event_time;
    num_matches += 1;
  }
  //std::cout << "Avg membership lifetime: " << avg_lifetime/num_matches << "\n";
  return avg_lifetime/num_matches;
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
  Member_Event event2 = {"CH", node_address, event_time, "I Resign", node_address};
  Membership_List.push_back(event2);
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