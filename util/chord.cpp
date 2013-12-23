//chord.cpp
#include "chord.h"
using namespace chordModuleNS;

size_t default_chordhash(const std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  size_t mod = 1 << m;
  for (int i=0; i<len; i++)
    val += ((size_t)s[i]+123);
  val %= mod;
  return val;
}

chordModule::chordModule(const ip_port& _selfAddr, 
                         const ip_port& _joinAddr,
                         size_t (*_hash)(const std::string&))
  : selfAddr(_selfAddr), hash(_hash), fingerTable(m)
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
//output: host peer address where key belongs to
void chordModule::lookup(const std::string& key, 
                         ip_port& hostpeer, 
                         const ip_port& requester)
{
  size_t targetHashVal = hash(key);
  size_t selfHashVal = hash(ipport2str(selfAddr));
  size_t nextHashVal = hash(ipport2str(nextAddr));
  Json::Value msg;
  Json::StyledWriter writer;
  std::string outputConfig;
  if (selfAddr==requester && targetHashVal==selfHashVal)
  {
    hostpeer = selfAddr;
    return;
  }
  if (targetHashVal>selfHashVal && targetHashVal<=nextHashVal)
  {
    //key belongs to successor
    if (requester==selfAddr)
    {
      hostpeer = nextAddr;
      return;
    }
    else
    {
      msg["req_type"] = "key_host_resp";
      msg["ip"] = nextAddr.first;
      msg["port"] = (int)nextAddr.second;
      outputConfig = writer.write(msg);
      client clt(requester.first.c_str(),requester.second);
      clt.sendstring(outputConfig.c_str());
    }
  }
  else
  {
    //consult local finger table to find max node < targetHashVal:
    std::vector<ftentry>::iterator ftitr = fingerTable.begin();
    std::vector<ftentry>::iterator previtr = ftitr;
    while (ftitr!=fingerTable.end())
    {
      uint16_t hashval = ftitr->first;
      if (hashval>=targetHashVal) break;
      previtr = ftitr;
      ftitr++;
    }
    ip_port& contactAddr = previtr->second;
    //forward lookup request to contactAddr:
    msg["req_type"] = "key_host_req";
    msg["key"] = key;
    msg["requester_ip"] = requester.first;
    msg["requester_port"] = (int)requester.second;
    outputConfig = writer.write(msg);
    client clt(contactAddr.first.c_str(),contactAddr.second);
    clt.sendstring(outputConfig.c_str());
  }
  if (selfAddr==requester)
  {
    //requester must be blocked to wait for response
    
  }
}

void chordModule::join(const ip_port& contact)
{
}

void chordModule::leave(const ip_port& contact)
{

}

void chordModule::processRequest(const std::string& req)
{

}
