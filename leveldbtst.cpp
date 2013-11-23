//leveldbtst.cpp
//leveldb server test
#include "include/leveldbserver.h"
using namespace std;

leveldbserver* ls;

void signal_callback_handler(int signum)
{
  delete ls;
  exit(signum);
}

void printusage()
{
  cout<<"leveldb server usage:"<<endl;
  cout<<"./l.out [--clusterport] [--selfport] "<<
                "[--clusterip] [--selfip] [--dbdir]"<<endl;
}

int main(int argc, char** argv)
{
try
{
  signal(SIGINT, signal_callback_handler);

  uint16_t clusterport = 9998;//(argc==5)?atoi(argv[1]):9998;
  uint16_t selfport = 8888;//(argc==5)?atoi(argv[2]):8888;
  std::string clusterip = "";//(argc==5)?std::string(argv[3]):"";
  char* selfip = NULL;//(argc==5)?argv[4]:NULL;
  std::string dbdir = "/home/ly232/levdb/db0";

  for (int i=1; i<argc; i++)
  {
    char* option = argv[i];
    if (!strcmp(option,"--help"))
    {
      printusage();
      return 0;
    }
    if (!strcmp(option,"--clusterport"))
    {
      clusterport = atoi(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--selfport"))
    {
      selfport = atoi(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--clusterip"))
    {
      clusterip = std::string(argv[++i]);
      continue;
    }
    if (!strcmp(option,"--selfip"))
    {
      selfip = argv[++i];
      continue;
    }
    if (!strcmp(option,"--dbdir"))
    {
      dbdir = std::string(argv[++i]);
      continue;
    }
  }

  ls = new leveldbserver(clusterport,selfport,clusterip,selfip,dbdir);
  cout<<"leveldb server test"<<endl;
  cout<<"hostname: "<<ls->getsvrname()<<endl;
  cout<<"ip: "<<ls->getip()<<endl;
  cout<<"port: "<<ls->getport()<<endl;

  while (true)
  {
    int clfd = ls->accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    ls->requestHandler(clfd);
  }
}
catch (int e)
{
  delete ls;
  std::cerr<<"e="<<e<<std::endl;
}
  return 0;
}
