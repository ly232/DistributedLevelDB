//chord.h
//module for managing p2p distributed hashing using chord algo
#ifndef _chord_h
#define _chord_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _client_h
#include "client.h"
#endif

#ifndef _server_h
#include "server.h"
#endif

#include <jsoncpp/json.h>

namespace chordModuleNS {

typedef std::pair<uint_16, ip_port> ftentry; //finger table entry,
                                             //<subscript, ip_port>

const uint16 m = 6; //2^m = max number of ldbsvr's allowed per cluster

class chordModule
{
public:
  chordModule(const ip_port& _selfAddr,
              const ip_port& _joinAddr,
              size_t (*_hash)(const std::string&) = NULL);
  ~chordModule();
  void lookup(const std::string& key, ip_port& hostpeer);
  void processRequest(const std::string& req);
private:
  void join(const ip_port& contact);
  void leave(const ip_port& contact);
  ip_port selfAddr;
  ip_port prevAddr;
  ip_port nextAddr;
  std::map<uint_16, ftentry> fingerTable;
  size_t (*hash)(const std::string&);
  size_t selfHashVal;
};

} //end namespace chordModuleNS

#endif
