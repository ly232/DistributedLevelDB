//gatetst.cpp
//gateway server test
#include "include/gateserver.h"
using namespace std;
int main(int argc, char** argv)
{
try{
  char* ip = (argc>1)?argv[1]:NULL;
  gateserver gs(9999, 9998, ip); //gate svr port, cluster svr port
  cout<<"gateways server test"<<endl;
  cout<<"hostname: "<<gs.getsvrname()<<endl;
  cout<<"ip: "<<gs.getip()<<endl;
  cout<<"port: "<<gs.getport()<<endl;

  while(true)
  {
    int clfd = gs.accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    gs.requestHandler(clfd);
  }
} catch (int e) {
  std::cerr<<"e="<<e<<std::endl;
}
  
  return 0;
}
