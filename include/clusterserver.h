//clusterserver.h
#ifndef _clusterserver_h
#define _clusterserver_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _server_h
#include "server.h"
#endif

#ifndef _client_h
#include "client.h"
#endif

#include <jsoncpp/json.h>

/*
cluster min heap is designed for cluster manager
to keep track of the current smallest cluster,
so that when a new leveldb server request to join,
it will be assigned to this smallest cluster.
So for initial design, we want a min heap with
element=pair<cluster_id, cluster_size>, cluster_id
being the key. For now, we only need api changekey
*/
typedef std::pair<uint16_t, uint16_t> cluster_id_sz;
typedef struct cluster_min_heap cluster_min_heap;

class clusterserver : public server
{
public:
  clusterserver(const uint16_t port, const char* ip);
  virtual void requestHandler(int clfd);
  virtual ~clusterserver();
  pthread_t* get_thread_obj();
  std::vector<ip_port>& 
    get_server_list(const size_t cluster_id);
  Json::Value get_serialized_state();
  void join_cluster(std::string& joinip, uint16_t joinport);
private:

  struct cluster_min_heap
  { //keeps track of cluster id and its size dynamically
    cluster_min_heap();
    void changesz(uint16_t cluster_id, uint16_t cluster_sz);
    uint16_t get_min_cluster_id();
    std::vector<cluster_id_sz> heap;
    std::vector<int> cluster_heap_idx; //index for heap idx of a cluster
  };
  
  pthread_t thread_obj;

  //in-memory indexing data structures
  std::vector<std::vector<ip_port> >
    ctbl; //cluster table, maps cluster id to ldb server list
  cluster_min_heap cmh; //to dynamically find min cluster id
  std::map<ip_port, uint16_t> 
    ldbsvr_cluster_map; //ldbsvr table, maps ldbsvr to cluster id
  pthread_mutex_t idx_lock;

  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*);

  static void process_cluster_request(std::string& request,
				      std::string& response,
				      clusterserver* handle);

  uint16_t register_server(const std::string& ip, const uint16_t port);

  time_t timestamp;
  std::map<ip_port,bool> existing_cs_set;
  void broadcast_update_cluster_state(const ip_port& peeripport);
  void broadcast(const ip_port& exclude, const Json::Value& msg);
  void update_cluster_state(const Json::Value& root);
/*
//we might reuse the following code later, if we decide to maintain 
//cluster-to-cluster connection sockets.
//for now, we recreate connction for each cluster-to-cluster request.
  std::map<ip_port, client*> connection_tbl; //keep track of connections
                                            //THIS cs initiated.
  pthread_mutex_t conntbl_lock;
  void create_client(client*& cltp, const ip_port& ipport);
*/
};

#endif
