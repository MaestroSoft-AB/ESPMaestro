#include <tcp_client.h>
#include <error.h>
#include <netdb.h>
// #include <poll.h>
#include <stdint.h>
#include "error.h"
#include <errno.h>
#include <esp_log.h>

static const char *TAG = "TCP CLIENT";

/*---------------------Internal functions------------------------------*/

int tcp_client_set_nonblocking(int fd, int nonblocking);
int tcp_client_wait_writable(int fd, int timeout);
/*---------------------------------------------------------------------*/
int tcp_client_init(TCP_Client* _Client, const char* _Host, const char* _Port)
{
  if (!_Client || !_Host || !_Port) {
    return ERR_INVALID_ARG;
  }

  _Client->fd        = -1;
  _Client->readData  = NULL;
  _Client->writeData = NULL;
  _Client->data.addr = NULL;

  struct addrinfo addresses;
  memset(&addresses, 0, sizeof(addresses));
  addresses.ai_family   = AF_UNSPEC;   /* IPv4 & IPv6 */
  addresses.ai_socktype = SOCK_STREAM; /* TCP */
  addresses.ai_protocol = IPPROTO_TCP;

  int status = ERR_IO;

  struct addrinfo* result = NULL;
  int              rc     = getaddrinfo(_Host, _Port, &addresses, &result);
  if (rc != 0) {
    ESP_LOGE(TAG, "getaddrinfo failed with code %d", rc);
    //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return ERR_IO;
  }

  int fd = -1;

  for (struct addrinfo* addr_info = result; addr_info; addr_info = addr_info->ai_next) {
    fd = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
    if (fd < 0)
      continue;

    if (tcp_client_set_nonblocking(fd, 1) != SUCCESS) {
      close(fd);
      fd = -1;
      continue;
    }

    int cres = connect(fd, addr_info->ai_addr, addr_info->ai_addrlen);
    if (cres == 0) {
      status = SUCCESS;
      memset(&_Client->remote_addr, 0, sizeof(_Client->remote_addr));
      memcpy(&_Client->remote_addr, addr_info->ai_addr, addr_info->ai_addrlen);
      _Client->remote_addr_len = (socklen_t)addr_info->ai_addrlen;
      _Client->has_remote_addr = 1;
      break;
    }
    if (cres < 0 && errno == EINPROGRESS) {
      /* non-blocking connect pågår */
      status = ERR_IN_PROGRESS;
      memset(&_Client->remote_addr, 0, sizeof(_Client->remote_addr));
      memcpy(&_Client->remote_addr, addr_info->ai_addr, addr_info->ai_addrlen);
      _Client->remote_addr_len = (socklen_t)addr_info->ai_addrlen;
      _Client->has_remote_addr = 1;
      break;
    }

    close(fd);
    fd = -1;
  }

  freeaddrinfo(result);

  if (fd < 0) {
    return ERR_IO;
  }

  _Client->fd = fd;
  return status;
}

int tcp_client_init_ptr(TCP_Client** _ClientPtr, const char* _Host, const char* _Port)
{
  if (!_ClientPtr) {
    return ERR_INVALID_ARG;
  }
  TCP_Client* client = (TCP_Client*)malloc(sizeof(TCP_Client));
  if (!client) {
    ESP_LOGE(TAG, "%s: malloc", strerror(errno));
    return ERR_NO_MEMORY;
  }
  int result = tcp_client_init(client, _Host, _Port);
  if (result != SUCCESS) {
    free(client);
    return result;
  }

  *(_ClientPtr) = client;

  return SUCCESS;
}

int tcp_client_blocking_init(TCP_Client* _Client, const char* _host, const char* _port,
                             int _timeout_ms)
{
  if (!_Client || !_host || !_port) {
    return ERR_INVALID_ARG;
  }

  _Client->fd              = -1;
  _Client->readData        = NULL;
  _Client->writeData       = NULL;
  _Client->data.addr       = NULL;
  _Client->has_remote_addr = 0;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags    = AI_ADDRCONFIG;

  struct addrinfo* result = NULL;

  int res = getaddrinfo(_host, _port, &hints, &result);
  if (res != 0) {
    ESP_LOGE(TAG, "getaddrinfo failed with code %d", res);
    ESP_LOGE(TAG, "I/O error at getaddrinfo");
    return ERR_IO;
  }

  int fd       = -1;
  int last_err = ERR_IO;

  for (struct addrinfo* ai = result; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) {
      continue;
    }

    // Set nonblocking so we can use poll with timeout
    if (tcp_client_set_nonblocking(fd, 1) != SUCCESS) {
      close(fd);
      fd = -1;
      continue;
    }
    int cres;
    do {
      cres = connect(fd, ai->ai_addr, ai->ai_addrlen);
    } while (cres < 0 && errno == EINTR);

    if (cres == 0) {
      // Connected
      last_err = SUCCESS;
    } else if ((cres < 0 && errno == EINPROGRESS) || errno == EALREADY) {
      // Connecting, wait for writable
      int wr = tcp_client_wait_writable(fd, _timeout_ms);
      if (wr != SUCCESS) {
        last_err = wr;
      } else {
        // check if connect was successful
        last_err = tcp_client_finish_connect(fd);
      }
    } else if (cres < 0 && errno == EISCONN) {
      // Already connected
      last_err = SUCCESS;
    } else {
      ESP_LOGE(TAG, "Connect failed");
    }

    if (last_err == SUCCESS) {
      memset(&_Client->remote_addr, 0, sizeof(_Client->remote_addr));
      memcpy(&_Client->remote_addr, ai->ai_addr, ai->ai_addrlen);
      _Client->remote_addr_len = (socklen_t)ai->ai_addrlen;
      _Client->has_remote_addr = 1;

      if (tcp_client_set_nonblocking(fd, 0) != SUCCESS) {
        close(fd);
        fd       = -1;
        last_err = ERR_IO;
        continue;
      }
      _Client->fd = fd;
      break;
    }

    close(fd);
    fd = -1;
  }

  freeaddrinfo(result);

  if (_Client->fd < 0) {
    ESP_LOGW(TAG, "Clientfd < 0");
    return last_err;
  }
  return SUCCESS;
}

