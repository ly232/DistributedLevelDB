//clusterserver.cpp
#include "include/clusterserver.h"
#include "include/client.h"
#include "include/syncobj.h"
#include <jsoncpp/json.h>
#include <algorithm>

clusterserver::cluster_min_heap::cluster_min_heap()
{
  cluster_id_sz cis;
  heap.push_back(cis);
  for (int i=0; i<MAX_CLUSTER; i++){
    cis.first = i; //cluster id
    cis.second = 0; //cluster size
    heap.push_back(cis);
    cluster_heap_idx.push_back(i+1);
  }
};
void clusterserver::cluster_min_heap::changesz(uint16_t cluster_id, 
                                               uint16_t cluster_sz)
{
  int chi = cluster_heap_idx[cluster_id];
  assert(heap[chi].first==cluster_id);
  if (cluster_sz > heap[chi].second)
  { //increase size ==> percolate down
    heap[chi].second = cluster_sz;
    while (chi*2<heap.size())
    {
      int swapidx = chi*2;
      if (swapidx+1<heap.size()) 
      {
        if (heap[swapidx+1].second<heap[swapidx].second)
        {
          swapidx++;
        }
      }
      if (cluster_sz>heap[swapidx].second)
      { //swap elements:
        cluster_heap_idx[heap[swapidx].first] = chi;
        cluster_id_sz tmp = heap[chi];
        heap[chi] = heap[swapidx];
        heap[swapidx] = tmp;
        chi = swapidx;
      } 
      else break;
    }
  }
  else
  { //decrease size ==> percolate up
    heap[chi].second = cluster_sz;
    while (chi/2>1)
    {
      int swapidx = chi/2;
      if (cluster_sz<heap[swapidx].second)
      { //swap elements:
        cluster_heap_idx[heap[swapidx].first] = chi;
        cluster_id_sz tmp = heap[chi];
        heap[chi] = heap[swapidx];
        heap[swapidx] = tmp;
        chi = swapidx;
      } 
      else break;
    }
  }
  cluster_heap_idx[cluster_id] = chi;
};
uint16_t clusterserver::cluster_min_heap::get_min_cluster_id()
{
  return heap[1].first;
};

clusterserver::clusterserver(const uint16_t port, 
			     const char* ip)
  :server(port, ip),
   ctbl(MAX_CLUSTER)
{
  std::cout<<"starting cluster server with ip: "
	   <<std::string(ip)<<", port: "<<port<<std::endl;
}

clusterserver::~clusterserver()
{
  
}

void clusterserver::requestHandler(int clfd)
{
  pthread_t main_thread_obj;
  int* clfdptr = new int; //clfdptr will be deleted by main thread
  *clfdptr = clfd;
  std::vector<void*>* argv = new std::vector<void*>; 
    //argv will be deleted by main thread
  argv->push_back((void*)clfdptr);
  argv->push_back((void*)this);
  if(pthread_create(&main_thread_obj, 
		 0, 
		 &main_thread, 
		 (void*)argv)
     !=0)
    throw THREAD_ERROR;
}

