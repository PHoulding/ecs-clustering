/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/**
Steps:
1) ECS cluster formation step
  - Set all NODE_STATUS = UNSPECIFIED
  - All nodes on random timeout (this is undeclared, use like 0ms->1sec?)
  - Check for received CLUSTERHEAD_CLAIM
      - NODE_STATUS = CLUSTERMEMBER or = CLUSTERGATEWAY (if multiple received)
  - Send off CLUSTERHEAD_CLAIM message to all nodes
      - NODE_STATUS = CLUSTERHEAD
  - Receive message from nearby for CLUSTERMEMBERs (probably a scheduled wait here)
      - If no messages received, set NODE_STATUS = STANDALONE
  - Add node to information table
2) Cluster maintenance step (recurring during simulation)
**/

#include <algorithm>
#include <map>

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

#include "logging.h"
#include "nsutil.h"
#include "util.h"
#include "ecs-clustering.h"

//TODO add protobuf
//#include "proto/messages.pb.h"

namespace ecs {
using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED(ecsClusterApp);

TypeID ecsClusterApp::GetTypeId() {
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
      "NodeStatus",
      "The status of the node based on it's role in the network/cluster (n_s)",
      EnumValue(&ecsClusterApp::Node_Status),
      MakeEnumChecker<Node_Status>(
        Node_Status::UNSPECIFIED, "Node_Status::UNSPECIFIED",
        Node_Status::CLUSTER_HEAD, "Node_Status::CLUSTER_HEAD",
        Node_Status::CLUSTER_MEMBER, "Node_Status::CLUSTER_MEMBER",
        Node_Status::CLUSTER_GATEWAY, "Node_Status::CLUSTER_GATEWAY",
        Node_Status::STANDALONE, "Node_Status::STANDALONE",
        Node_Status::CLUSTER_GUEST, "Node_Status::CLUSTER_GUEST"));
  return id;
}

//override
void ecsClusterApp::StartApplication() {
  if(m_state == State::RUNNING) {
    NS_LOG_DEBUG("Ignoring ecsClusterApp::StartApplication request on already started application");
    return;
  }
  NS_LOG_DEBUG("Starting ecsClusterApp");
  m_state = State::NOT_STARTED;

  if(m_socket_recv == 0) {
    m_socket_recv = SetupSocket(APPLICATION_PORT, 0); //not implemented
  }
  if(m_neighborhood_socket == 0) {
    m_neighborhood_socket = SetupSocket(APPLICATION_PORT, m_neightborhoodHops);
  }
  if(m_election_socket == 0) {
    m_election_socket = SetupSocket(APPLICATION_PORT, m_electionNeighborhoodHops);
  }

  m_address = GetID();
  m_peerTable = Table(m_profileDelay.GetSeconds(), m_neighborhoodHops);
  m_state = State::RUNNING;


  //Scheduling of events. Maybe this is where i put the algorithm??
  SchedulePing();
  ScheduleClusterFormationWatchdog();
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
  socket->SetRecvCallback(MakeNullCallback<void, Ptr<Soocket>>());
}

/**
Send message wrappers
**/
void ecsClusterApp::SendToNodes(Ptr<Packet> message, const std::set<uint32_t> nodes) {
  for(std::set<uint32_t>::iterator it = nodes.begin(); it!=ndoes.end; ++it) {
    SendMessage(Ipv4Address(*it), message);
  }
}

/**
Generate messages to be sent
**/
//Possibly remove this first one as its mainly for data lookup in RHPMAN
Ptr<Packet> ecsClusterApp::GenerateLookup(
    uint64_t messageID,
    uint64_t dataID,
    double sigma,
    uint32_t srcNode) {
  ecs::packets::Message message;
  message.set_id(messageID);
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  ecs::packets::Request* request = message.mutable_request();
  request->set_data_id(dataID);
  request->set_requestor(srcNode);
  request->set_sigma(sigma);

  return GeneratePacket(message);
}
Ptr<Packet> ecsClusterApp::GeneratePing() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  ecs:packets::Ping* ping = message.mutable_ping();
  //ping->set_delivery_probability(profile);

  return GeneratePacket(message);
}

