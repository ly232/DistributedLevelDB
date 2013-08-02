//gatetst.cpp
//gateway server test
#include "include gateserver.h"
using namespace std;
int main()
{
  gateserver gs(9999);
  cout<<"gateways server test"<<endl;
  cout<<"hostname: "<<gs.getsvrname()<<endl;
  cout<<"ip: "<<gs.getip()<<endl;
  cout<<"port: "<<gs.getport()<<endl;

  while(true)
  {
    int clfd = gs.accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    gs.requestHandler
  }
  return 0;
}
