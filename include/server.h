//server.h
//interface file for all server implementations
//all derived classes need to implement
//requestHandler()
#ifndef _server_h
#define _server_h
#include "common.h"
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
#endif
