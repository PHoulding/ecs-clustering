
//#include <stdio.h>
//#include <stdlib.h>
#include "table.h"

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

  for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); ++it) {
    list.push_back(trim(*it));
  }

  return list;
}

static std::vector<std::string> filterStrings(const std::vector<std::string> strings) {
  std::vector<std::string> list;

  for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); ++it) {
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

static std::vector<std::string> getDestinations(
    const std::vector<std::string> strings,
    uint32_t maxHops) {
  std::vector<std::string> list;

  for (std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); ++it) {
    std::vector<std::string> parts = tokenize(*it);

    uint32_t hops = (uint32_t)std::stoi(parts[3], nullptr, 10);

    if (hops > 0 && hops <= maxHops) list.push_back(parts[0]);
  }

  return list;
}

static uint32_t convertIpv4(const std::string address) {
  return ns3::Ipv4Address(address.c_str()).Get();
}

static std::set<uint32_t> createSet(const std::vector<std::string> destinations) {
  std::set<uint32_t> table;

  for (std::vector<std::string>::const_iterator it = destinations.begin(); it != destinations.end();
       ++it) {
    table.insert(convertIpv4(*it));
  }

  return table;
}

static std::set<uint32_t> setUnion(const std::set<uint32_t> a, const std::set<uint32_t> b) {
  std::set<uint32_t> res;

  for (std::set<uint32_t>::const_iterator it = a.begin(); it != a.end(); ++it) {
    res.insert(*it);
  }

  for (std::set<uint32_t>::const_iterator it = b.begin(); it != b.end(); ++it) {
    res.insert(*it);
  }
  return res;
}

static std::set<uint32_t> setIntersection(const std::set<uint32_t> a, const std::set<uint32_t> b) {
  std::set<uint32_t> res;

  for (std::set<uint32_t>::const_iterator it = a.begin(); it != a.end(); ++it) {
    if (b.find(*it) != b.end()) res.insert(*it);
  }

  return res;
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

Table::~Table() {}

void Table::nextTable() {
  lastTable = currentTable;
  currentTable = (currentTable + 1) % numTables;
}

double Table::ComputeChangeDegree() {
  uint16_t unionSize = setUnion(tables[currentTable], tables[lastTable]).size();
  uint16_t intersectSize = setIntersection(tables[currentTable], tables[lastTable]).size();

  return (unionSize - intersectSize) / (double)unionSize;
}

void Table::UpdateTable(const std::string table) {
  std::vector<std::string> parts = split(table, '\n');
  std::vector<std::string> trimmed = trimStrings(parts);
  std::vector<std::string> filtered = filterStrings(trimmed);

  std::set<uint32_t> currentNeighbors = createSet(getDestinations(filtered, maxHops));

  nextTable();
  tables[currentTable] = currentNeighbors;
}

}  // namespace ecs
