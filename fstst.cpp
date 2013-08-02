//fstst.cpp
//file server test
#include "include/fileserver.h"
using namespace std;
int main(){
try{
  cout<<"file server test"<<endl;
  fileserver fs(8888);
  cout<<"file server host name: "<<fs.getsvrname()<<endl;
  cout<<"file server ip: "<<fs.getip()<<endl;
  cout<<"file server port: "<<fs.getport()<<endl;
  
  while (true)
  {
    int clfd = fs.accept_conn();
      if (clfd<0) throw SOCKET_ACCEPT_ERROR;
      switch(fork())
      {
	case -1:
	  cerr<<"fork error"<<endl;
	  exit(1);
	case 0: //child process
	  close(fs.getsockfd());//close listen sock
	  fs.requestHandler(clfd);
	  switch(fork())
	  {
	    case -1:
	      cerr<<"fork error"<<endl;
	      exit(1);
	    case 0: //child process
	      execlp("/bin/chmod","chmod","755","recv.out",NULL);
	    default:
	      wait(NULL);
	      execlp("./recv.out","recv.out",NULL);
	  }
	default: //parent process
	  break;
      }
  }
}
catch(int e)
{
  cout<<"error codde = "<<e<<endl;
}
  return 0;
}
