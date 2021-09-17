/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/// \file ecs-clustering-helper.h
/// \author Keefer Rourke <krourke@uoguelph.ca>
/// \brief Declarations for the a simulation that attempts to reproduce the
///     RHPMAN scheme and performance evaluation from Shi and Chen 2014.
///
///     *References*:
///      - K. Shi and H. Chen, "RHPMAN: Replication in highly partitioned mobile
///        ad hoc networks," International Journal of Distributed Sensor Networks
///        Jul. 2014.
///
/// Copyright (c) 2020 by Keefer Rourke <krourke@uoguelph.ca>
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

//This code was modified from Keefer Rourke's original code for RHPMAN in ns3

#ifndef ECS_CLUSTERING_HELPER_H
#define ECS_CLUSTERING_HELPER_H

#include <map>

#include "ns3/application-container.h"
#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/attribute.h"
#include "ns3/core-module.h"
#include "ns3/node-container.h"
#include "ns3/object-base.h"
#include "ns3/object-factory.h"
#include "ns3/socket.h"

#include "ns3/ecs-clustering.h"

namespace ecs {

using namespace ns3;

class ecsClusterAppHelper {
  public:
    ecsClusterAppHelper() {m_factory.SetTypeId(ecsClusterApp::GetTypeId()); };
    void SetAttribute(std::string name, const AttributeValue& value);
    void SetDataOwners(int32_t num); //possible remove this???

    /// \brief Configures an ECS application and installs it on each node.
    ApplicationContainer Install(NodeContainer nodes);
    ApplicationContainer Install(Ptr<Node> node) const;
    ApplicationContainer Install(std::string nodeName) const;

  private:
    Ptr<Application> createAndInstallApp(Ptr<Node> node) const;
    ObjectFactory m_factory;
};

/* ... */

}; // namespace ecs

#endif /* ECS_CLUSTERING_HELPER_H */
