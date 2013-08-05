//client.h
#ifndef _client_h
#define _client_h

#ifndef _common_h
#include "common.h"
#endif

class client
{
public:
  client(const char* remote_ip, const uint16_t remote_port);
  ~client();
  void sendfile(const char* fname);
  std::string sendstring(const char* str);
private:
  bool goodconn;
  int socket_fd;
  struct sockaddr_in svaddr; //client address
  //thread initialization callback routines
  static void* send_thread(void* arg);
  static void* recv_thread(void* arg);
};
#endif
