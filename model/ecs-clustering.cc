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
      MakeTimeChecker(0.1_sec));
  return id;
}
// .AddAttribute(
//   "NodeStatus",
//   "The status of the node based on it's role in the network/cluster (n_s)",
//   EnumValue(&ecsClusterApp::Node_Status),
//   MakeEnumChecker<Node_Status>(
//     Node_Status::UNSPECIFIED, "Node_Status::UNSPECIFIED",
//     Node_Status::CLUSTER_HEAD, "Node_Status::CLUSTER_HEAD",
//     Node_Status::CLUSTER_MEMBER, "Node_Status::CLUSTER_MEMBER",
//     Node_Status::CLUSTER_GATEWAY, "Node_Status::CLUSTER_GATEWAY",
//     Node_Status::STANDALONE, "Node_Status::STANDALONE",
//     Node_Status::CLUSTER_GUEST, "Node_Status::CLUSTER_GUEST"));


//override
void ecsClusterApp::StartApplication() {
  if(m_state == State::RUNNING) {
    NS_LOG_DEBUG("Ignoring ecsClusterApp::StartApplication request on already started application");
    return;
  }
  //NS_LOG_UNCOND("Starting ecsClusterApp");
  m_state = State::NOT_STARTED;

  if(m_socket_recv == 0) {
    m_socket_recv = SetupSocket(APPLICATION_PORT, 0); //not implemented
  }
  if(m_neighborhood_socket == 0) {
    m_neighborhood_socket = SetupSocket(APPLICATION_PORT, m_neighborhoodHops);
  }
  // if(m_election_socket == 0) {
  //   m_election_socket = SetupSocket(APPLICATION_PORT, m_electionNeighborhoodHops);
  // }

  m_address = GetID();
  m_peerTable = Table(m_profileDelay.GetSeconds(), m_neighborhoodHops);
  m_state = State::RUNNING;
  m_node_status = Node_Status::UNSPECIFIED;
  m_inquiry_timeout = ns3::Time::FromDouble(5.0, ns3::Time::Unit::S);


  //Scheduling of events. Maybe this is where i put the algorithm??

  //SchedulePing();
  //NS_LOG_UNCOND("Ping Scheduled");
  ScheduleWakeup();
  //SchedulePing();
  //ScheduleInquiry();
  //ScheduleRefreshRoutingTable();
  //ScheduleClusterFormationWatchdog();
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
  m_inquiry_event.Cancel();

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
Ptr<Packet> ecsClusterApp::GenerateResign() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  message.mutable_resign();

  return GeneratePacket(message);

}
Ptr<Packet> ecsClusterApp::GenerateInquiry() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  message.mutable_inquiry();
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
  //total_messages_sent++;
}
void ecsClusterApp::SendMessage(Ipv4Address dest, Ptr<Packet> packet) {
  m_socket_recv->SendTo(packet, 0, InetSocketAddress(dest, APPLICATION_PORT));
  //total_messages_sent++;
}

/**
Marshall calls this section the "send messages" section   :)
I imagine this is just functions that call the helper functions in the other sending section
**/
void ecsClusterApp::SendPing(uint8_t node_status) {
  Ptr<Packet> message = GeneratePing(node_status);
  BroadcastToNeighbors(message);
  //NS_LOG_UNCOND("Ping Sent!");
}

void ecsClusterApp::SendClusterHeadClaim() {
  SetStatus(Node_Status::CLUSTER_HEAD);
  Ptr<Packet> message = GenerateClusterHeadClaim();
  BroadcastToNeighbors(message);
//  NS_LOG_UNCOND("CH Claim Sent!");
}

void ecsClusterApp::SendStatus(uint32_t nodeID, uint8_t statusInt) {
  Ptr<Packet> message = GeneratePing(GenerateNodeStatusToUint());
  SendMessage(Ipv4Address(nodeID), message);
  //NS_LOG_UNCOND("Status Sent!");
}
void ecsClusterApp::SendCHMeeting(uint32_t nodeID) {
  Ptr<Packet> message = GenerateMeeting();
  SendMessage(Ipv4Address(nodeID), message);
//  NS_LOG_UNCOND("CH Meeting Sent!");
}
void ecsClusterApp::SendResign(uint32_t nodeID) {
  Ptr<Packet> message = GenerateResign();
  BroadcastToNeighbors(message);
//  NS_LOG_UNCOND("Resign Sent!");
}
void ecsClusterApp::SendInquiry() {
  Ptr<Packet> message = GenerateInquiry();
  BroadcastToNeighbors(message);
//  NS_LOG_UNCOND("Inquiry sent");
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
  //SendClusterHeadClaim();
  //SetStatus(Node_Status::CLUSTER_HEAD);

  Ptr<UniformRandomVariable> standoff = CreateObject<UniformRandomVariable> ();
  standoff->SetAttribute("Min", DoubleValue(0.1));
  standoff->SetAttribute("Min", DoubleValue(3.0));
  m_standoff_time = ns3::Time::FromDouble(standoff->GetValue(), ns3::Time::Unit::S);
  m_CH_claim_event = Simulator::Schedule(m_standoff_time, &ecsClusterApp::SendClusterHeadClaim, this);
}

