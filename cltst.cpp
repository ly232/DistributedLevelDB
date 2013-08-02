//cltst.cpp
//unit test for client
#include <iostream>
#include "include/client.h"
using namespace std;
int main(int argc, char** argv)
{
  if (argc!=3) return 1;
  try
  {
    cout<<"client test"<<endl;
    client clt(argv[1], atoi(argv[2]));
    //clt.sendfile("a.out");
    clt.sendstring("hello world");
  }
  catch(int e)
  {
    cout<<"error code = "<<e<<endl;
  }
  return 0;
}
