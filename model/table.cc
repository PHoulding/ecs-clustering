/// \file table.cc
/// \author Marshall Asch <masch@uoguelph.ca>
///
/// Copyright (c) 2021 by Marshall Asch <masch@uoguelph.ca>
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
#include "table.h"
#include <math.h> /* isnan */

#include <ctype.h>
#include <iostream>
#include <sstream>  // std::istringstream

#include "ns3/ipv4-address.h"

namespace ecs {

static std::vector<std::string> split(const std::string str, char seprater) {
  std::vector<std::string> list;
  std::string tmp = str;

  size_t last = 0;
  size_t pos = 0;

  while ((pos = tmp.find(seprater, last)) != std::string::npos) {
    list.push_back(tmp.substr(last, (pos - last)));
    last = pos + 1;
  }

  return list;
}

static std::string trim(const std::string str) {
  std::string tmp = str;
  tmp.erase(tmp.find_last_not_of(" \n\r\t") + 1);
  tmp.erase(0, tmp.find_first_not_of(" \n\r\t"));

  return tmp;
}

static std::vector<std::string> trimStrings(const std::vector<std::string> strings) {
  std::vector<std::string> list;

  for (auto it = strings.begin(); it != strings.end(); ++it) {
    list.push_back(trim(*it));
  }

  return list;
}

static std::vector<std::string> filterStrings(const std::vector<std::string> strings) {
  std::vector<std::string> list;

  for (auto it = strings.begin(); it != strings.end(); ++it) {
    if ((*it).empty() || !isdigit((*it)[0])) {
      continue;
    }
    list.push_back((*it));
  }

  return list;
}

static std::vector<std::string> tokenize(const std::string str) {
  std::istringstream iss(str);
  std::vector<std::string> tokens;

  for (std::string s; iss >> s;) {
    tokens.push_back(s);
  }

  return tokens;
}

static bool isLoopback(const std::string address) { return address == "127.0.0.1"; }

// note this will only work in network with a mask of 255.255.0.0!!!!!!
static bool isBroadcast(const std::string address) {
  return address.find(".255.255") != std::string::npos;
}

static std::vector<std::string> getDsdvDestinations(
    const std::vector<std::string> enteries,
    uint32_t maxHops) {
  std::vector<std::string> list;

  for (auto entry : enteries) {
    std::vector<std::string> parts = tokenize(entry);

    uint32_t hops = (uint32_t)std::stoi(parts[3], nullptr, 10);

    if (isLoopback(parts[0]) || isBroadcast(parts[0])) continue;
    if (hops <= 0 || hops > maxHops) continue;

    list.push_back(parts[0]);
  }

  return list;
}

static std::vector<std::string> getAodvDestinations(
    const std::vector<std::string> enteries,
    uint32_t maxHops) {
  std::vector<std::string> list;

  for (auto entry : enteries) {
    std::vector<std::string> parts = tokenize(entry);

    uint32_t hops = (uint32_t)std::stoi(parts[5], nullptr, 10);

    if (parts[3] != "UP") continue;
    if (isLoopback(parts[0]) || isBroadcast(parts[0])) continue;
    if (hops <= 0 || hops > maxHops) continue;

    list.push_back(parts[0]);
  }

  return list;
}

static uint32_t convertIpv4(const std::string address) {
  return ns3::Ipv4Address(address.c_str()).Get();
}

static std::set<uint32_t> createSet(const std::vector<std::string> destinations) {
  std::set<uint32_t> table;

  for (auto entry : destinations) {
    table.insert(convertIpv4(entry));
  }

  return table;
}

static std::set<uint32_t> setUnion(const std::set<uint32_t> a, const std::set<uint32_t> b) {
  std::set<uint32_t> res;

  for (auto item : a) {
    res.insert(item);
  }

  for (auto item : b) {
    res.insert(item);
  }
  return res;
}

static std::set<uint32_t> setIntersection(const std::set<uint32_t> a, const std::set<uint32_t> b) {
  std::set<uint32_t> res;

  for (auto item : a) {
    if (b.find(item) != b.end()) res.insert(item);
  }

  return res;
}

std::set<uint32_t> Table::GetNeighbors(const std::string table, uint32_t maxHops) {
  std::vector<std::string> parts = split(table, '\n');
  std::vector<std::string> trimmed = trimStrings(parts);
  std::vector<std::string> filtered = filterStrings(trimmed);

  std::vector<std::string> destinations;

  if (table.find("AODV") != std::string::npos) {
    destinations = getAodvDestinations(filtered, maxHops);
  } else if (table.find("DSDV") != std::string::npos) {
    destinations = getDsdvDestinations(filtered, maxHops);
  }

  return createSet(destinations);
}

Table::Table() {
  numTables = 0;
  currentTable = 0;
  lastTable = 0;
  numTables = 0;
  maxHops = 0;
  tables.resize(0);
}

Table::Table(uint16_t num, uint32_t filter) {
  numTables = num;
  currentTable = 0;
  lastTable = 0;
  maxHops = filter;
  tables.resize(numTables);
}

void Table::nextTable() {
  currentTable = (currentTable + 1) % numTables;
  lastTable = (currentTable + 1) % numTables;
}

double Table::ComputeChangeDegree() const {
  // 0 if there are no tables to handle edge case
  if (numTables == 0 || currentTable >= numTables || lastTable >= numTables) return 0;

  uint16_t unionSize = setUnion(tables[currentTable], tables[lastTable]).size();
  uint16_t intersectSize = setIntersection(tables[currentTable], tables[lastTable]).size();

  double res = (unionSize - intersectSize) / (double)unionSize;
  return isnan(res) ? 0 : res;
}

void Table::UpdateTable(const std::string table) {
  nextTable();
  tables[currentTable] = Table::GetNeighbors(table, maxHops);

  // std::cout << "========================================\n" << table <<
  // "===========================================\n";
}

}  // namespace ecs
