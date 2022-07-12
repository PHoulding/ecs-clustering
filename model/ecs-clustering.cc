/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/**
Random thoughts:
  - Defining access points? is this something I need to do or does the Simulator already just do it?
  - Sending pings & messages, do they need to be scheduled for each time? Something to look into.
  - HandleClaim, would a clusterhead ever get this message (i.e. a node is trying to claim CH while it is one?)
  - HandleResponse, delete???
  - CH meeting with two CHs of equal degree was never discussed in paper. This will need to be noted as an assumption.
**/

#include <algorithm>
#include <map>
#include <iostream>
#include <string>

#include <cfloat>
#include <limits>

#include "ns3/application-container.h"
#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/attribute.h"
//#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/node-container.h"
#include "ns3/object-base.h"
#include "ns3/object-factory.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/pointer.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/random-variable-stream.h"

#include "logging.h"
#include "nsutil.h"
#include "util.h"
#include "ecs-clustering.h"

#include "proto/messages.pb.h"
#include <google/protobuf/text_format.h>

namespace ecs {

using namespace ns3;

static Ptr<Packet> GeneratePacket(const ecs::packets::Message message);
static ecs::packets::Message ParsePacket(const Ptr<Packet> packet);

NS_OBJECT_ENSURE_REGISTERED(ecsClusterApp);

TypeId ecsClusterApp::GetTypeId() {
  static TypeId id = TypeId("ecs-clustering:EcsClusterApp")
    .SetParent<Application>()
    .SetGroupName("Applications")
    .AddConstructor<ecsClusterApp>()
    .AddAttribute(
      "NeighborhoodSize",
      "Number of hops considered to be in the neightborhood of this node (h)",
      UintegerValue(1),
      MakeUintegerAccessor(&ecsClusterApp::m_neighborhoodHops),
      MakeUintegerChecker<uint32_t>(1))
    .AddAttribute(
      "StandoffTime",
      "Maximum amount of time for nodes to claim cluster head. They can activate beforehand (so_t)",
      TimeValue(3.0_sec),
      MakeTimeAccessor(&ecsClusterApp::m_standoff_time),
      MakeTimeChecker(0.1_sec))
    .AddAttribute(
      "ProfileUpdateDelay",
      "Time to wait between profile update and exchange (T)",
      TimeValue(6.0_sec),
      MakeTimeAccessor(&ecsClusterApp::m_profileDelay),
      MakeTimeChecker(0.1_sec))
    .AddAttribute(
      "WaitTime",
      "The time waited before a coming alive",
      TimeValue(30.0_sec),
      MakeTimeAccessor(&ecsClusterApp::m_waitTime),
      MakeTimeChecker(0.1_sec));
  return id;
}

//override
void ecsClusterApp::StartApplication() {
  if(m_state == State::RUNNING) {
    NS_LOG_DEBUG("Ignoring ecsClusterApp::StartApplication request on already started application");
    return;
  }
  m_state = State::NOT_STARTED;

  if(m_socket_recv == 0) {
    m_socket_recv = SetupSocket(APPLICATION_PORT, 0); //not implemented
  }
  if(m_neighborhood_socket == 0) {
    m_neighborhood_socket = SetupSocket(APPLICATION_PORT, m_neighborhoodHops);
  }

  m_address = GetID();
  m_peerTable = Table(m_profileDelay.GetSeconds(), m_neighborhoodHops);
  m_state = State::RUNNING;
  m_node_status = Node_Status::UNSPECIFIED;
  m_hello_message_timeout = 1.0_sec;
  m_table_scan_timeout = 0.1_sec;
  m_valid_entry_timeout = 2.3_sec;

  ScheduleWakeup();
  Simulator::Schedule(57.0_sec, &ecsClusterApp::ScheduleAverageRecording, this);
}
//override
void ecsClusterApp::StopApplication() {
  if(m_state == State::NOT_STARTED) {
    NS_LOG_ERROR("Called ecsClusterApp::StopApplication on a NOT_STARTED instance");
    return;
  }
  if(m_state == State::STOPPED) {
    NS_LOG_DEBUG("Ignoring ecsClusterApp::StopApplication on already stopped instance");
    return;
  }

  if(m_socket_recv != 0) {
    DestroySocket(m_socket_recv);
    m_socket_recv = 0;
  }
  if(m_neighborhood_socket != 0) {
    DestroySocket(m_neighborhood_socket);
    m_neighborhood_socket = 0;
  }
  if(m_election_socket != 0) {
    DestroySocket(m_election_socket);
    m_election_socket = 0;
  }
  m_state = State::STOPPED;
  //TODO: Cancel events
  
  m_ping_event.Cancel();
  m_table_update_event.Cancel();
  m_CH_claim_event.Cancel();
  m_hello_event.Cancel();
  m_table_scan_event.Cancel();
  m_check_CHResign_event.Cancel();
  m_print_table_event.Cancel();
}

ecsClusterApp::Node_Status ecsClusterApp::GetStatus() const { return m_node_status; }
ecsClusterApp::State ecsClusterApp::GetState() const { return m_state; }

void ecsClusterApp::SetStatus(ecsClusterApp::Node_Status status) {
  m_node_status = status;
}

// this will get the nodes IPv4 address and return it as a 32 bit integer
uint32_t ecsClusterApp::GetID() {
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
  Ipv4Address ipAddr = iaddr.GetLocal();

  uint32_t addr = ipAddr.Get();
  return addr;
}

/**
Socket setup functions
**/
Ptr<Socket> ecsClusterApp::SetupRcvSocket(uint16_t port) {
  Ptr<Socket> socket;
  socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  if(socket->Bind(local) == -1) {
    NS_FATAL_ERROR("Failed to bind socket");
  }

  socket->SetRecvCallback(MakeCallback(&ecsClusterApp::HandleRequest, this));
  return socket;
}
Ptr<Socket> ecsClusterApp::SetupSendSocket(uint16_t port, uint8_t ttl) {
  Ptr<Socket> socket;
  socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

  socket->Connect(InetSocketAddress(Ipv4Address::GetBroadcast(), port));
  socket->SetAllowBroadcast(true);
  socket->SetIpTtl(ttl);

  socket->SetRecvCallback(MakeCallback(&ecsClusterApp::HandleRequest, this));
  return socket;
}

Ptr<Socket> ecsClusterApp::SetupSocket(uint16_t port, uint32_t ttl) {
  return ttl == 0 ? SetupRcvSocket(port) : SetupSendSocket(port, ttl);
}

void ecsClusterApp::DestroySocket(Ptr<Socket> socket) {
  socket->Close();
  socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
}

/**
Send message wrappers
**/
void ecsClusterApp::SendToNodes(Ptr<Packet> message, const std::set<uint32_t> nodes) {
  for(std::set<uint32_t>::iterator it = nodes.begin(); it!=nodes.end(); ++it) {
    SendMessage(Ipv4Address(*it), message);
  }
}

/**
Generate messages to be sent
**/
Ptr<Packet> ecsClusterApp::GeneratePing(uint8_t node_status) {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());
  message.set_node_status(node_status);

