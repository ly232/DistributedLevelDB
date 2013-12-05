//clusterserver.cpp
#include "include/clusterserver.h"
#include "include/client.h"
#include "include/syncobj.h"
#include <jsoncpp/json.h>
#include <algorithm>

clusterserver* cshdl; //handle for static clusterserver methods

void clusterserver::heartbeat_handler(int signum)
{
  if (signum==SIGALRM)
  {
    std::cout<<"will send heartbeat"<<std::endl;
    Json::Value hbmsg; //dummy msg. ldbsvr does not explicitly reply.
    //heartbeat to leveldb servers:
    std::map<ip_port, uint16_t>::iterator ldbitr = 
      cshdl->ldbsvr_cluster_map.begin();
    std::map<ip_port, uint16_t>::iterator lcmend = 
      cshdl->ldbsvr_cluster_map.end();
    while (ldbitr!=lcmend)
    {
      client clt(ldbitr->first.first.c_str(),ldbitr->first.second);
      Json::StyledWriter writer;
      std::string outputConfig = writer.write(hbmsg);
      clt.sendstring(outputConfig.c_str());
      if (errno)
      {
        //peer did not respond. assume peer is dead, and notify other cs
        cshdl->ldbsvr_cluster_map.erase(ldbitr);
        time(&cshdl->timestamp);
        ip_port dummy_exclude;
        cshdl->broadcast_update_cluster_state(dummy_exclude);
        errno = 0; //clear error
      }
      ldbitr++;
    }
    //TODO: heartbeat to cluster servers:
  }
  alarm(HEARTBEAT_RATE);
}

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
			     const char* ip, bool master)
  :server(port, ip),
   ctbl(MAX_CLUSTER)
{
  cshdl = this;
  std::cout<<"starting cluster server with ip: "
	   <<std::string(ip)<<", port: "<<port<<std::endl;
  pthread_mutex_init(&idx_lock, NULL);
  //pthread_mutex_init(&conntbl_lock, NULL);
  existing_cs_set.insert(std::pair<ip_port,bool>(
    ip_port(getip(),getport()), true
  ));
  time(&timestamp);
  if (master)
  {
    //register alarm to send heartbeat
    alarm(HEARTBEAT_RATE);
    signal(SIGALRM, heartbeat_handler);
  }
}