int tcp_client_set_nonblocking(int fd, int nonblocking)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return ERR_IO;
  }

  if (nonblocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(fd, F_SETFL, flags) == -1) {
    return ERR_IO;
  }

  return SUCCESS;
}

int tcp_client_wait_writable(int _fd, int _timeout_ms)
{
  struct pollfd pfd;

  memset(&pfd, 0, sizeof(pfd));
  pfd.fd     = _fd;
  pfd.events = POLLOUT | POLLERR | POLLHUP;

  int res;

  do {
    res = poll(&pfd, 1, _timeout_ms);
  } while (res < 0 && errno == EINTR);

  if (res == 0) {
    return ERR_IO_TIMEOUT;
  }
  if (res < 0) {
    return ERR_IO;
  }

  return SUCCESS;
}

ssize_t tcp_client_realloc_data(TCP_Data* _Data, void* _input, size_t _size)
{
  if (!_Data || !_input || !_size) {
    return ERR_INVALID_ARG;
  }

  ssize_t new_size = _size * sizeof(uint8_t);
  void*   ptr      = realloc(
      _Data->addr, _Data->size + new_size +
                       1); // We reallocate memory for our chunk and make a pointer to the new addr
  if (!ptr) {
    ESP_LOGE(TAG, "%s: realloc", strerror(errno));
    ESP_LOGE(TAG, "Not enough memory for TCP buffer - realloc returned NULL");
    return ERR_NO_MEMORY;
  }

  _Data->addr = ptr; // We redefine our addr to the newly allocated memory (Should test if previous
                     // addr pointer should be freed aswell?)
  memcpy(&(_Data->addr[_Data->size]), _input,
         new_size);             // We copy realsize*bytes from contents to our chunk
  _Data->size += new_size;      // we add realsize to our chunksize
  _Data->addr[_Data->size] = 0; // null last byte for strings

  return new_size; // We return the size of the chunk
}

int tcp_client_read_simple(TCP_Client* _Client, uint8_t* _buf, int _buf_len)
{
  return recv(_Client->fd, _buf, _buf_len, 0);
}

int tcp_client_write(TCP_Client* _Client, size_t _Length)
{
  if (!_Client || _Client->fd < 0 || !_Client->writeData) {
    return ERR_INVALID_ARG;
  }

  size_t  bytesLeft = _Length;
  size_t  totalSent = 0;
  ssize_t bytesSent;
  char*   message = _Client->writeData;

  while (bytesLeft > 0) {

    bytesSent = send(_Client->fd, message, bytesLeft, 0);

    if (bytesSent < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN) {
        return totalSent;
      }

      ESP_LOGE(TAG, "%s: send", strerror(errno));
      return ERR_IO; // fatal error
    }

    if (bytesSent == 0) {
      return (int)totalSent;
    }

    totalSent += bytesSent;
    bytesLeft -= bytesSent;
    message += bytesSent;
  }
  return (int)totalSent;
}

int tcp_client_write_simple(TCP_Client* _Client, const uint8_t* _buf, int _len)
{
  return send(_Client->fd, _buf, _len, MSG_NOSIGNAL);
}

void tcp_client_disconnect(TCP_Client* _Client)
{
  if (_Client->fd >= 0)
    close(_Client->fd);

  _Client->fd = -1;
}

int tcp_client_connect_step(TCP_Client* _Client)
{
  if (!_Client || _Client->fd < 0 || !_Client->has_remote_addr) {
    return ERR_INVALID_ARG;
  }

  int res = connect(_Client->fd, (struct sockaddr*)&_Client->remote_addr, _Client->remote_addr_len);

  if (res == 0) {
    return SUCCESS;
  }

  if (errno == EISCONN) {
    // Connected now
    return SUCCESS;
  }

  if (errno == EINPROGRESS || errno == EALREADY) {
    // Still connecting
    return ERR_BUSY;
  }

  return ERR_IO; // Failed to connect
}

int tcp_client_finish_connect(int _fd)
{
  int       soerr = 0;
  socklen_t slen  = sizeof(soerr);

  if (getsockopt(_fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0) {
    return ERR_IO;
  }

  if (soerr != 0) {
    errno = soerr;
    return ERR_IO;
  }
  return SUCCESS;
}

void tcp_client_dispose(TCP_Client* _Client)
{
  if (_Client == NULL) {
    return;
  }
  if (_Client->fd >= 0) {
    close(_Client->fd);
    _Client->fd = -1;
  }
  if (_Client->readData != NULL) {
    free(_Client->readData);
    _Client->readData = NULL;
  }
  if (_Client->writeData != NULL) {
    free(_Client->writeData);
    _Client->writeData = NULL;
  }
  if (_Client->data.addr != NULL) {
    free(_Client->data.addr);
    _Client->data.addr = NULL;
    _Client->data.size = 0;
  }
}

void tcp_client_dispose_ptr(TCP_Client** _ClientPtr)
{
  if (_ClientPtr == NULL || *(_ClientPtr) == NULL) {
    return;
  }
  tcp_client_dispose(*(_ClientPtr));
  free(*(_ClientPtr));
  *(_ClientPtr) = NULL;
}