  message.mutable_ping();
  return GeneratePacket(message);
}

Ptr<Packet> ecsClusterApp::GenerateStatus(uint8_t node_status) {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());
  message.set_node_status(node_status);

  message.mutable_status();
  return GeneratePacket(message);
}

Ptr<Packet> ecsClusterApp::GenerateClusterHeadClaim() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  message.mutable_claim();

  return GeneratePacket(message);
}
Ptr<Packet> ecsClusterApp::GenerateMeeting() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  ecs::packets::Meeting * meeting = message.mutable_meeting();
  meeting->set_tablesize(m_informationTable.size());

  return GeneratePacket(message);
}
Ptr<Packet> ecsClusterApp::GenerateResign(uint8_t node_status) {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());
  message.set_node_status(node_status);

  message.mutable_resign();

  return GeneratePacket(message);

}

/**
Generate packet to be sent around
**/
static Ptr<Packet> GeneratePacket(const ecs::packets::Message message) {
  uint32_t size = message.ByteSizeLong();
  uint8_t* payload = new uint8_t[size];

  if(!message.SerializeToArray(payload, size)) {
    NS_LOG_ERROR("Failed to serialize the message for transmission");
  }
  Ptr<Packet> packet = Create<Packet>(payload, size);
  delete[] payload;
  return packet;
}


static ecs::packets::Message ParsePacket(const Ptr<Packet> packet) {
  uint32_t size = packet->GetSize();
  uint8_t* payload = new uint8_t[size];
  packet->CopyData(payload, size);

  ecs::packets::Message message;
  message.ParseFromArray(payload, size);
  delete[] payload;

  return message;
}

/**
Marshall calls this the "Actually send messages" section   :)
**/
void ecsClusterApp::BroadcastToNeighbors(Ptr<Packet> packet) {
  m_neighborhood_socket->Send(packet);
}
void ecsClusterApp::SendMessage(Ipv4Address dest, Ptr<Packet> packet) {
  m_socket_recv->SendTo(packet, 0, InetSocketAddress(dest, APPLICATION_PORT));
}

/**
Marshall calls this section the "send messages" section   :)
I imagine this is just functions that call the helper functions in the other sending section
**/
void ecsClusterApp::SendPing(uint8_t node_status) {
  Ptr<Packet> message = GeneratePing(node_status);
  BroadcastToNeighbors(message);
  stats.incPing();
}

void ecsClusterApp::SendClusterHeadClaim() {
  SetStatus(Node_Status::CLUSTER_HEAD);
  Ptr<Packet> message = GenerateClusterHeadClaim();
  BroadcastToNeighbors(message);
  m_CH_Claim_flag = true;
  stats.recordCHClaim(m_address, Simulator::Now().GetSeconds());
  stats.incClaim();
}

