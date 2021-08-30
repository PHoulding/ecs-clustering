/// \file simulation-params.cc
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

//This is a modified version of the original file provided by Keefer Rourke

#include <inttypes.h>
#include <cmath>
#include <utility>

#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/random-walk-2d-mobility-model.h"

#include "simulation-params.h"
#include "util.h"

namespace ecs {

std::pair<SimulationParameters, bool> SimulationParameters::parse(int argc, char* argv[]) {
  /* Default simulation values. */
  // Simulation run time.
  double optRuntime = 2.0_minutes;

  double optWaitTime = 30.0_seconds;

  // Simulation seed.
  uint32_t optSeed = 1;

  // Node parameters.
  uint32_t optTotalNodes = 160;

  // Simulation area parameters.
  double optAreaWidth = 1000.0_meters;
  double optAreaLength = 1000.0_meters;


  // Traveller mobility model parameters.
  double optTravellerVelocity = 20.0_mps;

  // Traveller random 2d walk mobility model parameters.
  // Note: Shi and Chen do not specify any parameters of their random walk
  //   mobility models.
  double optTravellerWalkDistance = 0.0_meters;
  double optTravellerWalkTime = 30.0_seconds;
  std::string optTravellerWalkMode = "distance";


  // Link and network parameters.
  std::string optRoutingProtocol = "dsdv";
  double optWifiRadius = 100.0_meters;

  bool optStaggeredStart = false;
  double optStaggeredVariance = 4.0;

  // Animation parameters.
  std::string animationTraceFilePath = "ecs.xml";
  /* Setup commandline option for each simulation parameter. */
  CommandLine cmd;
  cmd.AddValue("runTime", "Simulation run time in seconds", optRuntime);
  cmd.AddValue("totalNodes", "Total number of nodes in the simulation", optTotalNodes);
  cmd.AddValue(
      "staggeredStart",
      "If the application starting should be staggered across the different nodes",
      optStaggeredStart);
  cmd.AddValue(
      "staggeredStartVariance",
      "The variance used to generate the application startup variance",
      optStaggeredVariance);
  cmd.AddValue("partitionNodes", "The number of nodes placed per partition", optNodesPerPartition);
  cmd.AddValue(
    "hops",
    "The number of hops to consider in the neighborhood of a node",
    optNeighborhoodSize);
  cmd.AddValue(
      "forwardingThreshold",
      "The delivery probability threshold for a node to forward data",
      optForwardingThreshold);
  cmd.AddValue("areaWidth", "Width of the simulation area in meters", optAreaWidth);
  cmd.AddValue("areaLength", "Length of the simulation area in meters", optAreaLength);
  cmd.AddValue("travellerVelocity", "Velocity of traveller nodes in m/s", optTravellerVelocity);
  cmd.AddValue(
      "travellerWalkDist",
      "The distance in meters that traveller walks before changing "
      "directions",
      optTravellerWalkDistance);
  cmd.AddValue(
      "travellerWalkTime",
      "The time in seconds that should pass before a traveller changes "
      "directions",
      optTravellerWalkTime);
  cmd.AddValue(
      "travellerWalkMode",
      "Should a traveller change direction after distance walked or time "
      "passed; options are 'distance' or 'time' ",
      optTravellerWalkMode);
  cmd.AddValue(
      "electionPeriod",
      "The number of seconds to wait before checking the results of an election",
      optElectionPeriod);
  cmd.AddValue(
      "requestTimeout",
      "The number of seconds to wait before marking a lookup as failed",
      optRequestTimeout);
  cmd.AddValue("routing", "One of either 'DSDV' or 'AODV'", optRoutingProtocol);
  cmd.AddValue("wifiRadius", "The radius of connectivity for each node in meters", optWifiRadius);
  // cmd.AddValue("animationXml", "Output file path for NetAnim trace file",
  // animationTraceFilePath);
  cmd.Parse(argc, argv);

  /* Parse the parameters. */
  bool ok = true;
  SimulationParameters result;
  if (optRuntime < 0) {
      std::cerr << "Runtime (" << optRuntime << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optTotalNodes < 0) {
      std::cerr << "Total number of nodes (" << optTotalNodes << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optStaggeredVariance < 0) {
    std::cerr << "Startup variance can not be negative (" << optStaggeredVariance << ")"
              << std::endl;
    return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optForwardingThreshold < 0 || optForwardingThreshold > 1) {
    std::cerr << "Forwarding Threshold (" << optForwardingThreshold << ") is not a probability" << std::endl;
    return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optAreaWidth < 0) {
      std::cerr << "Area width (" << optAreaWidth << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optAreaLength < 0) {
      std::cerr << "Area length (" << optAreaWidth << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optTravellerVelocity < 0) {
      std::cerr << "Traveller velocity (" << optTravellerVelocity << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }
  if (optTravellerWalkTime < 0) {
      std::cerr << "Traveller walk time (" << optTravellerWalkTime << ") is negative"
                << std::endl;
      return std::pair<SimulationParameters, bool>(result, false);
  }

  RandomWalk2dMobilityModel::Mode travellerWalkMode;
  std::tie(travellerWalkMode, ok) = getWalkMode(optTravellerWalkMode);
  if(!ok) {
    std::cerr << "Unrecognized Walk mode '" + optTravellerWalkMode + "'." << std::endl;
  }
  if(!optTravellerWalkDistance) {
    optTravellerWalkDistance = std::min(optAreaWidth, optAreaLength);
  }
  RoutingType routingType = getRoutingType(optRoutingProtocol);
  if(routingType == RoutingType::UNKNOWN) {
    std::cerr << "Unrecognized routing type '" + optRoutingProtocol + "'." << std::endl;
    return std::pair<SimulationParameters, bool>(result, false);
  }

  Ptr<ConstantRandomVariable> travellerVelocityGenerator = CreateObject<ConstantRandomVariable>();
  travellerVelocityGenerator->SetAttribute("Constant", DoubleValue(optTravellerVelocity));

  Ptr<UniformRandomVariable> pbnVelocityGenerator = CreateObject<UniformRandomVariable>();
  pbnVelocityGenerator->SetAttribute("Min", DoubleValue(optPbnVelocityMin));
  pbnVelocityGenerator->SetAttribute("Max", DoubleValue(optPbnVelocityMax));


  result.seed = optSeed;
  result.runtime = Seconds(optRuntime);
  result.area = SimulationArea(
      std::pair<double, double>(0.0, 0.0),
      std::pair<double, double>(optAreaWidth, optAreaLength));
  result.rows = optRows;''
  result.cols = optCols;

  result.totalNodes = optTotalNodes;
  result.dataOwners = std::round(optTotalNodes * (optPercentageDataOwners / 100.0));

  result.travellerNodes = optTotalNodes - (optNodesPerPartition * (optRows * optCols));
  result.travellerVelocity = travellerVelocityGenerator;
  result.travellerDirectionChangePeriod = Seconds(optTravellerWalkTime);
  result.travellerDirectionChangeDistance = optTravellerWalkDistance;
  result.travellerWalkMode = travellerWalkMode;

  result.pbnVelocity = pbnVelocityGenerator;
  result.pbnVelocityChangePeriod = Seconds(optPbnVelocityChangeAfter);

  result.routingProtocol = routingType;
  result.wifiRadius = optWifiRadius;

  result.staggeredStart = optStaggeredStart;
  result.staggeredVariance = optStaggeredVariance;

  result.netanimTraceFilePath = animationTraceFilePath;

  return std::pair<SimulationParameters, bool>(result, ok);

}
} //namespace ecs
