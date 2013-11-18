//clusterserver.h
#ifndef _clusterserver_h
#define _clusterserver_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _server_h
#include "server.h"
#endif

typedef std::pair<int,int> key_range

class clusterserver : public server
{
public:
  clusterserver(const uint16_t port, const char* ip);
  virtual void requestHandler(int clfd);
  virtual ~clusterserver();
  pthread_t* get_thread_obj();
  std::list<server_address>& 
    get_server_list(const size_t cluster_id);
private:
  //std::vector<std::list<server_address> > 
  //  ctbl; //cluster table, maps cluster id to server list
  std::map<key_range,std::list<server_address> > ctbl;
  pthread_t thread_obj;

  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*);

  static void process_cluster_request(std::string& request,
				      std::string& response,
				      clusterserver* handle);

  void register_server(const size_t cluster_id, 
		       const server_address& svr);

};

#endif
