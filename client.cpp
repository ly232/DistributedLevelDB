//client.cpp
#include "include/client.h"
client::client(const char* remote_ip, const uint16_t remote_port)
{
  try
  {
    //socket construction:
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))==-1)
      throw SOCKET_CONSTRUCT_ERROR;
    memset(&svaddr, 0, sizeof(struct sockaddr_in));
    svaddr.sin_family = AF_INET;
    inet_pton(AF_INET, remote_ip, (void*)&svaddr.sin_addr);
    svaddr.sin_port = htons(remote_port);
    connect(socket_fd, (struct sockaddr*)&svaddr, sizeof(struct sockaddr));
  }
  catch(int e)
  {
    throw;
  }
}

client::~client()
{
  if (close(socket_fd)==-1)
    throw SOCKET_CLOSE_ERROR;
}

void client::sendfile(const char* fname)
{
  int fd;
  ssize_t byte_sent = 0;
  ssize_t byte_read = 0;
  try
  {
    if ((fd=open(fname, O_RDONLY))==-1)
      throw FILE_IO_ERROR;
    while ((byte_read=read(fd, buf, BUF_SIZE))>0)
    {
      if ((byte_sent=write(socket_fd, buf, byte_read))!=byte_read)
	throw FILE_IO_ERROR;
    }
    if (close(fd)<-1)
      throw FILE_IO_ERROR;
    if (byte_sent<-1)
      throw SOCKET_SEND_ERROR;
  }
  catch(int e)
  {
    throw;
  }
}