void ecsClusterApp::SendStatus(uint32_t nodeID) {
  Ptr<Packet> message = GenerateStatus(GenerateNodeStatusToUint());
  SendMessage(Ipv4Address(nodeID), message);
  stats.incStatus();
}
void ecsClusterApp::SendCHMeeting(uint32_t nodeID) {
  Ptr<Packet> message = GenerateMeeting();
  SendMessage(Ipv4Address(nodeID), message);
  NS_LOG_UNCOND("CH Meeting Sent!");
  stats.incMeeting();
}
void ecsClusterApp::SendResign(uint8_t node_status) {
  Ptr<Packet> message = GenerateResign(node_status);
  BroadcastToNeighbors(message);
  if (m_CH_Claim_flag) {
    stats.recordCHResign(m_address, Simulator::Now().GetSeconds());
    m_CH_Claim_flag = false;
    stats.incResign();
  }
}

/**
Event schedulers
**/
void ecsClusterApp::SchedulePing() {
  if(m_state != State::RUNNING) return;
  SendPing(GenerateNodeStatusToUint());
  m_ping_event = Simulator::Schedule(m_profileDelay, &ecsClusterApp::SchedulePing, this);
}

void ecsClusterApp::ScheduleWakeup() {
  if(m_state != State::RUNNING) return;
  if(m_node_status != Node_Status::UNSPECIFIED) return;

  Ptr<UniformRandomVariable> standoff = CreateObject<UniformRandomVariable> ();
  standoff->SetAttribute("Min", DoubleValue(m_waitTime.GetSeconds()));
  standoff->SetAttribute("Max", DoubleValue(m_standoff_time.GetSeconds()));
  random_m_standoff_time = ns3::Time::FromDouble(standoff->GetValue(), ns3::Time::Unit::S);
  m_CH_claim_event = Simulator::Schedule(random_m_standoff_time, &ecsClusterApp::SendClusterHeadClaim, this);

  //Schedule periodic events
  m_hello_event = Simulator::Schedule(random_m_standoff_time+m_hello_message_timeout, &ecsClusterApp::ScheduleHello, this);
  m_table_scan_event = Simulator::Schedule(random_m_standoff_time+m_hello_message_timeout+m_table_scan_timeout, &ecsClusterApp::ScheduleScan, this);
}

void ecsClusterApp::ScheduleHello() {
  SendPing(GenerateNodeStatusToUint());
  m_hello_event = Simulator::Schedule(m_hello_message_timeout, &ecsClusterApp::ScheduleHello, this);
}

void ecsClusterApp::ScheduleScan() {
  RefreshRoutingTable();
  RefreshInformationTable();
  m_table_scan_event = Simulator::Schedule(m_table_scan_timeout, &ecsClusterApp::ScheduleScan, this);
}

void ecsClusterApp::ScheduleAverageRecording() {
  if(m_state != State::RUNNING) return;
  if(Simulator::Now().GetSeconds() > 55) {
    if(GetStatus() == Node_Status::CLUSTER_HEAD) {
      //std::cout << "increasing head!";
      stats.IncreaseCHCount();
      stats.IncreaseClusterSizeCount(m_informationTable.size());
    } else if(GetStatus() == Node_Status::CLUSTER_MEMBER) {
      //std::cout << "increasing mem!";
      stats.IncreaseCMemCount();
    } else if(GetStatus() == Node_Status::CLUSTER_GATEWAY) {
      //std::cout << "increasing gate!";
      stats.IncreaseGateCount();
      uint64_t num_heads_covering = GetNumHeadsCovering();
      stats.IncreaseGateCoverageCount(num_heads_covering);
    } else if(GetStatus() == Node_Status::CLUSTER_GUEST) {
      stats.IncreaseGuestCount();
      uint64_t num_access_points = GetNumAccessPoints();
      stats.IncreaseAccessPointCount(num_access_points);
    }
  }
  Simulator::Schedule(60.0_sec, &ecsClusterApp::ScheduleAverageRecording, this);
}

void ecsClusterApp::ScheduleClusterHeadClaim() {
  if(m_state != State::RUNNING) return;
  SendClusterHeadClaim();
}

// Logging event to print information table 3 seconds after clusterhead claim
void ecsClusterApp::SchedulePrintInformationTable() {
  m_print_table_event =
      Simulator::Schedule(1.0_sec, &ecsClusterApp::PrintCustomClusterTable, this);
}
// Logging event for clusterheads to print information table every X seconds
void ecsClusterApp::ScheduleHeadPrintTable() {
  if(GetStatus()==Node_Status::CLUSTER_HEAD) {
    PrintCustomClusterTable();
  }
  Simulator::Schedule(10.0_sec, &ecsClusterApp::ScheduleHeadPrintTable, this);
}


