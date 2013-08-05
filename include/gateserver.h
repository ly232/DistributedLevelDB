//gateserver.h
#ifndef _gateserver_h
#define _gateserver_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _server_h
#include "server.h"
#endif

class gateserver : public server
{
public:
  gateserver(const uint16_t port, 
	     const char* ip = NULL):server(port, ip){};
  virtual void requestHandler(int clfd);
  virtual ~gateserver(){};
private:
  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*);
};

#endif
