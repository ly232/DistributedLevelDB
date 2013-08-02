//client.h
#ifndef _client_h
#define _client_h
#include "common.h"
class client
{
public:
  client(const char* remote_ip, const uint16_t remote_port);
  ~client();
  void sendfile(const char* fname);
private:
  int socket_fd;
  struct sockaddr_in svaddr; //client address
  char buf[BUF_SIZE];
};
#endif