/**
Message handlers
**/
void ecsClusterApp::HandleRequest(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address from;
  Address localAddress;

  while ((packet = socket->RecvFrom(from))) {
    socket->GetSockName(localAddress);

    uint32_t srcAddress = InetSocketAddress::ConvertFrom(from).GetIpv4().Get();
    ecs::packets::Message message = ParsePacket(packet);

    if(CheckDuplicateMessage(message.id())) {
      NS_LOG_INFO("already recieved this message, dropping.");
      return;
    }
    if(message.has_ping()) {
      //ping received
      stats.IncreaseClusteringMessages();
      //std::cout << "ping recieved at time " << Simulator::Now().GetSeconds() << "\n";
      HandlePing(srcAddress, message.node_status());
    } else if(message.has_claim()) {
      //CH claim received
      stats.IncreaseClusterChangeMessages();
      stats.IncreaseClusteringMessages();
     // std::cout << "claim recieved at time " << Simulator::Now().GetSeconds() << "\n";
      HandleClaim(srcAddress);
    } else if(message.has_meeting()) {
      //clusterhead meeting, handle by sending number of connected nodes
      //(i.e. information table size) to other. if less table size, resign
      stats.IncreaseClusterChangeMessages();
      stats.IncreaseClusteringMessages();
     // std::cout << "meeting recieved at time " << Simulator::Now().GetSeconds() << "\n";
      HandleMeeting(srcAddress,message.node_status(),message.meeting().tablesize());
    } else if(message.has_resign()) {
      //clusterhead meeting has occured, and the node broadcasting this message
      //has a smaller information table, thus causing it to resign.
      stats.IncreaseClusterChangeMessages();
      stats.IncreaseClusteringMessages();
      //std::cout << "resign recieved at time " << Simulator::Now().GetSeconds() << "\n";
      HandleCHResign(srcAddress,message.node_status());
    } else if(message.has_status()) {
      //Simple message relaying a given node's node_status to another node.
      //Sent when a clusterhead claim is received during cluster formation
      //std::cout << "status recieved at time " << Simulator::Now().GetSeconds() << "\n";
      HandleStatus(srcAddress,message.node_status());
    } else {
      std::cout << "handling message: other\n";
      NS_LOG_WARN("Unknown message type");
    }
  }
}
/**
Message handlers below. Above is sorting the messages from one another
**/

