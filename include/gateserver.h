//gateserver.h
#ifndef _gateserver_h
#define _gateserver_h
#include "common.h"
#include "server.h"
class gateserver : public server
{
public:
  gateserver(const uint16_t port, 
	     const char* ip = NULL):server(port, ip){};
  void requestHandler(int clfd);
  virtual ~gateserver(){};
private:

};
#endif