Ptr<Packet> ecsClusterApp::GenerateClusterHeadClaim() {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  message.mutable_claim();

  return GeneratePacket(message);
}

Ptr<Packet> ecsClusterApp::GenerateResponse(uint64_t responseTo) {
  ecs::packets::Message message;
  message.set_id(GenerateMessageID());
  message.set_timestamp(Simulator::Now().GetMilliSeconds());

  ecs::packets::Response* response = message.mutable_response();
  response->set_request_id(responseTo);

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
  total_messages_sent++;
}
void ecsClusterApp::SendMessage(Ipv4Address dest, Ptr<Packet> packet) {
  m_socket_recv->SendTo(packet, 0, InetSocketAddress(dest, APPLICATION_PORT));
  total_messages_sent++;
}

/**
Marshall calls this section the "send messages" section   :)
I imagine this is just functions that call the helper functions in the other sending section
**/
void ecsClusterApp::SendPing() {
  Ptr<Packet> message = GeneratePing();
  BroadcastToNeighbors(message);
}

void ecsClusterApp::SendResponse(uint64_t requestID, uint32_t nodeID) {
  Ptr<Packet> message = GenerateResponse(requestID);
  SendMessage(Ipv4Address(nodeID), message);
}

void ecsClusterApp::SendClusterHeadClaim() {
  Ptr<Packet> message = GenerateClusterHeadClaim();
  BroadcastToNeighbors(message);
}

/**
Event schedulers
**/
void ecsClusterApp::SchedulePing() {
  if(m_state != State::RUNNNING) return;
  SendPing();

  m_ping_event = Simulator::Schedule(m_profileDelay, &ecsClusterApp::SchedulePing, this);
}

void ecsClusterApp::ScheduleClusterHeadClaim() {
  if(m_state != State::RUNNING) return;

  SendClusterHeadClaim();
  m_node_status = CLUSTER_HEAD;

  m_CH_claim = Simulator::Schedule(m_profileDelay, &ecsClusterApp::ScheduleClusterHeadClaim, this);
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
    NS_LOG_INFO(
      "At time " << Simulator::Now().GetSeconds() << "s client recieved " << packet->GetSize()
                 << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                 << InetSocketAddress::ConvertFrom(from).GetPort());

    uint32_t srcAddress = InetSocketAddress::ConvertFrom(from).GetIpv4().Get();
    ecs::packets::Message message = ParsePacket(packet);

    if(CheckDuplicateMessage(message.id())) {
      NS_LOG_INFO("already recieved this message, dropping.");
      stats.incDuplicate();
      return;
    }
    if(message.hasPing()) {
      //ping received
      stats.incReceived(Stats::Type::PING);
      HandlePing(srcAddress, message.ping().delivery_probability());
    } else if(message.hasClaim()) {
      //CH claim received
      stats.incReceived(Stats::Type::Claim);
      HandleClaim(srcAddress)
    } else if(message.hasResponse()) {
      //response received
      stats.incReceived(Stats::Type::Response);
      HandleResponse(srcAddress,message.response().node_status());
    } else {
      stats.incReceived(Stats::Type::UKNOWN);
      std::cout << "handling message: other\n";
      NS_LOG_WARN("Unknown message type");
    }
  }
}
/**
Message handlers below. Above is sorting the messages from one another
**/
//Handles pings being received from another node (probably will be used to update information table)
void ecsClusterApp::HandlePing(uint32_t nodeID) {

}
//Handles another node sending a clusterhead claim. Follows the algorithm from the paper
void ecsClusterApp::HandleClaim(uint32_t nodeID) {

}
//Handles response from a given node, possibly have to fix the node_status to a better type later
void ecsClusterApp::HandleResponse(uint32_t nodeID, String node_status) {

}



void ecsClusterApp::ScheduleClusterFormationWatchdog() {
  if(m_state != State::RUNNING) return;
  m_cluster_watchdog_event = Simulator::Schedule(m_peer_timeout, &ecsClusterApp::ClusterFormation, this);
}
void ecsClusterApp::ClusterFormation() {
  //TODO: Write cluster formation step. Incorporate all nodes to have their node status inside
}

} //namespace ecs