//Handles pings being received from another node (probably will be used to update information table)
void ecsClusterApp::HandlePing(uint32_t nodeID, uint8_t node_status) {
  // set up row to check
  InformationTableRow row;
  row.nodeID = nodeID;
  row.status = GenerateStatusFromUint(node_status);
  row.clusterHeadID = 0;
  row.accessPointID = 0;
  row.entryTime = Simulator::Now().GetSeconds();

  //check if node already exists in table
  std::list<InformationTableRow>::iterator it;
  //bool nodeExists = false;
  //uint32_t count = 0;
  //std::cout << "table size: " << m_informationTable.size() << "\n";
  for (it = m_informationTable.begin(); it != m_informationTable.end(); it++) {
    //std::cout << "count " << count << "\n";
    //count++;
    // if(++count >= m_informationTable.size())
    //     break;
//    std::cout << "it: " << it->nodeID << " row: " << row.nodeID << "\n";
    if(it->nodeID==row.nodeID) {
      //std::cout << "erasing node " << it->nodeID << "\n";
      m_informationTable.erase(it++);
      //break;
      //m_informationTable.push_back(row);
      //nodeExists = true;
    }
    
  }
  m_informationTable.push_back(row);
  
  switch (node_status) {
  case 1: //CH sent ping
    switch(GetStatus()) { //my status
      case Node_Status::UNSPECIFIED:
          SetStatus(Node_Status::CLUSTER_MEMBER);
          SendStatus(nodeID);
        break;
      case Node_Status::CLUSTER_HEAD:
        SendCHMeeting(nodeID);
        break;
      case Node_Status::CLUSTER_MEMBER: {
        // Check if node is recieving ping from original clusterhead
          uint32_t currentCH_id = GetMemberClusterHeadsID();
          if(currentCH_id!=0) {
            if(nodeID!=currentCH_id) {
              SetStatus(Node_Status::CLUSTER_GATEWAY);
              stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
              // nodes surrounding will be updated on next ping
            }
          }
        }
        break;
      case Node_Status::CLUSTER_GATEWAY:
        // eventually add to clusterheadIDs
        // nothing else to do
        break;
      case Node_Status::STANDALONE:
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendStatus(nodeID);
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      case Node_Status::CLUSTER_GUEST:
        //std::cout << "change guest to member from CH ping";
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendStatus(nodeID);
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      default:
        //std::cout << "should never get here\n";
        break;
    }
    break;
  case 2: //CM sent ping
    switch(GetStatus()) {
      case Node_Status::UNSPECIFIED:
        //std::cout << "setting cguest from unspec\n";
        SetStatus(Node_Status::CLUSTER_GUEST);
        SendStatus(nodeID);
        break;
      case Node_Status::STANDALONE:
        //std::cout << "setting cguest from standalone\n";
        SetStatus(Node_Status::CLUSTER_GUEST);
        SendStatus(nodeID);
        break;
      default:
        // do nothing, no interactions with combo given
        break;
    }
  case 3: //C_GW sent ping
    switch(GetStatus()) {
      case Node_Status::UNSPECIFIED:
        //std::cout << "setting cguest from unspec @ gateway\n";
        SetStatus(Node_Status::CLUSTER_GUEST);
        SendStatus(nodeID);
        break;
      case Node_Status::STANDALONE:
        //std::cout << "setting cguest from standalone @ gateway\n";
        SetStatus(Node_Status::CLUSTER_GUEST);
        SendStatus(nodeID);
        break;
      default:
        // do nothing, no interactions with combo given
        break;
    }
  default:
    // will get here with a standalone, unspec, or guest sending a ping, do nothing
    // since it will be handled when the node recieves instead of sends
    // is added to table already so handled enough.
    break;
  }
  // if(node_status==1 && GetStatus()==Node_Status::CLUSTER_HEAD) {// 2 cluster heads
  //   SendCHMeeting(nodeID);
  // } else if(node_status && GetStatus()==Node_Status::CLUSTER_MEMBER) {// cluster head send to member
  //   SetStatus(Node_Status::CLUSTER_GATEWAY);
  //   SendStatus(nodeID);
  //   stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
  // } else if(node_status && GetStatus()==Node_Status::CLUSTER_GUEST) {// CH send to Cguest
  //   SetStatus(Node_Status::CLUSTER_MEMBER);
  //   SendStatus(nodeID);
  //   stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
  // } else if(node_status==2 && GetStatus()==Node_Status::STANDALONE) { // CM sent to standalone
  //   SetStatus(Node_Status::CLUSTER_GUEST);
  //   SendStatus(nodeID);
  // } else if()
}
//Handles another node sending a clusterhead claim. Follows the algorithm from the paper
void ecsClusterApp::HandleClaim(uint32_t nodeID) {
  InformationTableRow row;
  row.nodeID = nodeID;
  row.status = Node_Status::CLUSTER_HEAD;
  row.clusterHeadID = 0;
  row.accessPointID = 0;
  row.entryTime = Simulator::Now().GetSeconds();
  m_informationTable.push_back(row);
  //m_informationTable[nodeID] = std::pair<Node_Status::CLUSTER_HEAD, Simulator::Now().GetSeconds()>;
  //if in standoff, automatically join their cluster
  //NS_LOG_UNCOND("standoffTime for " << GetID() << " is " << m_standoff_time << " NStime= " << Simulator::Now());
  if(Simulator::Now() < m_standoff_time) {
    //NS_LOG_UNCOND("Canceling CH claim for " << GetID());
    //NS_LOG_UNCOND("Claim recieved, cancelling Claim event for " << GetID() << "(pre-standoff time!)");
    m_CH_claim_event.Cancel();
    switch (GetStatus()) {
      case Node_Status::UNSPECIFIED:
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendStatus(nodeID);
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      case Node_Status::CLUSTER_MEMBER:
        SetStatus(Node_Status::CLUSTER_GATEWAY);
        SendStatus(nodeID);
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      default:
        SendStatus(nodeID);
    }
  } else {
    //should theoretically be no way a cluster head receives this message??
    //Also no real need to adjust a cluster gateway if it already exists as one
    //    - maybe just send back status as a default
    m_CH_claim_event.Cancel();
    //NS_LOG_UNCOND("Claim recieved, cancelling Claim event for " << GetID());
    switch (GetStatus()) {
      case Node_Status::CLUSTER_MEMBER:
        SetStatus(Node_Status::CLUSTER_GATEWAY);
        SendPing(GenerateNodeStatusToUint());
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      case Node_Status::STANDALONE:
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendPing(GenerateNodeStatusToUint());
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      case Node_Status::CLUSTER_GUEST:
        //std::cout << "guest to member from claim";
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendPing(nodeID);
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
        break;
      default:
        SendStatus(nodeID);
    }
  }
}
//Handles response from a given node. I dont think this actually needs to be here right now.
void ecsClusterApp::HandleResponse(uint32_t nodeID, uint8_t node_status) {
  InformationTableRow row;
  row.nodeID = nodeID;
  row.status = GenerateStatusFromUint(node_status);
  row.clusterHeadID = 0;
  row.accessPointID = 0;
  row.entryTime = Simulator::Now().GetSeconds();
  m_informationTable.push_back(row);
  //m_informationTable[nodeID] = std::pair<GenerateStatusFromUint(node_status), Simulator::Now().GetSeconds()>;
}
//Handles ClusterHeadMeeting messaage received
void ecsClusterApp::HandleMeeting(uint32_t nodeID, uint8_t node_status, uint64_t neighborhood_size) {
  if(GetStatus()!=Node_Status::CLUSTER_HEAD) {
    NS_LOG_ERROR("ClusterHead meeting sent to node which isnt a cluster head");
    return;
  }
  //sending node's degree is greater than this node's, therefore resign
  //EQUAL TO WAS NOT ACTUALLY DISCUSSED IN ORIGINAL PAPER
  //For simplicity's sake, still resign due to minimizing number of messages.
  if(neighborhood_size >= m_informationTable.size()) {
    //Send CHResign
    SendResign(GenerateNodeStatusToUint());
    SetStatus(Node_Status::CLUSTER_MEMBER);
    //Broadcast new status to update nodes
    SendPing(nodeID);
    stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
  } else if(neighborhood_size < m_informationTable.size()) {
    //Send CHmeeting back to original with my size
    SendCHMeeting(nodeID);
  } else {
    NS_LOG_ERROR("Somehow got past neighborhood size block??");
  }
}
//Handles CHResign message
void ecsClusterApp::HandleCHResign(uint32_t nodeID, uint8_t node_status) {
  InformationTableRow row;
  row.nodeID = nodeID;
  row.status = GenerateStatusFromUint(node_status);
  row.clusterHeadID = 0;
  row.accessPointID = 0;
  row.entryTime = Simulator::Now().GetSeconds();
  m_informationTable.push_back(row);
  //m_informationTable[nodeID] = std::pair<GenerateStatusFromUint(node_status), Simulator::Now().GetSeconds()>;

  //if gateway, check for number of CHs
  if(m_node_status == Node_Status::CLUSTER_GATEWAY) {
    std::list<InformationTableRow>::iterator it;
    std::list<uint32_t> ch_IDs;
    int num_CHs = 0;
    int num_mems = 0;
    for (it = m_informationTable.begin(); it != m_informationTable.end(); it++) {
      if(it->status == Node_Status::CLUSTER_HEAD) {
        num_CHs+=1;
        ch_IDs.push_back(it->nodeID);
      } else if(it->status == Node_Status::CLUSTER_MEMBER || it->status == Node_Status::CLUSTER_GATEWAY) {
        num_mems+=1;
      }
    }
    // If 1 CH, change to CM
    if(num_CHs == 1) {
      SetStatus(Node_Status::CLUSTER_MEMBER);
      SendPing(GenerateNodeStatusToUint());
      stats.recordMembershipEnd(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
      stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), ch_IDs.front());
    } else if(num_CHs == 0 && num_mems>0) {
      //std::cout << "setting cguest from resign\n";
      SetStatus(Node_Status::CLUSTER_GUEST);
      SendPing(GenerateNodeStatusToUint());
      stats.recordMembershipEnd(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
    } else if(num_CHs > 1) {
      SetStatus(Node_Status::CLUSTER_GATEWAY);
      SendPing(GenerateNodeStatusToUint());
      stats.recordMembershipEnd(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), nodeID);
      std::list<uint32_t>:: iterator chIterator;
      for(chIterator = ch_IDs.begin(); chIterator != ch_IDs.end(); chIterator++) {
        stats.recordMembershipStart(NodeStatusToStringFromTable(m_node_status), m_address, Simulator::Now().GetSeconds(), *chIterator);
      }
      
    }
    
    //Should be absolutely no way that there are no CHs or members/gateways to connect to at this point
  }

  //Possibly queue new CH formation??
  if(m_node_status != Node_Status::CLUSTER_HEAD) {
    //iterate through information table, if any heads, break, otherwise send CH claim
    //NS_LOG_UNCOND("RESIGN HANDLE - CHECKING FOR CH IN INFORMATION TABLE");
    std::list<InformationTableRow>::iterator it;
    bool checkForCH = true;
    for (it = m_informationTable.begin(); it != m_informationTable.end(); it++) {
      if(it->status == Node_Status::CLUSTER_HEAD) {
        checkForCH=false;
        break;
      }
    }
    if(checkForCH) {
      //Should be cancellable now that it is an event
      Ptr<UniformRandomVariable> standoff = CreateObject<UniformRandomVariable> ();
      standoff->SetAttribute("Min", DoubleValue(0.1));
      standoff->SetAttribute("Max", DoubleValue(0.5));
      random_m_standoff_time = ns3::Time::FromDouble(standoff->GetValue(), ns3::Time::Unit::S);

      m_CH_claim_event = Simulator::Schedule(random_m_standoff_time, &ecsClusterApp::ScheduleClusterHeadClaim, this);
    }
  }
}