clusterserver::~clusterserver()
{
  ip_port dummy;
  Json::Value msg;
  msg["req_type"] = "clusterserver_leave";
  msg["ip"] = getip();
  msg["port"] = getport();
  broadcast(dummy,msg);
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
  delete (std::vector<void*>*)arg;
  delete clfdptr;
  //NOTE: client socket descriptor is closed at send_thread.
  //      this is to avoid a race condition when a new request
  //      comes between end of send thread and socket close.
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
  //this is the end of main thread. so socket is no longer needed. close.
  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

//recv_thread can receive either:
//  1. request from leveldb to join cluster
//  2. request from gateserver to get cluster list give a search key
//  3. request from another clusterserver to join cluster
//in all cases, the request must be in json format.
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
  if (req_type=="leveldbserver_join")
  {
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    uint16_t cluster_id = cs->register_server(ip,port);
    time(&cs->timestamp);
    cs->ldbsvr_cluster_map[ip_port(ip,port)] = cluster_id;
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    root.clear();
    root["result"] = "ok";
    root["cluster_id"] = cluster_id;
    ip_port exclude; //empty exclude--broadcast to every cs about ldb
    cs->broadcast_update_cluster_state(exclude);
  }
  else if (req_type=="leveldbserver_leave")
  {
    std::string ip = root["req_args"]["ip"].asString();
    uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
    bool updateinfo = false;
    root.clear();
    ip_port ldbsvr_id = ip_port(ip,port);
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    std::map<ip_port, uint16_t>::iterator mapitr = 
        cs->ldbsvr_cluster_map.find(ldbsvr_id);
    if (mapitr==cs->ldbsvr_cluster_map.end()) root["result"] = "";
    else
    {
      root["result"] = "ok";
      cs->ctbl[mapitr->second].erase(
        std::find(cs->ctbl[mapitr->second].begin(),
                  cs->ctbl[mapitr->second].end(),
                  mapitr->first)
      );
      cs->cmh.changesz(mapitr->second,
        cs->cmh.heap[cs->cmh.cluster_heap_idx[mapitr->second]].second-1);
      cs->ldbsvr_cluster_map.erase(mapitr);
      time(&cs->timestamp);
      updateinfo = true;
    }
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    if (updateinfo)
    {
      ip_port exclude; //empty pair: don't exclude anyone
      cs->broadcast_update_cluster_state(exclude);
    }
  }
  else if (req_type=="get_cluster_list")
  {
    std::string key = root["req_args"]["key"].asString();
    root.clear();
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    std::vector<ip_port>& sl = cs->get_server_list(hash(key));
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    std::vector<ip_port>::iterator it = sl.begin();
    int i = 0;
    while (it!=sl.end())
    {
      root["result"][i]["ip"] = it->first;
      root["result"][i]["port"] = (int) it->second;
      it++;
    }
  }
  else if (req_type=="broadcast_update_cluster_state")
  {
    time_t remote_ts = 
      static_cast<time_t>(root["req_args"]["timestamp"].asDouble());
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    if (difftime(remote_ts,cs->timestamp)>0)
    {
      cs->update_cluster_state(root["req_args"]);
    }
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    root.clear();
    root["status"] = "ok";
  }
  else if (req_type=="clusterserver_join")
  { //a new cluster server requests to join cluster.
    //update existing cluster server set by adding remote cs to set:
    ip_port peeripport(root["ip"].asString(), root["port"].asUInt());
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    cs->existing_cs_set[peeripport] = true;
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    //now process request:
    Json::StyledWriter writer;
    root.clear();
    root["status"] = "OK";
    root["result"] = cs->get_serialized_state();
    //broadcast to all clusterservers about the newly joined cs:
    cs->broadcast_update_cluster_state(peeripport);
  }
  else if (req_type=="clusterserver_leave")
  {
    ip_port peeripport(root["ip"].asString(), root["port"].asUInt());
    if (pthread_mutex_lock(&cs->idx_lock)!=0) throw THREAD_ERROR;
    cs->existing_cs_set.erase(peeripport);
    if (pthread_mutex_unlock(&cs->idx_lock)!=0) throw THREAD_ERROR;
  }
  else //unrecognized request
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
  if (pthread_mutex_lock(&idx_lock)!=0) throw THREAD_ERROR;
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
  //serialize existing_cs_set:
  result["existing_cs_set"] = Json::Value(Json::arrayValue);
  std::map<ip_port,bool>::iterator cssetitr = existing_cs_set.begin();
  while (cssetitr!=existing_cs_set.end())
  {
    Json::Value val;
    val["ip"] = cssetitr->first.first;
    val["port"] = cssetitr->first.second;
//std::cout<<"serialized port: "<<cssetitr->first.second<<std::endl;
    result["existing_cs_set"].append(val);
    cssetitr++;
  }
  //serialize timestamp:
  result["timestamp"] = static_cast<double>(timestamp);
  if (pthread_mutex_unlock(&idx_lock)!=0) throw THREAD_ERROR;
  return result;
}

//deserilization counterpart to get_serialized_state
void
clusterserver::update_cluster_state(const Json::Value& root)
{
std::cout<<"update cluster state"<<std::endl;
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
  const Json::Value& chiref = root["cmh"]["cluster_heap_idx"];
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
  //update existing_cs_set:
  existing_cs_set.clear();
  for (Json::ValueIterator itr1 = 
         root["existing_cs_set"].begin();
       itr1!=root["existing_cs_set"].end(); 
       itr1++)
  {
    existing_cs_set.insert(
      std::pair<ip_port,bool>(
        ip_port((*itr1)["ip"].asString(),
                (*itr1)["port"].asUInt()),
        true
      )
    );
  }
  //update timestamp:
  timestamp = static_cast<time_t>(root["timestamp"].asDouble());
}