void* clusterserver::main_thread(void* arg)
{
  std::vector<void*>* argv = (std::vector<void*>*)arg;

  int* clfdptr = (int*)((*argv)[0]);
  int clfd = *clfdptr;
  clusterserver* cs = (clusterserver*)((*argv)[1]);

    syncobj* so = new syncobj(2L, 2L, 1L);
    std::string* replymsg = new std::string;

    std::vector<void*> thread_arg;
    thread_arg.push_back((void*)&clfd);
    thread_arg.push_back((void*)so);
    thread_arg.push_back((void*)replymsg);
    thread_arg.push_back((void*)cs);
  
    if (pthread_create(&so->_thread_obj_arr[0], 
		       0, 
		       &recv_thread, 
		       (void*)&thread_arg)
	!=0)
      throw THREAD_ERROR;
    if (pthread_create(&so->_thread_obj_arr[1], 
		       0, 
		       &send_thread, 
		       (void*)&thread_arg)
	!=0)
      throw THREAD_ERROR;
    if (pthread_join(so->_thread_obj_arr[0], 0)!=0) //recv thread
      throw THREAD_ERROR;
    if (pthread_join(so->_thread_obj_arr[1], 0)!=0) //send thread
      throw THREAD_ERROR;

    delete so;
    delete replymsg;

  delete clfdptr;
  delete (std::vector<void*>*)arg;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

void* clusterserver::send_thread(void* arg)
{
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex
    = ((syncobj*)argv[1])->_mutex_arr[0];
  pthread_mutex_t& cv_mutex
    = ((syncobj*)argv[1])->_mutex_arr[1];
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  std::string& resp_str = (*(std::string*)argv[2]);
  //wait until recv thread finishes reception 
  //and get a response from leveldb server
  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  while(resp_str.empty())
    pthread_cond_wait(&cv, &cv_mutex);
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  const char* resp = resp_str.c_str();
  size_t resp_len = strlen(resp)+1;
  size_t rmsz = resp_len;
  size_t byte_sent = -1;
  while (rmsz>0)
  {
    if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
    byte_sent = write(clfd,resp,rmsz);
    if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;
    if (byte_sent<0) throw FILE_IO_ERROR;
    rmsz -= byte_sent;
    resp += byte_sent;
  }

  return 0;
}

//recv_thread can receive either:
//  1. request from leveldb to join cluster, or
//  2. request from gateserver to get cluster list give a search key
//in both cases, the request must be in json format.
void* clusterserver::recv_thread(void* arg)
{
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex 
    = ((syncobj*)argv[1])->_mutex_arr[0];
  int byte_received = -1;
  std::string request;
  char buf[BUF_SIZE];
  bool done = false;
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  pthread_mutex_t& cv_mutex = ((syncobj*)argv[1])->_mutex_arr[1];
  std::string* replymsg  = (std::string*)argv[2];
  clusterserver* cs = (clusterserver*)argv[3];
  while (!done)
  {
    memset(buf, 0, BUF_SIZE);
    if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
    byte_received=read(clfd, buf, BUF_SIZE);
    if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;
    if(buf[byte_received-1]=='\0')
      done = true;
    request = request + std::string(buf);
  }
  
  std::string response;
  process_cluster_request(request,response,cs);

  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  *replymsg = response;
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
  return 0;
}

void clusterserver::process_cluster_request(std::string& request,
					    std::string& response,
					    clusterserver* cs)
{
//std::cout<<"cluster request="<<request<<std::endl;
  Json::Value root;
  Json::Reader reader;
  Json::StyledWriter writer;
  
  if (!reader.parse(request,root))
  {
    root.clear();
    root["result"] = "";
    response = writer.write(root);
    return;
  }
  std::string req_type = root["req_type"].asString();
  std::transform(req_type.begin(), 
		 req_type.end(), 
		 req_type.begin(), 
		 ::tolower);
  if (req_type=="join")
  { //TODO: need sync protection on ctbl, cmh, and ldbsvr_cluster_map
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    uint16_t cluster_id = cs->register_server(ip,port);
    cs->ldbsvr_cluster_map[ip_port(ip,port)] = cluster_id;
    root.clear();
    root["result"] = "ok";
    root["cluster_id"] = cluster_id;
  }
  else if (req_type=="leave")
  { //TODO: need sync protection on ctbl, cmh, and ldbsvr_cluster_map
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    root.clear();
    ip_port ldbsvr_id = ip_port(ip,port);
    std::map<ip_port, uint16_t>::iterator mapitr = 
        cs->ldbsvr_cluster_map.find(ldbsvr_id);
    if (mapitr==cs->ldbsvr_cluster_map.end()) root["result"] = "";
    else
    {
      root["result"] = "ok";
      cs->ldbsvr_cluster_map.erase(mapitr);
    }
  }
  else if (req_type=="get_cluster_list")
  {
    std::string key = root["req_args"]["key"].asString();
    root.clear();
    std::vector<ip_port>& sl = cs->get_server_list(hash(key));
    std::vector<ip_port>::iterator it = sl.begin();
    int i = 0;
    while (it!=sl.end())
    {
      root["result"][i]["ip"] = it->first;
      root["result"][i]["port"] = (int) it->second;
      it++;
    }
  }
  else
  {
    root["result"] = "";
  }
  response = writer.write(root);
}

/*
assign given server ip/port to the smallest cluster. return cluster id.
*/
uint16_t clusterserver::register_server(const std::string& ip, 
                                        const uint16_t port)
{
  int cluster_id = cmh.get_min_cluster_id();
  ctbl[cluster_id].push_back(ip_port(ip,port));
  cmh.changesz(cluster_id,
                cmh.heap[cmh.cluster_heap_idx[cluster_id]].second+1);
  return cluster_id;
}

std::vector<ip_port >& 
clusterserver::get_server_list(const size_t cluster_id)
{
  return ctbl[cluster_id];
}

pthread_t* clusterserver::get_thread_obj()
{
  return &thread_obj;
}

Json::Value clusterserver::get_serialized_state()
{
  Json::Value result;
  //serialize cluster table:
  result["ctbl"] = Json::Value(Json::arrayValue);
  int ctbl_len = ctbl.size();
  for (int i=0; i<ctbl_len; i++)
  {
    result["ctbl"][i] = Json::Value(Json::arrayValue);
    int clst_len = ctbl[i].size();
    for (int j=0; j<clst_len; j++)
    {
      Json::Value val;
      val["ip"] = ctbl[i][j].first;
      val["port"] = ctbl[i][j].second;
      result["ctbl"][i].append(val);
    }
  }
  //serialize cluster min heap:
  result["cmh"]["heap"] = Json::Value(Json::arrayValue);
  result["cmh"]["cluster_heap_idx"] = Json::Value(Json::arrayValue);
  int sz = cmh.cluster_heap_idx.size();
  for (int i=0; i<sz; i++)
  {
    Json::Value val;
    val["cluster_id"] = cmh.heap[i+1].first;
    val["cluster_sz"] = cmh.heap[i+1].second;
    result["cmh"]["heap"].append(val);
    val.clear();
    val = cmh.cluster_heap_idx[i];
    result["cmh"]["cluster_heap_idx"].append(val);
  }
  //serialize leveldb server to cluster id table:
  result["ldbsvr_cluster_map"] = Json::Value(Json::arrayValue);
  std::map<ip_port, uint16_t>::iterator itr = ldbsvr_cluster_map.begin();
  while (itr!=ldbsvr_cluster_map.end())
  {
    Json::Value val;
    val["ip"] = itr->first.first;
    val["port"] = itr->first.second;
    val["cluster_id"] = itr->second;
    result["ldbsvr_cluster_map"].append(val);
    itr++;
  }
  return result;
}

/*
queries an existing gateway server about cluster server configuration,
then update itself.
i.e. deserialization routine with respect to get_serialized_state().
*/
void 
clusterserver::join_cluster(std::string& joinip, uint16_t joinport)
{
  client clt(joinip.c_str(), joinport);
  Json::Value root;
  root["req_type"] = "join_gateway";
  Json::StyledWriter writer;
  std::string outputConfig = writer.write(root);
  std::string reply = clt.sendstring(outputConfig.c_str());
  std::cout<<"join gateway cluster response: "<<reply<<std::endl;
  //reconstruct cluster:
  Json::Reader reader;
  root.clear();
  if (!reader.parse(reply,root))
  {
    std::cerr<<"failed to parse cluster server config response"
             <<std::endl;
    exit(1);
  }
  root = root["result"];
  //update ctbl: 
  //note we do not clear ctbl because it's always size MAX_CLUSTER
  int i = 0;
  for (Json::ValueIterator itr1 = root["ctbl"].begin();
       itr1!=root["ctbl"].end(); itr1++)
  {
    Json::Value& vec = *itr1;
    ctbl[i].clear();
    for (Json::ValueIterator itr2 = vec.begin();
         itr2 != vec.end(); itr2++)
    {
      ctbl[i].push_back(ip_port((*itr2)["ip"].asString(),
                                (*itr2)["port"].asUInt()));
    }
    i++;
  }
  //update cluster min heap:
  cmh.heap.clear();
  cmh.heap.push_back(cluster_id_sz());
  cmh.cluster_heap_idx.clear();
  for (Json::ValueIterator itr1 = root["cmh"]["heap"].begin();
       itr1!=root["cmh"]["heap"].end(); itr1++)
  {
    cmh.heap.push_back(cluster_id_sz((*itr1)["cluster_id"].asUInt(),
                                     (*itr1)["cluster_sz"].asUInt()));
  }
  Json::Value& chiref = root["cmh"]["cluster_heap_idx"];
  int chirefsz = chiref.size();
  for (i=0; i<chirefsz; i++)
  {
    cmh.cluster_heap_idx.push_back(chiref[i].asUInt());
  }
  //update ldbsvr cluster map:
  ldbsvr_cluster_map.clear();
  for (Json::ValueIterator itr1 = 
         root["ldbsvr_cluster_map"].begin();
       itr1!=root["ldbsvr_cluster_map"].end(); 
       itr1++)
  {
    cmh.heap.push_back(cluster_id_sz((*itr1)["cluster_id"].asUInt(),
                                     (*itr1)["cluster_sz"].asUInt()));
    ldbsvr_cluster_map.insert(
      std::pair<ip_port,uint16_t>(
        ip_port( (*itr1)["ip"].asString(),(*itr1)["port"].asUInt() ),
        (*itr1)["cluster_id"].asUInt()
                                 )
                             );
  }
  //end communication:
  root.clear();
  root["req_type"] = "exit";
  outputConfig = writer.write(root);
  reply = clt.sendstring(outputConfig.c_str());
  std::cout<<"reply="<<reply<<std::endl;
}