//Handles status message from neighbor in response to clusterhead claim during standoff
void ecsClusterApp::HandleStatus(uint32_t nodeID, uint8_t node_status) {
  InformationTableRow row;
  row.nodeID = nodeID;
  row.status = GenerateStatusFromUint(node_status);
  row.clusterHeadID = 0;
  row.accessPointID = 0;
  row.entryTime = Simulator::Now().GetSeconds();
  m_informationTable.push_back(row);
  //m_informationTable[nodeID] = std::pair<GenerateStatusFromUint(node_status), Simulator::Now().GetSeconds()>;
  if (m_CH_Claim_flag) {
    //NS_LOG_UNCOND("recording status recieve");
    stats.recordCHRecieveStatus(m_address, Simulator::Now().GetSeconds());
  }
  //NS_LOG_UNCOND("!!!!!!!!!!!HANDLE STATUS!!!!!!!!!!!");
  //PrintCustomClusterTable();
}

bool ecsClusterApp::CheckDuplicateMessage(uint64_t messageID) {
  // true if it is in the list
  bool status = m_received_messages.find(messageID) != m_received_messages.end();
  m_received_messages.insert(messageID);

  return status;
}

//Simple function which translates the Node_Status enum to an integer for easier communication
uint8_t ecsClusterApp::GenerateNodeStatusToUint() {
  switch (GetStatus()) {
    case Node_Status::UNSPECIFIED:
      return 0;
    case Node_Status::CLUSTER_HEAD:
      return 1;
    case Node_Status::CLUSTER_MEMBER:
      return 2;
    case Node_Status::CLUSTER_GATEWAY:
      return 3;
    case Node_Status::STANDALONE:
      return 4;
    case Node_Status::CLUSTER_GUEST:
      return 5;
  }
  std::cout << "GenerateNodeStatusToUint returning unspec\n";
  return 0;
}
//Copy of function above but for a specific node in an information table.
//Used for printing out the information table of a clusterhead claim
uint8_t ecsClusterApp::NodeStatusToUintFromTable(Node_Status status) {
  switch (status) {
    case Node_Status::UNSPECIFIED:
      return 0;
    case Node_Status::CLUSTER_HEAD:
      return 1;
    case Node_Status::CLUSTER_MEMBER:
      return 2;
    case Node_Status::CLUSTER_GATEWAY:
      return 3;
    case Node_Status::STANDALONE:
      return 4;
    case Node_Status::CLUSTER_GUEST:
      return 5;
  }
  std::cout << "NodeStatusToUintFromTable returning unspec\n";
  return 0;
}

