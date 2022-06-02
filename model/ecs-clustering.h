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

#include "table.h"
#include "ecs-stats.h"

namespace ecs {

using namespace ns3;

class ecsClusterApp : public Application {
  public:
    //unspec = 0, ch = 1, cm = 2, cgw = 3, sa = 4, cg = 5
    enum class Node_Status { UNSPECIFIED, CLUSTER_HEAD,
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

    static void CleanUp();


//local based vars & functions
  private:
    void StartApplication() override;
    void StopApplication() override;

    void SetStatus(Node_Status status);

    State m_state;
    Node_Status m_node_status;
    uint32_t m_neighborhoodHops;
    Time m_profileDelay;
    Time m_standoff_time;
    Time random_m_standoff_time;
    Time m_inquiry_timeout;
    Time m_waitTime;

    Ptr<Socket> m_socket_recv;
    Ptr<Socket> m_neighborhood_socket;
    Ptr<Socket> m_election_socket;

    //helpers
    uint32_t GetID();
    Ptr<Socket> SetupRcvSocket(uint16_t port);
    Ptr<Socket> SetupSendSocket(uint16_t port, uint8_t ttl);
    Ptr<Socket> SetupSocket(uint16_t port, uint32_t ttl);
    void DestroySocket(Ptr<Socket> socket);

    void SendToNodes(Ptr<Packet> message, const std::set<uint32_t> nodes);

    Ptr<Packet> GeneratePing(uint8_t node_status);
    Ptr<Packet> GenerateStatus(uint8_t node_status);
    Ptr<Packet> GenerateClusterHeadClaim();
    Ptr<Packet> GenerateMeeting();
    Ptr<Packet> GenerateResponse(uint64_t responseTo);
    Ptr<Packet> GenerateResign(uint8_t node_status);
    Ptr<Packet> GenerateInquiry();

    // EventId m_election_watchdog_event;
    // EventId m_replica_announcement_event;
    EventId m_ping_event;
    // EventId m_election_results_event;
    EventId m_table_update_event;
    EventId m_CH_claim_event;
    EventId m_inquiry_event;
    EventId m_check_CHResign_event;
    EventId m_print_table_event;


    void BroadcastToNeighbors(Ptr<Packet> packet);
    void SendMessage(Ipv4Address dest, Ptr<Packet> packet);
    void SendPing(uint8_t node_status);
    void SendResponse(uint64_t requestID, uint32_t nodeID);
    void SendClusterHeadClaim();
    void SendStatus(uint32_t nodeID);
    void SendCHMeeting(uint32_t nodeID);
    void SendResign(uint8_t node_status);
    void SendInquiry();

    void SchedulePing();
    void ScheduleWakeup();
    void ScheduleClusterHeadClaim();
    void ScheduleInquiry();
    void ScheduleRefreshRoutingTable();
    void SchedulePrintInformationTable();
    void ScheduleHeadPrintTable();
    void ScheduleClusterHeadCount();

    void HandleRequest(Ptr<Socket> socket);
    void HandlePing(uint32_t nodeID, uint8_t node_status);
    void HandleClaim(uint32_t nodeID);
    void HandleResponse(uint32_t nodeID, uint8_t node_status);
    //create getNeighborhoodSize method
    void HandleMeeting(uint32_t nodeID, uint8_t node_status, uint64_t neighborhood_size);
    void HandleCHResign(uint32_t nodeID, uint8_t node_status);
    void HandleInquiry(uint32_t nodeID, uint8_t node_status);
    void HandleStatus(uint32_t nodeId, uint8_t node_status);

    bool CheckDuplicateMessage(uint64_t messageID);

    uint8_t GenerateNodeStatusToUint();
    Node_Status GenerateStatusFromUint(uint8_t status);
    uint8_t NodeStatusToUintFromTable(Node_Status status);
    uint64_t GenerateMessageID();
    std::string NodeStatusToStringFromTable(Node_Status status);


    void ScheduleClusterFormationWatchdog();
    void ClusterFormation();

    std::string GetRoutingTableString();
    void RefreshRoutingTable();
    void RefreshInformationTable();
    void CheckCHShouldResign();

    void CancelEventMap(std::map<uint64_t, EventId> events);
    void CancelEventMap(std::map<uint32_t, EventId> events);
    void PrintCustomClusterTable();


    uint32_t m_address;
    std::map<uint32_t, Node_Status> m_informationTable;

    std::set<uint64_t> m_received_messages;

    Table m_peerTable;

    bool m_CH_Claim_flag;

    Stats stats;

};
}; //namespace ecs

#endif /* ECS_CLUSTERING_H */
