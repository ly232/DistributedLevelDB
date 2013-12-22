//chord.cpp
#include "chord.h"
using namespace chordModuleNS;

size_t default_hash(const std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  size_t mod = 1 << m;
  for (int i=0; i<len; i++)
    val += (size_t)s[i];
  val %= mod;
  return val;
}

chordModule::chordModule(const ip_port& _selfAddr, 
                         const ip_port& _joinAddr,
                         size_t (*_hash)(const std::string&))
  : selfAddr(_selfAddr), joinAddr(_joinAddr), hash(_hash)
{
  if (!hash)
  {
    hash = default_chordhash;
  }
  join(_joinAddr);
}

chordModule::~chordModule()
{

}

//lookup in a chord circle (algorithm 20.21 in stanford db book)
//input: key
//output: hostpeer where key belongs to
void chordModule::lookup(const std::string& key, ip_port& hostpeer)
{
  size_t j = hash(key);
  
}

void chordModule::join(const ip_port& contact)
{
  std::string selfAddrStr;
  std::stringstream ss;
  ss<<selfAddr.second;
  ss>>selfAddrStr;
  selfAddrStr+=selfAddr.first;
  selfHashVal = hash(selfAddrStr);
}

void chordModule::leave(const ip_port& contact)
{

}

void chordModule::processRequest(const std::string& req)
{

}
