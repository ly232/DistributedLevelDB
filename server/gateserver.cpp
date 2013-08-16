//gateserver.cpp
#include "include/gateserver.h"
#include "include/syncobj.h"
#include "include/client.h"
#include <jsoncpp/json.h>

static size_t hash(std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  for (int i=0; i<len; i++)
    val += (size_t)s[i];
  val %= MAX_CLUSTER;
  return val;
}

void* gateserver::cluster_server_init(void* arg)
{
  gateserver* gs = (gateserver*) arg;
  while (true)
  {
    int clfd = gs->cs->accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    gs->cs->requestHandler(clfd);
  }
}

gateserver::gateserver(const uint16_t gsport, 
		       const uint16_t csport, 
		       const char* ip)
  :server(gsport, ip),sync_client(true)
{
  //start cluster server to 
  //monitor leveldb servers join/leave cluster
  cs = new clusterserver(csport, getip().c_str());
  if (pthread_create(cs->get_thread_obj(), 
		     0, 
		     &cluster_server_init, 
		     (void*)this)
      !=0) 
    throw THREAD_ERROR;
}

gateserver::~gateserver()
{
  delete cs;
}

void* gateserver::main_thread(void* arg)
{
  /*
   * main thread handles a client request by 3 phases:
   * 1) spawn a recv_thread to read the entire json request
   * 2) identify a leveldb server and send request to that server
   * 3) wait for leveldb server's response,
   *    then pass that response to client
   */
  std::vector<void*>* argv = (std::vector<void*>*)arg;
  int* clfdptr = (int*)((*argv)[0]);
  int clfd = *clfdptr;
  gateserver* gatesvr = (gateserver*)((*argv)[1]);

  volatile bool* client_exit = new bool;
  *client_exit = false;

  while (!(*client_exit))
  {
    std::string* ackmsg = new std::string;

    //recv thread will fill in leveldbrsp
    syncobj* so = new syncobj(2L, 2L, 1L);

    std::vector<void*> thread_arg;
    thread_arg.push_back((void*)&clfd);
    thread_arg.push_back((void*)so);
    thread_arg.push_back((void*)ackmsg);
    thread_arg.push_back((void*)client_exit);
    thread_arg.push_back((void*)gatesvr);

    //for recv thread:
    if (pthread_create(&so->_thread_obj_arr[0], 
		       0, 
		       &recv_thread, 
		       (void*)&thread_arg)
	!=0)
      throw THREAD_ERROR;
    //for send thread:
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
    delete ackmsg;
    delete so;
  }

  delete clfdptr;
  delete (std::vector<void*>*)arg;
  delete client_exit;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

//requestHandler does not block
//it passes the work down to another thread main_thread
//main_thread will block, but that does not block gateway server
void gateserver::requestHandler(int clfd)
{
  pthread_t main_thread_obj;
  int* clfdptr = new int; //will be deleted by main thread
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

void* gateserver::send_thread(void* arg)
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

void* gateserver::recv_thread(void* arg)
{
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex 
    = ((syncobj*)argv[1])->_mutex_arr[0];
  int byte_received = -1;
  std::string request;
  char buf[BUF_SIZE];
  bool done = false;
  //cv between send and recv threads:
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  pthread_mutex_t& cv_mutex = ((syncobj*)argv[1])->_mutex_arr[1];
  std::string* ackmsg = (std::string*)argv[2];
  bool* client_exit = (bool*)argv[3];
  gateserver* gatesvr = (gateserver*)argv[4];

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
  std::cout<<"request="<<request<<std::endl;

  //if client requests exit, 
  //set client_exit flag so that 
  //main thread can close socket accordingly
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(request,root))
  {
    if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
    *ackmsg = 
      "error: invalid json requst format encounted in recv thread";
    if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
    if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
    return 0;
  }
  std::string req_type = root["req_type"].asString();
  if (req_type=="exit")
  {
    pthread_mutex_lock(&socket_mutex);
    *client_exit = true;
    pthread_mutex_unlock(&socket_mutex);
    Json::StyledWriter writer;
    root.clear();
    root["status"] = "OK";
    root["result"] = "";
    if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
    *ackmsg = writer.write(root);
    if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
    if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
    return 0;
  }

  // pick a leveldb server to forward request
  // now gateserver acts as client to leveldbserver
  //char ldbsvrip[INET_ADDRSTRLEN] = "192.168.75.164";
  //const uint16_t ldbsvrport = 8888;
  std::string key = root["req_args"]["key"].asString();
  bool skip = false;
  std::string ldback;
  const size_t cluster_id = hash(key);
  std::list<server_address>& svrlst = 
    gatesvr->cs->get_server_list(cluster_id);
  std::list<server_address>::iterator itr 
    = svrlst.begin();
  
if (gatesvr->sync_client)
{
  while(itr!=svrlst.end())
  {
    std::string ldbsvrip = itr->_ip;
    const uint16_t ldbsvrport = itr->_port;
    itr++;
    client clt(ldbsvrip.c_str(), ldbsvrport);
    ldback = clt.sendstring(request.c_str());
    root.clear();
    if (!reader.parse(ldback,root))
      continue;
    if (root["status"].asString()=="OK"&&!skip)
    {
      skip = true;
      if (pthread_mutex_lock(&cv_mutex)!=0) continue;
      *ackmsg = ldback;
      if (pthread_mutex_unlock(&cv_mutex)!=0) continue;
    }
  }
  if (!skip)
  {
    if (pthread_mutex_lock(&cv_mutex)!=0) 
      throw THREAD_ERROR;
    *ackmsg = ldback;
    if (pthread_mutex_unlock(&cv_mutex)!=0) 
      throw THREAD_ERROR;
    if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
    return 0;
  }
  if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
}
else //async client, i.e. call client::sendstring_noblock
{
  int* numdone = new int;
  *numdone = 0;
  int* numtotal = new int;
  *numtotal = svrlst.size();
  syncobj* cso = new syncobj(0, 1, 1);
  char* reqstr = new char[request.size()+1];
  memcpy(reqstr, request.c_str(), request.size()+1);
  while(itr!=svrlst.end())
  {
    std::string ldbsvrip = itr->_ip;
    const uint16_t ldbsvrport = itr->_port;
    itr++;
    client clt = client(ldbsvrip.c_str(), ldbsvrport);
    clt.sendstring_noblock(reqstr, cso, numdone);
  }
  //for eventual consistency, we wait for at least one ack.
  if (pthread_mutex_lock(&cso->_mutex_arr[0])) throw THREAD_ERROR;
  while(!(*numdone))
    pthread_cond_wait(&cso->_cv_arr[0], &cso->_mutex_arr[0]);
  if (pthread_mutex_unlock(&cso->_mutex_arr[0])) throw THREAD_ERROR;
  //delete cso;
  std::vector<void*>* cleanarg = new std::vector<void*>;
  cleanarg->push_back((void*)cso);
  cleanarg->push_back((void*)numdone);
  cleanarg->push_back((void*)numtotal);
  cleanarg->push_back((void*)reqstr);
  pthread_t cleanup_thread; //clean up numdone in background
  if (pthread_create(&cleanup_thread, 
      0, &cleanup_thread_handler, (void*)cleanarg))
      throw THREAD_ERROR;
}
  return 0;
}

void* gateserver::cleanup_thread_handler(void* arg)
{
  std::vector<void*>* cleanarg = (std::vector<void*>*)arg;
  syncobj* cso = (syncobj*)((*cleanarg)[0]);
  int* numdone = (int*)((*cleanarg)[1]);
  int* numtotal = (int*)((*cleanarg)[2]);
  char* reqstr = (char*)((*cleanarg)[3]);
  if (pthread_mutex_lock(&cso->_mutex_arr[0])) throw THREAD_ERROR;
  while (*numdone < *numtotal) 
    pthread_cond_wait(&cso->_cv_arr[0], &cso->_mutex_arr[0]);
  if (pthread_mutex_unlock(&cso->_mutex_arr[0])) throw THREAD_ERROR;
  delete cso;
  delete numdone;
  delete cleanarg;
  delete [] reqstr;
}

