#include <transport.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

/*************************** TLS TEST STUFF *********************/

static int transport_bio_send(void* ctx, const unsigned char* buf, size_t len)
{
  TCP_Client* tcp = (TCP_Client*)ctx;
  int         ret = tcp_client_write_simple(tcp, buf, (int)len);
  if (ret < 0)
    return -1; // tls_client kommer mappa errno om det behövs
  return ret;
}

static int transport_bio_recv(void* ctx, unsigned char* buf, size_t len)
{
  TCP_Client* tcp = (TCP_Client*)ctx;
  int         ret = tcp_client_read_simple(tcp, buf, (int)len);
  return ret; // errno sätts av recv()
}


int transport_init(Transport* t, const char* host, const char* port, const char* scheme,
                   int timeout_ms, bool use_blocking)
{

  if (t == NULL || host == NULL || port == NULL || scheme == NULL || host[0] == '\0' ||
      port[0] == '\0' || scheme[0] == '\0') {
    return ERR_INVALID_ARG;
  }

  memset(t, 0, sizeof(Transport));


  int  res;
  char scheme_lower[6] = {0};
  t->host              = host;
  t->port              = port;
  t->scheme            = scheme;
  t->timeout_ms        = timeout_ms;
  t->use_blocking      = use_blocking;


  // Convert scheme to lower for comparison
  for (int i = 0; scheme[i] != '\0' && i < 5; i++) {
    scheme_lower[i] = (char)tolower((unsigned char)scheme[i]);
  }


  if (strcmp(scheme_lower, "https") == 0) {
    t->use_tls = true;
  } else if (strcmp(scheme_lower, "http") == 0) {
    t->use_tls = false;
  } else {
    return ERR_BAD_FORMAT;
  }

  if (t->use_blocking) {
    res = tcp_client_blocking_init(&t->tcp, t->host, t->port, t->timeout_ms);
  } else {
    res = tcp_client_init(&t->tcp, t->host, t->port);
  }

  if (res != SUCCESS && res != ERR_IN_PROGRESS) {
    return res;
  }

  if (t->use_tls) {
    if (t->use_blocking) {
      TLS_BIO bio = {.io_ctx = &t->tcp, .send = transport_bio_send, .recv = transport_bio_recv};

      if (tls_client_init(&t->tls, host, &bio) != 0) {
        printf("Transport failed to init tls\n");
        tcp_client_dispose(&t->tcp);
        return ERR_IO;
      }
      t->tls_initiated = true;

      // Loop until handshake is done
      while (true) {
        int hs = tls_client_handshake_step(&t->tls);
        if (hs == 0) {
          return SUCCESS;
        }

        if (hs == ERR_IN_PROGRESS) {
          continue;
        }

        return ERR_IO;
      }
    } else {
      t->tls_initiated = false;
    }
  }
  // Non-blocking returns tcp status
  return res;
}


int transport_read(Transport* _Transport, uint8_t* buf, size_t len)
{

  if (!_Transport) {
    return ERR_INVALID_ARG;
  }

  if (!_Transport->use_tls) {
    return tcp_client_read_simple(&_Transport->tcp, buf, len);
  }

  int res = tls_client_read(&_Transport->tls, buf, len);

  if (res >= 0) {
    return res;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {

    return -1;
  }
  return ERR_IO;
}

int transport_write(Transport* _Transport, const uint8_t* buf, size_t len)
{
  if (_Transport == NULL) {
    return ERR_INVALID_ARG;
  }

  if (_Transport->use_tls == true) {
    return tls_client_write(&_Transport->tls, buf, len);
  }


  if (_Transport->use_tls == false) {
    return tcp_client_write_simple(&_Transport->tcp, buf, len);
  }

  // If we are here something went wrong
  return ERR_IO;
}

int transport_finish_connect(Transport* t)
{

  if (t == NULL) {
    return ERR_INVALID_ARG;
  }

  if (!t->use_tls) {
    if (t->use_blocking) {
      return SUCCESS;
    }

    return tcp_client_finish_connect(t->tcp.fd);
  }

  // TCP First finish tcp connection
  if (!t->use_blocking) {
    int cres = tcp_client_finish_connect(t->tcp.fd);
    if (cres != SUCCESS) {
      return cres;
    }
    if (t->use_tls && !t->tls_initiated) {
      TLS_BIO bio = {.io_ctx = &t->tcp, .send = transport_bio_send, .recv = transport_bio_recv};
      if (tls_client_init(&t->tls, t->host, &bio) != 0) {
        return ERR_IO;
      }
      t->tls_initiated = true;
    }
  }

  // Handshake


  int hs = tls_client_handshake_step(&t->tls);
  if (hs == 0) {
    printf("Handshake success!\n");
    return SUCCESS;
  }

  if (hs == ERR_IN_PROGRESS) {
    return ERR_IN_PROGRESS;
  }

  printf("finish_connect_failed\n");
  return ERR_IO;
}


void transport_dispose(Transport* _Transport)
{
  if (!_Transport) {
    return;
  }

  if (_Transport->use_tls == true) {
    tls_client_dispose(&_Transport->tls);
    tcp_client_dispose(&_Transport->tcp);
  } else {
    tcp_client_dispose(&_Transport->tcp);
  }
}
