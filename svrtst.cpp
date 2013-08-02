//svrtst.cpp
//unit test for server
#include <iostream>
#include "include/server.h"
using namespace std;
int main()
{
  try
  {
    cout<<"server test"<<endl;
    server svr(8888);
    cout<<"server hostname: "<<svr.getsvrname()<<endl;
    cout<<"server ip: "<<svr.getip()<<endl;
    cout<<"server port: "<<svr.getport()<<endl;
/*
    while (true)
    {
      int clfd = svr.accept();
      if (clfd<0) throw SOCKET_ACCEPT_ERROR;
      switch(fork())
      {
	case -1:
	  cout<<"fork error"<<endl;
	  exit(1);
	case 0: //child process
	  close(svr.getsockfd()); //request handler doesnt need listen socket
	  requestHandler(clfd);
	  exit(0);
	default:
	  close(clfd);
	  break;
      }
    }
*/

    svr.run("./hello.out");
    cout<<"finished receiving"<<endl;
    pid_t pid_chmod = fork();
    if (pid_chmod==0)
    {
      //child process
      cout<<"will do chmod"<<endl;
      char a1[] = "chmod";
      char a2[] = "755";
      char a3[] = "hello.out";
      char* arglist[] = {a1,a2,a2};
      execve("chmod", arglist, NULL);
      exit(0);
    }
    else
    {
      cout<<"finished chmod"<<endl;
      pid_t pid_hello = fork();
      if (pid_hello==0)
      {
	cout<<"will execve hello.out"<<endl;
	char arg[] = "hello.out";
	char* arglist[] = {arg};
	execve("hello.out",arglist,NULL);
	exit(0);
      }
      else
      {
	cout<<"done with hello.out"<<endl;
      }
    }


  }
  catch(int e)
  {
    cout<<"error code = "<<e<<endl;
    cout<<"errno = "<<errno<<endl;
  }
  return 0;
}
