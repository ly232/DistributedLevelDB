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
  {
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    uint16_t cluster_id = cs->register_server(ip,port);
    cs->ldbsvr_cluster_map[ip_port(ip,port)] = cluster_id;
    root.clear();
    root["result"] = "ok";
    root["cluster_id"] = cluster_id;
  }
  else if (req_type=="leave")
  {
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
    std::list<ip_port>& sl = cs->get_server_list(hash(key));
    std::list<ip_port>::iterator it = sl.begin();
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

std::list<ip_port >& 
clusterserver::get_server_list(const size_t cluster_id)
{
  return ctbl[cluster_id];
}

pthread_t* clusterserver::get_thread_obj()
{
  return &thread_obj;
}
