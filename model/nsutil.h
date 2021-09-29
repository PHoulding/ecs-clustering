/// \file nsutil.h
/// \author Keefer Rourke <krourke@uoguelph.ca>
/// \brief A collection of methods that make it bit easier to work with some
///     parts of ns-3.
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

#ifndef __nsutil_h
#define __nsutil_h

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "ns3/aodv-helper.h"
//#include "ns3/core-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/random-walk-2d-mobility-model.h"

namespace ecs {

using namespace ns3;

/// \brief User defined literal for time values in seconds.
ns3::Time operator"" _sec(const long double seconds);

/// \brief User defined literal for time values in minutes.
ns3::Time operator"" _min(const long double minutes);

/// \brief Routing type to use for the simulation.
///   Supported values are DSDV and AODV.
enum class RoutingType { DSDV, AODV, UNKNOWN };

/// \brief Get the Routing Type enum from a string.
///
/// \param str The string to parse.
/// \return RoutingType The RoutingType enum defining the routing to use for
///   for the simulation.
RoutingType getRoutingType(std::string str);

/// \brief Parses a RandomWalk2dMobilityModel::Mode from a string.
///
/// \param str The string to parse.
/// \return std::pair<ns3::RandomWalk2dMobilityModel::Mode, bool>
///   where the first value is the result, and the second is a boolean indicating
///   success or failure. On failure, the second part of the returned pair will be false.
std::pair<ns3::RandomWalk2dMobilityModel::Mode, bool> getWalkMode(std::string str);

};  // namespace ecs

#endif
