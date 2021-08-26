/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/ecs-clustering-helper.h"

using namespace ns3;
using namespace ecs;

int
main (int argc, char *argv[])
{
  RngSeedManager::SetSeed(7);
  Time::SetResolution(Time::NS);

  SimulationParameters params;
  bool ok;
  std::tie(params, ok) = SimulationParameters::parse(argc,argv);




  /* ... */

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
