/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ECS_CLUSTERING_H
#define ECS_CLUSTERING_H

#define APPLICATION_PORT 5000

#include <map>
#include <set> //std::set

#include "ns3/application-container.h"
#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/attribute.h"
#include "ns3/callback.h"
//#include "ns3/core-module.h"
#include "ns3/event-id.h"
#include "ns3/node-container.h"
#include "ns3/object-base.h"
#include "ns3/object-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

namespace ecs {

using namespace ns3;

class ecsClusterApp : public Application {
  public:
    //unspec = 0, ch = 1, cm = 2, cgw = 3, sa = 4, cg = 5
    enum Node_Status { UNSPECIFIED, CLUSTER_HEAD,
                      CLUSTER_MEMBER, CLUSTER_GATEWAY,
                      STANDALONE, CLUSTER_GUEST };
    enum class State { NOT_STARTED = 0, RUNNING, STOPPED };

    static TypeId GetTypeId();
    ecsClusterApp()
      : m_state(State::NOT_STARTED),
        m_node_status(Node_Status::UNSPECIFIED),
        m_neighborhoodHops(1){};


    //implement these two
    Node_Status GetStatus() const;
    State GetState() const;


//local based vars & functions
  private:
    void StartApplication() override;
    void StopApplication() override;

    void SetStatus(NodeStatus status);

    State m_state;
    Ptr<Socket> m_socket_recv;
    Ptr<Socket> m_neighborhood_socket;
    Ptr<Socket> m_election_socket;

    Node_Status m_node_status
    std::map<uint32_t, uint8_t> m_informationTable;

    //helpers
    uint32_t GetID();
    Ptr<Socket> SetupRcvSocket(uint16_t port);
    Ptr<Socket> SetupSendSocket(uint16_t port, uint8_t ttl);
    Ptr<Socket> SetupSocket(uint16_t port, uint32_t ttl);
    void DestroySocket(Ptr<Socket> socket);

    void SendToNodes(Ptr<Packet> message, const std::set<uint32_t> nodes);

    Ptr<Packet> GeneratePing(uint8_t node_status);
    Ptr<Packet> GenerateClusterHeadClaim();
    Ptr<Packet> GenerateMeeting();
    Ptr<Packet> GenerateResponse(uint64_t responseTo);

    void BroadcastToNeighbors(Ptr<Packet> packet);
    void SendMessage(Ipv4Address dest, Ptr<Packet> packet);
    void SendPing(uint8_t node_status);
    void SendResponse(uint64_t requestID, uint32_t nodeID);
    void SendClusterHeadClaim();
    void SendStatus(uint32_t nodeID);
    void SendCHMeeting(uint32_t nodeID);

    void SchedulePing();
    void ScheduleClusterHeadClaim();

    void HandleRequest(Ptr<Socket> socket);
    void HandlePing(uint32_t nodeID, uint8_t node_status);
    void HandleClaim(uint32_t nodeID);
    void HandleResponse(uint32_t nodeID, uint8_t node_status);
    //create getNeighborhoodSize method
    void HandleMeeting(uint32_t nodeID, uint8_t node_status, uint64_t neighborhood_size);
    void HandleCHResign(uint32_t nodeID);
    void HandleInquiry(uint32_t nodeID, uint8_t node_status);
    void HandleStatus(uint32_t nodeId, uint8_t node_status);

    uint8_t generateNodeStatusToUint();

    void ScheduleClusterFormationWatchdog();
    void ClusterFormation();

};
}; //namespace ecs

#endif /* ECS_CLUSTERING_H */
