/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/// \file ecs-clustering-helper.cc
/// \author Keefer Rourke <krourke@uoguelph.ca>
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
/// This code was modified from Keefer Rourke's original code for the RHPMAN application
#include <algorithm>
#include <map>

#include "ns3/application-container.h"
#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/attribute.h"
#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/node-container.h"
#include "ns3/object-base.h"
#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/udp-socket-factory.h"

//#include "../model/logging.h"
//#include "../model/nsutil.h"
//#include "../model/util.h"
#include "ecs-clustering-helper.h"

namespace ecs {

using namespace ns3;

void ecsClusterAppHelper::SetAttribute(std::string name, const AttributeVal& value) {
  m_factory.Set(name,value);
}

ApplicationContainer ecsClusterAppHelper::Install(NodeContainer nodes) {
  ApplicationContainer apps;
  for(size_t i=0; i<nodes.GetN(), i++) {
    apps.Add(createAndInstallApp(nodes.Get(i)));
  }
  return apps;
}

ApplicationContainer ecsClusterAppHelper::Install(Ptr<Node> node) const {
  ApplicationContainer apps;
  apps.Add(createAndInstallApp(node));
  return apps;
}

Ptr<Application> ecsClusterAppHelper::createAndInstallApp(Ptr<Node> node) const {
  Ptr<Application> app = m_factory.Create<Application>();
  node->AddApplication(app);
  return app;
}
} // namespace ecs