std::string ecsClusterApp::NodeStatusToStringFromTable(Node_Status status) {
  switch (status) {
    case Node_Status::UNSPECIFIED:
      return "Unspecified";
    case Node_Status::CLUSTER_HEAD:
      return "Cluster Head";
    case Node_Status::CLUSTER_MEMBER:
      return "Cluster Member";
    case Node_Status::CLUSTER_GATEWAY:
      return "Cluster Gateway";
    case Node_Status::STANDALONE:
      return "Standalone";
    case Node_Status::CLUSTER_GUEST:
      return "Cluster Guest";
  }
  std::cout << "NodeStatusToStringFromTable returning unspec\n";
  return "0";
}

//Simple function to translate uint to node_status enum
ecsClusterApp::Node_Status ecsClusterApp::GenerateStatusFromUint(uint8_t status) {
  switch(status) {
    case 0:
      return Node_Status::UNSPECIFIED;
    case 1:
      return Node_Status::CLUSTER_HEAD;
    case 2:
      return Node_Status::CLUSTER_MEMBER;
    case 3:
      return Node_Status::CLUSTER_GATEWAY;
    case 4:
      return Node_Status::STANDALONE;
    case 5:
      return Node_Status::CLUSTER_GUEST;
    default:
      return Node_Status::UNSPECIFIED;
  }
  std::cout << "GenerateStatusFromUint returning unspec on status " << status << "\n";
  return Node_Status::UNSPECIFIED;
}


// this will generate the ID value to use for the requests this is a static function that should be
// called to generate all the ids to ensure they are unique
uint64_t ecsClusterApp::GenerateMessageID() {
  static uint64_t id = 0;
  return ++id;
}

std::string ecsClusterApp::GetRoutingTableString() {
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ptr<Ipv4RoutingProtocol> table = ipv4->GetRoutingProtocol();
  NS_ASSERT(table);
  std::ostringstream ss;
  table->PrintRoutingTable(Create<OutputStreamWrapper>(&ss));

  return ss.str();
}

void ecsClusterApp::RefreshRoutingTable() {
  //NS_LOG_UNCOND("HERE1");
  m_peerTable.UpdateTable(GetRoutingTableString());
}

void ecsClusterApp::RefreshInformationTable() {
  std::list<InformationTableRow>::iterator it;
  std::list<InformationTableRow> toDelete;
  
  if(m_informationTable.size()>0) {
    if(GetStatus()==Node_Status::CLUSTER_HEAD) {
    //std::cout << "CH IT size: " << m_informationTable.size() << "\n";
    }
    for (it = m_informationTable.begin(); it!=m_informationTable.end(); ++it) {
      double gap_time = Simulator::Now().GetSeconds() - it->entryTime;
      //std::cout << "entry time: " << it->entryTime << " current time: " << Simulator::Now().GetSeconds() << " valid entry: " << m_valid_entry_timeout.GetSeconds() << "\n";
      if(gap_time > m_valid_entry_timeout.GetSeconds()) {
        m_informationTable.erase(it++);
      }
    }
  }

  // if(m_informationTable.size()>0) {
  //   for(it = m_informationTable.begin(); it!= m_informationTable.end(); ++it) {
  //     if(it->status != Node_Status::UNSPECIFIED) {
  //       std::cout << "\n" << "id: " << it->nodeID << "\n";
  //       std::cout << "status: " << NodeStatusToStringFromTable(it->status) << "\n";
  //       std::cout << "sim time: " << Simulator::Now().GetSeconds() << "\n";
  //       std::cout << "it time: " << it->entryTime << "\n";
  //       std::cout << "m time: " << m_valid_entry_timeout.GetSeconds() << "\n";
  //       double gap_time = Simulator::Now().GetSeconds() - it->entryTime;
  //       std::cout << "calc time: " << gap_time << "\n";
  //       if(gap_time > m_valid_entry_timeout.GetSeconds()) {
  //         InformationTableRow deletePlaceholder;
  //         deletePlaceholder.nodeID = it->nodeID;
  //         deletePlaceholder.status = it->status;
  //         deletePlaceholder.clusterHeadID = it->clusterHeadID;
  //         deletePlaceholder.accessPointID = it->accessPointID;
  //         deletePlaceholder.entryTime = it->entryTime;
  //         toDelete.push_back(deletePlaceholder);
  //       }
  //     }
  //   }
  //   std::cout << "toDelete #" << toDelete.size() << "\n";
  //   for(it = toDelete.begin(); it != toDelete.end(); ++it) {
  //     std::cout << "erasing node " << it->nodeID << "\n";
  //     m_informationTable.remove(it);
  //   }
  // }
}

