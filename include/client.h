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
  void sendstring(const char* str);
private:
  int socket_fd;
  struct sockaddr_in svaddr; //client address

  //thread initialization routines
  static void* send_thread(void* arg);
  static void* recv_thread(void* arg);
  pthread_mutex_t mutex;
};
#endif
