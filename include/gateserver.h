//gateserver.h
#ifndef _gateserver_h
#define _gateserver_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _server_h
#include "server.h"
#endif

#ifndef _clusterserver_h
#include "clusterserver.h"
#endif

class gateserver : public server
{
public:
  gateserver(const uint16_t gsport, 
	     const uint16_t csport,
	     const char* ip = NULL,
             bool master = false //master=true iff send heartbeat
            );
  virtual void requestHandler(int clfd);
  virtual ~gateserver();
  void setsync(){sync_client = true;};
  void setasync(){sync_client = false;};
  void join_cluster(std::string& joinip, uint16_t joinport);
private:
  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*);
  static void* cluster_server_init(void*);
  static void* cleanup_thread_handler(void*);
  clusterserver* cs;
  bool sync_client;
};

#endif