void ecsClusterApp::CancelEventMap(std::map<uint32_t, EventId> events) {
  for(auto it = events.begin(); it != events.end(); ++it) {
    it->second.Cancel();
  }
}
void ecsClusterApp::CancelEventMap(std::map<uint64_t, EventId> events) {
  for (auto it = events.begin(); it != events.end(); ++it) {
    it->second.Cancel();
  }
}
/**
 * @brief 
 * If a CH has more than 5 nodes, then they deserve to stand as a CH in order to translate information as needed
 */

void ecsClusterApp::CheckCHShouldResign() {
  //information table = empty
  bool can_resign = false;
  if(m_informationTable.size()<1) {
    //m_node_status = Node_Status::STANDALONE;
    SetStatus(Node_Status::STANDALONE);
  } else if(m_informationTable.size()+1<=5) {
    // <= 5 here seems suuuuuuper high for a network. Would like to check on higher/lower numbers to see impact
    // +1 because informationTable does not track self
    std::list<InformationTableRow>::iterator it;
    for(it = m_informationTable.begin(); it!= m_informationTable.end(); ++it) {
      if(it->status == Node_Status::CLUSTER_GATEWAY) {
        //If there is a cluster gateway to another cluster, I can resign and become a clusterguest through them!
        can_resign=true;
        break;
      }
    }
    if(can_resign) {
      //NS_LOG_UNCOND("\nResign sent from information table <=5 & gateway from " << GetID());
      //PrintCustomClusterTable();
      m_node_status = Node_Status::CLUSTER_GUEST;
      SendResign(GenerateNodeStatusToUint());
    }
  }
}

uint64_t ecsClusterApp::GetNumHeadsCovering() {
  uint64_t num_heads_covering=0;
  std::list<InformationTableRow>::iterator it;
  for(it = m_informationTable.begin(); it!=m_informationTable.end(); ++it) {
    if(it->status == Node_Status::CLUSTER_HEAD) {
      num_heads_covering++;
    }
  }
  return num_heads_covering;
}
uint64_t ecsClusterApp::GetNumAccessPoints() {
  uint64_t num_access_points=0;
  std::list<InformationTableRow>::iterator it;
  for(it = m_informationTable.begin(); it!=m_informationTable.end(); ++it) {
    if(it->status == Node_Status::CLUSTER_MEMBER || it->status == Node_Status::CLUSTER_GATEWAY) {
      num_access_points++;
    }
  }
  return num_access_points;
}

void ecsClusterApp::CleanUp() { google::protobuf::ShutdownProtobufLibrary(); }

void ecsClusterApp::PrintCustomClusterTable() {
  std::list<InformationTableRow>::iterator it;
  NS_LOG_UNCOND("Printing custom cluster table for " << GetID() << " with size " << m_informationTable.size() << " at " << Simulator::Now().GetSeconds());
  NS_LOG_UNCOND(GetID() << " \t " << NodeStatusToStringFromTable(m_node_status));
  for(it = m_informationTable.begin(); it != m_informationTable.end(); ++it) {
    NS_LOG_UNCOND(it->nodeID << " \t " << NodeStatusToStringFromTable(it->status) << " \t " << (int)NodeStatusToUintFromTable(it->status));
  }
  NS_LOG_UNCOND("\n");

}

uint32_t ecsClusterApp::GetMemberClusterHeadsID() {
  uint32_t chID = 0;
  for (auto it = m_informationTable.begin(); it != m_informationTable.end(); ++it) {
    if(it->status == Node_Status::CLUSTER_HEAD) {
      chID = it->nodeID;
    }
  }
  return chID;
}

std::list<uint32_t> ecsClusterApp::GetGatewayClusterHeadIDs() {
  std::list<uint32_t> chIDs;
  for (auto it = m_informationTable.begin(); it != m_informationTable.end(); ++it) {
    if(it->status == Node_Status::CLUSTER_HEAD) {
      chIDs.push_back(it->nodeID);
    }
  }
  return chIDs;
}



} //namespace ecs
