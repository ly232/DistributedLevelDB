//leveldbtst.cpp
//leveldb server test
#include <include/leveldb/db.h>
using namespace std;
int main()
{
try
{
  levelserver ls(8888);
  cout<<"leveldb server test"<<endl;
  cout<<"hostname: "<<ls.getsvrname()<<endl;
  cout<<"ip: "<<ls.getip()<<endl;
  cout<<"port: "<<ls.getport()<<endl;

  while (true)
  {
    int clfd = ls.accept_conn();
    if (clfd<0) throw SOCKET_ACCEPT_ERROR;
    ls.requestHandler(clfd);
  }
}
catch (int e)
{
  std::cerr<<"e="<<e<<std::endl;
}
  return 0;
}