/*
queries an existing gateway server about cluster server configuration,
then update itself by calling update_cluster_state().
*/
void 
clusterserver::join_cluster(std::string& joinip, uint16_t joinport)
{
  //client* cltp = NULL;
  //ip_port ipport = ip_port(joinip, joinport);
  //create_client(cltp, ipport);
  client clt(joinip.c_str(),joinport);
  Json::Value root;
  root["req_type"] = "clusterserver_join";
  root["ip"] = getip();
  root["port"] = getport();
  Json::StyledWriter writer;
  std::string outputConfig = writer.write(root);
  std::string reply = clt.sendstring(outputConfig.c_str());
  std::cout<<"clusterserver join cluster response: "<<reply<<std::endl;

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
  update_cluster_state(root);
}

/*
for each cluster server in cs->existing_cs_set, this routine
sends its cluster index data to each, along with its timestamp.
the receiving cluster servers will update their indexing members 
if received timestamp is greater than local timestamp
*/
void
clusterserver::broadcast_update_cluster_state(const ip_port& peeripport)
{
  Json::Value msg;
  msg["req_type"] = "broadcast_update_cluster_state";
  msg["req_args"] = get_serialized_state();
  broadcast(peeripport, msg);

/*
  std::map<ip_port,bool>::iterator itr = existing_cs_set.begin();
  std::map<ip_port,bool>::iterator itr_end = existing_cs_set.end();
  ip_port selfipport(getip(), getport());
  while (itr!=itr_end)
  {
    if (itr->first==exclude ||  //do not broadcast to remote node
        itr->first==selfipport) //do not broadcast to self
    {
      itr++; continue;
    }
    //client* cltp = NULL;
    //create_client(cltp, itr->first);
    client clt(itr->first.first.c_str(),itr->first.second);
    Json::Value root;
    root["req_type"] = "broadcast_update_cluster_state";
    root["req_args"] = get_serialized_state();
    Json::StyledWriter writer;
    std::string outputConfig = writer.write(root);
    clt.sendstring(outputConfig.c_str());
    itr++;
  }
*/
}

void
clusterserver::broadcast(const ip_port& exclude, 
                         const Json::Value& msg)
{
  std::map<ip_port,bool>::iterator itr = existing_cs_set.begin();
  std::map<ip_port,bool>::iterator itr_end = existing_cs_set.end();
  ip_port selfipport(getip(), getport());
  while (itr!=itr_end)
  {
    if (itr->first==exclude ||  //do not broadcast to remote node
        itr->first==selfipport) //do not broadcast to self
    {
      itr++; continue;
    }
    //below might be used in future if we decide to keep cs-to-cs conn
    //client* cltp = NULL;
    //create_client(cltp, itr->first);
    client clt(itr->first.first.c_str(),itr->first.second);
    Json::StyledWriter writer;
    std::string outputConfig = writer.write(msg);
    clt.sendstring(outputConfig.c_str());
    itr++;
  }
}

/*
singleton helper method to return a connection descriptor.
if conn already exists, return existing info.
else, make new connection and notify caller to delete ptr on heap.
void
clusterserver::create_client(client*& cltp, const ip_port& ipport)
{
  if (pthread_mutex_lock(&conntbl_lock)!=0) throw THREAD_ERROR;
  std::map<ip_port, client*>::iterator conntbl_itr = 
    connection_tbl.find(ipport);
  if (conntbl_itr==connection_tbl.end())
  {
    cltp = new client(ipport.first.c_str(), ipport.second);

    connection_tbl[ipport] = cltp;

  }
  else
{

    cltp = conntbl_itr->second;
}
  if (pthread_mutex_unlock(&conntbl_lock)!=0) throw THREAD_ERROR;
}
*/
