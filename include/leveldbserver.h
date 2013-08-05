//leveldbserver.h
#ifndef _leveldb_h
#define _leveldb_h
#include "common.h"
#include "server.h"
class leveldbserver : public server
{
public:
  leveldbserver(const uint16_t port, 
	     const char* ip = NULL):server(port, ip){};
  virtual void requestHandler(int clfd);
  virtual ~leveldbserver(){};
private:
  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*); 
};
#endif

