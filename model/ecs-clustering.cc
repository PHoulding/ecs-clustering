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


#include "ecs-clustering.h"

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
        Node_Status::CLUSTER_GUEST, "Node_Status::CLUSTER_GUEST"))
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

  ScheduleClusterFormationWatchdog();
}
//override
void ecsClusterApp::StopApplication() {

}

void ecsClusterApp::ScheduleClusterFormationWatchdog() {
  if(m_state != State::RUNNING) return;
  m_cluster_watchdog_event = Simulator::Schedule(m_peer_timeout, &ecsClusterApp::ClusterFormation, this);
}
void ecsClusterApp::ClusterFormation() {
  //TODO: Write cluster formation step. Incorporate all nodes to have their node status inside
}

} //namespace ecs
