//clusterserver.cpp
#include "include/clusterserver.h"
#include "include/client.h"
#include "include/syncobj.h"
#include <jsoncpp/json.h>
#include <algorithm>

static size_t hash(std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  for (int i=0; i<len; i++)
    val += (size_t)s[i];
  val %= MAX_CLUSTER;
  return val;
}

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

  if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
  if (write(clfd,resp,resp_len)!=resp_len) throw FILE_IO_ERROR;
  if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;

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
    if (byte_received < BUF_SIZE)
    {
      buf[byte_received] = '\0';
      done = true;
    }
    else if(buf[byte_received-1]=='\0')
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
    
    server_address sa(ip, port);
    cs->register_server(sa._hash, sa);
    
    root.clear();
    root["result"] = "ok";
  }
  else if (req_type=="leave")
  {
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    root.clear();
    server_address sa(ip,port);
    std::list<server_address>& sl = 
      cs->get_server_list(sa._hash);
    std::list<server_address>::iterator it = sl.begin();
    while (it!=sl.end())
    {
      if (*it==sa)
	break;
      it++;
    }
    if (it==sl.end()) //address is not on list...do nothing
      root["result"] = "";
    else
    {
      root["result"] = "ok";
      sl.erase(it);
    }
  }
  else if (req_type=="get_cluster_list")
  {
    std::string key = root["req_args"]["key"].asString();
    root.clear();
    std::list<server_address>& sl = cs->get_server_list(hash(key));
    std::list<server_address>::iterator it = sl.begin();
    int i = 0;
    while (it!=sl.end())
    {
      root["result"][i]["ip"] = it->_ip;
      root["result"][i]["port"] = (int) it->_port;
      it++;
    }
  }
  else
  {
    root["result"] = "";
  }
  response = writer.write(root);
}

void clusterserver::register_server(const size_t cluster_id, 
				    const server_address& svr)
{
  ctbl[cluster_id].push_back(svr);
}

std::list<server_address>& 
clusterserver::get_server_list(const size_t cluster_id)
{
  return ctbl[cluster_id];
}

pthread_t* clusterserver::get_thread_obj()
{
  return &thread_obj;
}
