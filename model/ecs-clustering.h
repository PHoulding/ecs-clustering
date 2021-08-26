/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ECS_CLUSTERING_H
#define ECS_CLUSTERING_H

#define APPLICATION_PORT 5000

namespace ecs {
using namespace ns3;

class ecsClusterApp : public Application {
  public:
    enum Node_Status { UNSPECIFIED, CLUSTER_HEAD,
                      CLUSTER_MEMBER, CLUSTER_GATEWAY,
                      STANDALONE, CLUSTER_GUEST };
    enum class State { NOT_STARTED = 0, RUNNING, STOPPED };


//local based vars & functions
  private:
    State m_state;
    Ptr<Socket> m_socket_recv;
    Ptr<Socket> m_neighborhood_socket;
    Ptr<Socket> m_election_socket;
}






private:


}

#endif /* ECS_CLUSTERING_H */
