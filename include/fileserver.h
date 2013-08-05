//fileserver.h
#ifndef _fileserver_h
#define _fileserver_h

#include _common_h
#include "common.h"
#include _common_h

#include _server_h
#include "server.h"
#include _server_h

class fileserver:public server
{
public:
  fileserver(const uint16_t port, 
	     const char* ip = NULL):server(port, ip){};
  void requestHandler(int clfd);
  virtual ~fileserver(){};
private:
};
#endif
