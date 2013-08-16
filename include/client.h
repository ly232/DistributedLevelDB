//client.h
#ifndef _client_h
#define _client_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _syncobj_h
#include "syncobj.h"
#endif

class client
{
public:
  client(const char* remote_ip, const uint16_t remote_port);
  ~client();
  void sendfile(const char* fname);
  std::string sendstring(const char* str);
  void sendstring_noblock(const char* req, 
			  syncobj* so, 
			  int* numdone);
private:
  bool goodconn;
  int socket_fd;
  struct sockaddr_in svaddr; //client address
  //thread initialization callback routines
  static void* send_thread(void* arg);
  static void* recv_thread(void* arg);
  static void* main_thread(void* arg); //for async sendstring
};
#endif
