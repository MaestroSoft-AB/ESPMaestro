#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

/* ******************************************************************* */
/* *************************** TCP CLIENT **************************** */
/* ******************************************************************* */

#define _POSIX_C_SOURCE 200809L /* This must be defined before netdb.h */


#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum
{
  CLIENT_STATE_INIT,
  CLIENT_STATE_CONNECTING,
  CLIENT_STATE_READING,
  CLIENT_STATE_WRITING,
  CLIENT_STATE_DISPOSING,
  CLIENT_STATE_ERROR

} TCPClientState;

typedef struct
{
  uint8_t* addr; // pointer to data
  size_t   size; // byte size of data

} TCP_Data;

typedef struct
{
  int   fd;
  char* readData;  /*Allocated in TCP_Client_Read, free'd in dispose*/
  char* writeData; /*Allocated in TCP_Client_Read, free'd in dispose*/

  TCP_Data                data;
  struct sockaddr_storage remote_addr;
  socklen_t               remote_addr_len;
  int                     has_remote_addr;

} TCP_Client;

int tcp_client_init(TCP_Client* _Client, const char* _host, const char* _port);
int tcp_client_init_ptr(TCP_Client** _ClientPtr, const char* _host, const char* _port);
int tcp_client_blocking_init(TCP_Client* _Client, const char* _host, const char* _port,
                             int _timeout_ms);
int tcp_client_read(TCP_Client* _Client);

/** Only runs recv() on given TCP_client fd to the passed buffer */
int tcp_client_read_simple(TCP_Client* _Client, uint8_t* _buf, int _buf_len);
/** Writes specified amount of data using realloc to TCP_Data struct
 * is type agnostic, hence pass size of type one wish to save data as */
ssize_t tcp_client_realloc_data(TCP_Data* _Data, void* _input, size_t _size);

int tcp_client_write(TCP_Client* _Client, size_t _length);
int tcp_client_write_simple(TCP_Client* _Client, const uint8_t* _buf, int _len);
int tcp_client_connect_step(TCP_Client* _Client); // For nonblocking tls
int tcp_client_finish_connect(int _fd);

void tcp_client_disconnect(TCP_Client* _Client);
void tcp_client_dispose(TCP_Client* _Client);
void tcp_client_dispose_ptr(TCP_Client** _ClientPtr);

#endif
