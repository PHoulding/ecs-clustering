/// \file grid-divider.cc
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
///

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/mobility-model.h"
#include "ns3/rectangle.h"

#include "simulation-area.h"

namespace ecs {

ns3::Rectangle SimulationArea::asRectangle() const {
  return ns3::Rectangle(this->minX(), this->maxX(), this->minY(), this->maxY());
}

ns3::Ptr<ns3::RandomRectanglePositionAllocator> SimulationArea::
    getRandomRectanglePositionAllocator() const {
  ns3::Ptr<ns3::UniformRandomVariable> x = ns3::CreateObject<ns3::UniformRandomVariable>();
  x->SetAttribute("Min", ns3::DoubleValue(this->minX()));
  x->SetAttribute("Max", ns3::DoubleValue(this->maxX()));

  ns3::Ptr<ns3::UniformRandomVariable> y = ns3::CreateObject<ns3::UniformRandomVariable>();
  y->SetAttribute("Min", ns3::DoubleValue(this->minY()));
  y->SetAttribute("Max", ns3::DoubleValue(this->maxY()));

  auto alloc = ns3::CreateObject<ns3::RandomRectanglePositionAllocator>();
  alloc->SetX(x);
  alloc->SetY(y);
  return alloc;
}

std::string SimulationArea::toString() const {
  std::stringstream ss;
  ss << "{(" << m_min.first << "," << m_min.second << "),(" << m_max.first << "," << m_max.second
     << ")}";
  return ss.str();
}

}
