//leveldbtst.cpp
//leveldb server test
#include "include/leveldbserver.h"
using namespace std;

leveldbserver* ls;

void signal_callback_handler(int signum)
{
  //printf("caught signal %d\n",signum);
  delete ls;
  exit(signum);
}

int main()
{
try
{
  signal(SIGINT, signal_callback_handler);
  ls = new leveldbserver(9998,8888);
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
