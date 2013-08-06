//server.h
//interface file for all server implementations
//all derived classes need to implement
//requestHandler()
#ifndef _server_h
#define _server_h

#ifndef _common_h
#include "common.h"
#endif

class server
{
public:
  server(const uint16_t port, const char* ip = NULL);
  virtual ~server();
  int accept_conn();
  virtual void requestHandler(int clfd) = 0;
  uint16_t getport();
  std::string getip();
  std::string getsvrname();
  int getsockfd();
protected:
  int socket_fd;
  struct sockaddr_in svaddr; //server address
  char hostname[256];
private:
  server& operator=(server&);
  server(const server&);
  server();
};

//instead of using struct sockaddr_in, we simplify server address
//with just ip and port, so that it's easier to compute the hash
//for cluster management
struct server_address
{
  server_address(std::string ip, 
		 uint16_t port):
  _ip(ip),_port(port)
  {
    size_t len = ip.length();
    _hash = 0;
    for (int i=0; i<len; i++)
      _hash += (size_t)ip[i];
    char c = (port & 0xFFFF) >> 2;
    _hash += (int)c;
    c = (port & 0xFFFF) << 2;
    _hash += (int)c;
    _hash %= MAX_CLUSTER;
  };

  bool operator==(const server_address& rhs) const
  {
    if (&rhs==this)
      return true;
    return (rhs._ip==_ip)&&(rhs._port==_port);
  }

  std::string _ip;
  uint16_t _port;
  size_t _hash;

};
typedef struct server_address server_address;

#endif