void ecsClusterApp::ScheduleInquiry() {
  if(m_state != State::RUNNING) return;
  SendInquiry();

  m_inquiry_event = Simulator::Schedule(m_inquiry_timeout, &ecsClusterApp::ScheduleInquiry, this);

}

void ecsClusterApp::ScheduleClusterHeadClaim() {
  if(m_state != State::RUNNING) return;

  SendClusterHeadClaim();
  SetStatus(Node_Status::CLUSTER_HEAD);
  //m_node_status = CLUSTER_HEAD;

  m_CH_claim_event = Simulator::Schedule(m_profileDelay, &ecsClusterApp::ScheduleClusterHeadClaim, this);
}

void ecsClusterApp::ScheduleRefreshRoutingTable() {
  if (m_state != State::RUNNING) return;

  RefreshRoutingTable();

  m_table_update_event =
      Simulator::Schedule(1.0_sec, &ecsClusterApp::ScheduleRefreshRoutingTable, this);
}
//TODO: add more scheduled events????

/**
Message handlers
**/
//This is the main message handler
void ecsClusterApp::HandleRequest(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address from;
  Address localAddress;

  while ((packet = socket->RecvFrom(from))) {
    socket->GetSockName(localAddress);
    // NS_LOG_UNCOND(
    //   "At time " << Simulator::Now().GetSeconds() << "s client recieved " << packet->GetSize()
    //              << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
    //              << InetSocketAddress::ConvertFrom(from).GetPort());

    uint32_t srcAddress = InetSocketAddress::ConvertFrom(from).GetIpv4().Get();
    ecs::packets::Message message = ParsePacket(packet);

    if(CheckDuplicateMessage(message.id())) {
      NS_LOG_INFO("already recieved this message, dropping.");
    //  stats.incDuplicate();
      return;
    }
    // std::string s;
    // google::protobuf::TextFormat::PrintToString(message, &s);
    // std::cout << s;
    if(message.has_ping()) {
      //ping received
      //stats.incReceived(Stats::Type::PING);
      //NS_LOG_UNCOND("Ping Received");
      HandlePing(srcAddress, message.node_status());
    } else if(message.has_claim()) {
      //CH claim received
      //stats.incReceived(Stats::Type::Claim);
      NS_LOG_UNCOND("Claim Received " << std::to_string(GenerateNodeStatusToUint()));
      HandleClaim(srcAddress);
    } else if(message.has_meeting()) {
      //clusterhead meeting, handle by sending number of connected nodes
      //(i.e. information table size) to other. if less table size, resign
      //stats.incReceived(Stats::Type::ClusterHeadMeeting);
      NS_LOG_UNCOND("Meeting Received " << std::to_string(GenerateNodeStatusToUint()));
      HandleMeeting(srcAddress,message.node_status(),message.meeting().tablesize());
    } else if(message.has_resign()) {
      //clusterhead meeting has occured, and the node broadcasting this message
      //has a smaller information table, thus causing it to resign. This message
      //is broadcasted to all neighbors on the information table. A node recieving this
      //message must then ping all neighbors to figure out it's new node status.
      //stats.incReceived(Stats::Type::ClusterHeadResign);
      NS_LOG_UNCOND("CHResign Received " << std::to_string(GenerateNodeStatusToUint()));
      HandleCHResign(srcAddress);
    } else if(message.has_inquiry()) {
      //Happens when a node needs to learn it's neighbors' node types in order
      //to decide it's own
      //stats.incReceived(Stats::Type::NeighborhoodInquiry);
      //NS_LOG_UNCOND("Inquiry Received");
      HandleInquiry(srcAddress,message.node_status());
    } else if(message.has_status()) {
      //Simple message relaying a given node's node_status to another node.
      //Sent when a clusterhead claim is received during cluster formation
    //  stats.incReceived(Stats::Type::StatusRelay);
      //NS_LOG_UNCOND("Status Received");
      HandleStatus(srcAddress,message.node_status());
    } else {
    //  stats.incReceived(Stats::Type::UKNOWN);
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
  m_informationTable[nodeID] = GenerateStatusFromUint(node_status);
  if(node_status && GetStatus()==Node_Status::CLUSTER_HEAD) {
    SendCHMeeting(nodeID);
  } else if(node_status && GetStatus()==Node_Status::CLUSTER_MEMBER) {
    //m_node_status = CLUSTER_GATEWAY;
    SetStatus(Node_Status::CLUSTER_GATEWAY);
    SendStatus(nodeID, GenerateNodeStatusToUint());
  } else if(node_status && GetStatus()==Node_Status::CLUSTER_GUEST) {
    SetStatus(Node_Status::CLUSTER_MEMBER);
    //m_node_status = CLUSTER_MEMBER;
    SendStatus(nodeID, GenerateNodeStatusToUint());
  }
}
//Handles another node sending a clusterhead claim. Follows the algorithm from the paper
void ecsClusterApp::HandleClaim(uint32_t nodeID) {
  m_informationTable[nodeID] = Node_Status::CLUSTER_HEAD;
  //if in standoff, automatically join their cluster
  if(Simulator::Now() < m_standoff_time) {
    m_CH_claim_event.Cancel();
    switch (GetStatus()) {
      case Node_Status::UNSPECIFIED:
        SetStatus(Node_Status::CLUSTER_MEMBER);
        //m_node_status = CLUSTER_MEMBER;
        SendStatus(nodeID, GenerateNodeStatusToUint());
        break;
      case Node_Status::CLUSTER_MEMBER:
        //m_node_status = CLUSTER_GATEWAY;
        SetStatus(Node_Status::CLUSTER_GATEWAY);
        SendStatus(nodeID, GenerateNodeStatusToUint());
        break;
      default:
        SendStatus(nodeID, GenerateNodeStatusToUint());
    }
  } else {
    //should theoretically be no way a cluster head receives this message??
    //Also no real need to adjust a cluster gateway if it already exists as one
    //    - maybe just send back status as a default
    switch (GetStatus()) {
      case Node_Status::CLUSTER_MEMBER:
        //m_node_status = CLUSTER_GATEWAY;
        SetStatus(Node_Status::CLUSTER_GATEWAY);
        SendPing(GenerateNodeStatusToUint());
        break;
      case Node_Status::STANDALONE:
        //m_node_status = CLUSTER_MEMBER;
        SetStatus(Node_Status::CLUSTER_MEMBER);
        m_informationTable[nodeID] = m_node_status;
        SendPing(GenerateNodeStatusToUint());
        break;
      case Node_Status::CLUSTER_GUEST:
        //m_node_status = CLUSTER_MEMBER;
        SetStatus(Node_Status::CLUSTER_MEMBER);
        SendPing(nodeID);
        break;
      default:
        SendStatus(nodeID, GenerateNodeStatusToUint());
    }
  }
}
//Handles response from a given node. I dont think this actually needs to be here right now.
void ecsClusterApp::HandleResponse(uint32_t nodeID, uint8_t node_status) {
  m_informationTable[nodeID] = GenerateStatusFromUint(node_status);
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
    SendResign(nodeID);
    //Change my ns to member
    SetStatus(Node_Status::CLUSTER_MEMBER);
    //m_node_status = CLUSTER_MEMBER;
    //Send Status to original node
    SendPing(nodeID);
  } else if(neighborhood_size < m_informationTable.size()) {
    //Send CHmeeting back to original with my size
    SendCHMeeting(nodeID);
  } else {
    NS_LOG_ERROR("Somehow got past neighborhood size block??");
  }
}
//Handles CHResign message
void ecsClusterApp::HandleCHResign(uint32_t nodeID) {
  m_informationTable[nodeID] = m_node_status;
  NS_LOG_UNCOND(m_informationTable.size());
  //Possibly queue new CH formation??
  if(m_node_status != Node_Status::CLUSTER_HEAD) {
    //iterate through information table, if any heads, break, otherwise send CH claim
    std::map<uint32_t, Node_Status>::iterator it;
    bool checkForCH = true;
    for (it = m_informationTable.begin(); it != m_informationTable.end(); it++) {

      if(it->second == Node_Status::CLUSTER_HEAD) {
        checkForCH=false;
        break;
      }
    }
    if(checkForCH) {
      SendClusterHeadClaim();
    }
  } else {
    return;
  }
}
//Handles neighborhood inquiry from another node and updates this node's information table
//Sent from standalones and CHs
void ecsClusterApp::HandleInquiry(uint32_t nodeID, uint8_t node_status) {
  m_informationTable[nodeID] = GenerateStatusFromUint(node_status);
  SendStatus(nodeID, GenerateNodeStatusToUint());
}
//Handles status message from neighbor in response to clusterhead claim during standoff
void ecsClusterApp::HandleStatus(uint32_t nodeID, uint8_t node_status) {
  m_informationTable[nodeID] = GenerateStatusFromUint(node_status);
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
  return 0;
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

// void ecsClusterApp::ScheduleClusterFormationWatchdog() {
//   if(m_state != State::RUNNING) return;
//   m_cluster_watchdog_event = Simulator::Schedule(m_peer_timeout, &ecsClusterApp::ClusterFormation, this);
// }
// void ecsClusterApp::ClusterFormation() {
//   //TODO: Write cluster formation step. Incorporate all nodes to have their node status inside
// }

} //namespace ecs
