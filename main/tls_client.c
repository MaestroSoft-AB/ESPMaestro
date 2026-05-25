#include <tls_client.h>
#include <tls_global_ca.h>
#include <error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/x509.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>


/**********************************BIO********************************************************/

static int tls_bio_send(void* ctx, const unsigned char* buf, size_t len)
{
  TLS_Client* c = (TLS_Client*)ctx;

  int res = c->bio.send(c->bio.io_ctx, buf, len);

  if (res > 0) {
    return res;
  }

  if (res == 0) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }


  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int tls_bio_recv(void* ctx, unsigned char* buf, size_t len)
{
  TLS_Client* c = (TLS_Client*)ctx;

  int res = c->bio.recv(c->bio.io_ctx, buf, len);

  if (res > 0) {
    return res;
  }

  if (res == 0) {
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}


/********************************************************************************************/


int tls_client_init(TLS_Client* _tls, const char* _host, const TLS_BIO* _bio)
{
  if (!_tls || !_bio || !_bio->send || !_bio->recv) {
    return ERR_INVALID_ARG;
  }

  memset(_tls, 0, sizeof(TLS_Client));
  _tls->bio = *_bio;

  mbedtls_ssl_init(&_tls->ssl);
  mbedtls_ssl_config_init(&_tls->conf);
  mbedtls_entropy_init(&_tls->entropy);
  mbedtls_ctr_drbg_init(&_tls->ctr_drbg);

  const char* pers = "maestro_tls_client";

  int res = mbedtls_ctr_drbg_seed(&_tls->ctr_drbg, mbedtls_entropy_func, &_tls->entropy,
                                  (const unsigned char*)pers, strlen(pers));

  if (res != 0) {
    printf("mbedtls_ctr_drbg_seed failed, error: %d\n", res);
    return ERR_IO;
  }

  res = mbedtls_ssl_config_defaults(&_tls->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);

  if (res != 0) {
    printf("mbedtls_ssl_config_defaults failed, error: %d\n", res);
    return ERR_IO;
  }

  mbedtls_ssl_conf_authmode(&_tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_rng(&_tls->conf, mbedtls_ctr_drbg_random, &_tls->ctr_drbg);

  /*SET GLOBAL CA*/
  mbedtls_x509_crt* ca = global_tls_ca_get();
  if (!ca) {
    printf("Failed to set global CA\n");
    return ERR_IO;
  }

  mbedtls_ssl_conf_ca_chain(&_tls->conf, ca, NULL);

  res = mbedtls_ssl_setup(&_tls->ssl, &_tls->conf);
  if (res != 0) {
    printf("mbedtls_ssl_setup failed, error: %d\n", res);
    return ERR_IO;
  }

  res = mbedtls_ssl_set_hostname(&_tls->ssl, _host);
  if (res != 0) {
    printf("mbedtls_ssl_set_hostname failed, error: %d\n", res);
    return ERR_IO;
  }

  /*SET BIO*/

  mbedtls_ssl_set_bio(&_tls->ssl, _tls, tls_bio_send, tls_bio_recv, NULL);

  _tls->handshake_done = 0;

  return SUCCESS;
}

int tls_client_handshake_step(TLS_Client* _tls)
{
  if (!_tls) {
    return ERR_INVALID_ARG;
  }

  if (_tls->handshake_done) {
    return SUCCESS;
  }

  int res = mbedtls_ssl_handshake(&_tls->ssl);

  if (res == 0) {
    _tls->handshake_done = 1;
    return SUCCESS;
  }

  // Set errno so http doesn't have to be adjusted for tls
  if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
    errno = EAGAIN;
    return ERR_IN_PROGRESS;
  }

  // if we get here something went wrong

  const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&_tls->ssl);
  if (peer) {
    char sub[512], iss[512];
    mbedtls_x509_dn_gets(sub, sizeof(sub), &peer->subject);
    mbedtls_x509_dn_gets(iss, sizeof(iss), &peer->issuer);
  }

  uint32_t flags = mbedtls_ssl_get_verify_result(&_tls->ssl);
  if (flags) {
    char vrfy[1024];
    mbedtls_x509_crt_verify_info(vrfy, sizeof(vrfy), "  ! ", flags);
  }

  char errbuf[256];
  mbedtls_strerror(res, errbuf, sizeof(errbuf));
  printf("TLS handshake error: -0x%04X (%s)\n", -res, errbuf);

  return ERR_IO;
}

int tls_client_read(TLS_Client* _tls, uint8_t* _buf, size_t _len)
{
  if (!_tls || !_buf) {
    return ERR_INVALID_ARG;
  }

  int res = mbedtls_ssl_read(&_tls->ssl, _buf, _len);

  if (res > 0) {
    return res;
  }

  if (res == 0) {
    return 0;
  }

  if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
    errno = EAGAIN;
    return -1;
  }

  if (res == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
    return 0;
  }

  char errbuf[256];
  mbedtls_strerror(res, errbuf, sizeof(errbuf));
  printf("mbedtls_ssl_read error: %d (-0x%04X) %s\n", res, (unsigned)-res, errbuf);


  errno = EIO;
  return -1;
}

int tls_client_write(TLS_Client* _tls, const uint8_t* _buf, size_t _len)
{
  if (!_tls || !_buf) {
    return ERR_INVALID_ARG;
  }

  int res = mbedtls_ssl_write(&_tls->ssl, _buf, _len);

  if (res >= 0) {
    return res;
  }

  if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
    errno = EAGAIN;
    return -1;
  }

  errno = EIO;
  return -1;
}

void tls_client_dispose(TLS_Client* _tls)
{
  if (!_tls) {
    return;
  }

  mbedtls_ssl_close_notify(&_tls->ssl);

  mbedtls_ssl_free(&_tls->ssl);
  mbedtls_ssl_config_free(&_tls->conf);
  mbedtls_ctr_drbg_free(&_tls->ctr_drbg);
  mbedtls_entropy_free(&_tls->entropy);

  memset(_tls, 0, sizeof(TLS_Client));
}
