//gatetst.cpp
//gateway server program
#include "include/gateserver.h"
#include "include/clusterserver.h"
#include <jsoncpp/json.h>
using namespace std;

gateserver* gs;

void signal_callback_handler(int signum)
{
  delete gs;
  exit(signum);
}

int main(int argc, char** argv)
{
try{
  signal(SIGINT, signal_callback_handler);
  char* ip = NULL;//(argc>1)?argv[1]:NULL;
  uint16_t gsport = 9999; //gateway server port
  uint16_t csport = 9998; //cluster server port
  string joinip;
  uint16_t joinport = 0;
  for (int i=1; i<argc; i++)
  {
    char* option = argv[i];
    if (!strcmp(option,"--joinip"))
    {
      joinip = std::string(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--joinport"))
    {
      joinport = atoi(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--gsport"))
    {
      gsport = atoi(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--csport"))
    {
      csport = atoi(argv[++i]);
      continue;
    }
  }
  bool master = (joinip=="" && !joinport);
  gs = new gateserver(gsport, csport, ip, master);
  cout<<"gateways server test"<<endl;
  cout<<"hostname: "<<gs->getsvrname()<<endl;
  cout<<"ip: "<<gs->getip()<<endl;
  cout<<"port: "<<gs->getport()<<endl;
  if (joinip!="" && joinport) 
    gs->join_cluster(joinip, joinport);

  while(true)
  {
    int clfd = gs->accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    gs->requestHandler(clfd);
  }
} catch (int e) {
  std::cerr<<"e="<<e<<std::endl;
}
  return 0;
}
