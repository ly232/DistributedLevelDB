//cltst.cpp
//unit test for client
#include <iostream>
#include "include/client.h"
using namespace std;
int main()
{
  try
  {
    cout<<"client test"<<endl;
    client clt("192.168.75.153", 9999);
    //clt.sendfile("a.out");
    clt.sendstring("hello world");
  }
  catch(int e)
  {
    cout<<"error code = "<<e<<endl;
  }
}
