/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <sysexits.h>

#include "ns3/animation-interface.h"
#include "ns3/aodv-helper.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/double.h"
//#include "ns3/dsdv-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/netanim-module.h"
#include "ns3/node-container.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-standards.h"
#include "ns3/yans-wifi-helper.h"

#include "ns3/core-module.h"
#include "ns3/ecs-clustering-helper.h"
#include "simulation-params.h"
#include "ns3/ecs-clustering.h"
#include "ns3/ecs-stats.h"

using namespace ns3;
using namespace ecs;

NS_LOG_COMPONENT_DEFINE("EcsClusteringExample");

void setupTravellerNodes(const SimulationParameters& params, NodeContainer& nodes) {
  NS_LOG_UNCOND("Setting up traveller node mobility models...");
  NodeContainer travellers;
  travellers.Create(params.totalNodes);

  MobilityHelper travellerMobilityHelper;
  travellerMobilityHelper.SetPositionAllocator(params.area.getRandomRectanglePositionAllocator());
  travellerMobilityHelper.SetMobilityModel(
    "ns3::RandomWalk2dMobilityModel",
    "Bounds",
    RectangleValue(params.area.asRectangle()),
    "Speed",
    PointerValue(params.travellerVelocity),
    "Distance",
    DoubleValue(params.travellerDirectionChangeDistance),
    "Time",
    TimeValue(params.travellerDirectionChangePeriod),
    "Mode",
    EnumValue(params.travellerWalkMode));

  travellerMobilityHelper.Install(travellers);
  nodes.Add(travellers);
}

void resetStats(Stats stats) {
  stats.Reset();
}

int
main (int argc, char *argv[])
{
  RngSeedManager::SetSeed(7);
  Time::SetResolution(Time::NS);

  SimulationParameters params;
  bool ok;
  std::tie(params, ok) = SimulationParameters::parse(argc,argv);

  if(!ok) {
    std::cerr << "Error parsing the parameters. \n";
    return -1;
  }

  /* Create nodes, network topology, and start simulation. */
  NodeContainer allAdHocNodes;
  //allAdHocNodes.Create(params.totalNodes);
  NS_LOG_UNCOND("Simulation running over area: " << params.area);
  // Set up the traveller nodes.
  // Travellers can move across the whole simulation space.
  setupTravellerNodes(params, allAdHocNodes);

  NS_LOG_UNCOND("Setting up wireless devices for all nodes...");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);


  auto wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  // Yu and Chong refer to a 250m radius of connectivity for each node.
  // They do not assume any propagation loss model, so we use a constant
  // propagation loss model which amounts to having connectivity withing the
  // radius, and having no connectivity outside the radius.
  wifiChannel.AddPropagationLoss(
      "ns3::RangePropagationLossModel",
      "MaxRange",
      DoubleValue(params.wifiRadius));
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  NS_LOG_UNCOND("Assigning MAC addresses in ad hoc mode...");
  NetDeviceContainer adhocDevices = wifi.Install(wifiPhy, wifiMac, allAdHocNodes);

  NS_LOG_UNCOND("Setting up Internet stacks...");
  InternetStackHelper internet;

  NS_LOG_DEBUG("Using AODV routing");
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);

  internet.Install(allAdHocNodes);
  Ipv4AddressHelper adhocAddresses;
  adhocAddresses.SetBase("10.1.0.0", "255.255.0.0");
  adhocAddresses.Assign(adhocDevices);

  ecsClusterAppHelper ecs;
  ecs.SetAttribute("NeighborhoodSize", UintegerValue(params.neighborhoodSize));
  ecs.SetAttribute("StandoffTime", TimeValue(params.standoffTime));
  ecs.SetAttribute("WaitTime", TimeValue(params.waitTime));

  ApplicationContainer ecsApps = ecs.Install(allAdHocNodes);
 
  ecsApps.Start(Seconds(0));
  ecsApps.Stop(params.runtime);

  Stats stats;
  Simulator::Schedule(params.waitTime, &resetStats, stats);

  NS_LOG_UNCOND("Running simulation for " << params.runtime.GetSeconds() << " seconds...");
  NS_LOG_UNCOND("params.runtime = " << params.runtime + 1.0_sec);
  NS_LOG_UNCOND("Max running time is " << Simulator::GetMaximumSimulationTime());
  NS_LOG_UNCOND("With " << params.totalNodes << " nodes");

  Simulator::Stop(params.runtime + 1.0_sec);

  Simulator::Run();
  NS_LOG_UNCOND("test time @ " << Simulator::Now());
  //std::cout << "test time @ " << Simulator::Now() << "\n";
  Simulator::Destroy();
  NS_LOG_UNCOND("Done.");
  //std::cout << "Done\n";
  stats.PrintMessageTotals();
  stats.PrintClusterAverage(params.seed, params.nodeSpeed, params.totalNodes);
  //stats.WriteFinalStats(params.runtime.GetSeconds()-1, params.totalNodes, params.nodeSpeed, params.seed);

  ecsClusterApp::CleanUp();

  return 0;
}
