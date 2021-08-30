/// \file simulation-params.h
/// \author Keefer Rourke <mail@krourke.org>
/// \brief All simulation parameters are declared in this file.
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

//This code was modified from Keefer Rourke's original code for RHPMAN
#ifndef __simulation_params_h
#define __simulation_params_h

#include <inttypes.h>
#include <utility>

#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/random-walk-2d-mobility-model.h"

#include "nsutil.h"
#include "simulation-area.h"
#include "util.h"

namespace ecs {

class SimulationParameters {
public:
/// Simulation RNG seed.
uint32_t seed;
/// Simulation runtime.
ns3::Time runtime;
/// Total nodes used in the simulation.
uint32_t totalNodes;
/// Number of nodes per partition.
uint32_t nodesPerPartition;
/// Number of traveller nodes (computed).
uint32_t travellerNodes;
/// The number of hops defining the neighborhood considered for a replicating
uint8_t neighborhoodSize;
/// The simulation area.
SimulationArea area;
/// The velocity of the travellers.
ns3::Ptr<ns3::ConstantRandomVariable> travellerVelocity;
/// The period after which traveller nodes should change their direction if
/// the travellerWalkMode is MODE_TIME.
ns3::Time travellerDirectionChangePeriod;

ns3::Time updateTime;
ns3::Time lookupTime;
ns3::Time waitTime;

bool staggeredStart;
double staggeredVariance;

/// The distance after which traveller nodes should change their direction if
/// the travellerWalkMode is MODE_DISTANCE.
double travellerDirectionChangeDistance;
/// Governs the behaviour of the traveller nodes' walking.
ns3::RandomWalk2dMobilityModel::Mode travellerWalkMode;
/// The velocity of the partition-bound nodes.
ns3::Ptr<ns3::UniformRandomVariable> pbnVelocity;
/// The period after which partition-bound nodes change velocity.
ns3::Time pbnVelocityChangePeriod;
/// Indicates the type of routing to use for the simulation.
rhpman::RoutingType routingProtocol;
/// The radius of connectivity for each node.
double wifiRadius;
/// The path on disk to output the NetAnim trace XML file for visualizing the
/// results of the simulation.
std::string netanimTraceFilePath;

SimulationParameters() {}

/// \brief Parses command line options to set simulation parameters.
///
/// \param argc The number of command line options passed to the program.
/// \param argv The raw program arguments.
/// \return std::pair<SimulationParameters, bool>
///   If the boolean value is false, there was an error calling the program,
///   and so the construction of the simulation parameters does not make sense.
///   If it is true, construction succeeded and the simulation may run.
///
static std::pair<SimulationParameters, bool> parse(int argc, char* argv[]);
};

}
