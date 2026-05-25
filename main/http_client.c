#include "error.h"
#include <errno.h>
#include <http_client.h>
#include <string.h>
#include <string_utils.h>
#include <stddef.h>
#include <esp_log.h>

static const char *TAG = "HTTP CLIENT";

void            http_client_taskwork(void* _context, uint64_t _montime);
HTTPClientState http_client_worktask_connecting(HTTP_Client* _Client);
HTTPClientState http_client_worktask_build_request(HTTP_Client* _Client);
HTTPClientState http_client_worktask_send_request(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_firstline(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_headers(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_body(HTTP_Client* _Client);
HTTPClientState http_client_worktask_returning(HTTP_Client* _Client);
HTTPClientState http_client_worktask_decipher_chonkiness(HTTP_Client* _Client);
HTTPClientState http_client_worktask_read_body_chunked(HTTP_Client* _Client);
HTTPClientState http_client_worktask_waiting_connect(HTTP_Client* _Client);

/*******************Blocking funcs*****************************/
static int http_blocking_work(const char* _url, HTTPMethod _method, const http_data* _in_body,
                              http_data* _out_body, int _timeout_ms);
void       http_client_destroy(HTTP_Client* c);

/*************************************************************/

int http_client_initiate(HTTP_Client* _Client, const char* _URL, HTTPMethod _method,
                         http_client_on_success _on_success, void* _context, char** _response_out)
{
  (void)_method; // To be used with mbedtls
  if (!_Client) {
    return ERR_INVALID_ARG;
  }
  HTTP_Request* req = calloc(1, sizeof(HTTP_Request));
  if (!req) {
    return ERR_NO_MEMORY;
  }

  HTTP_Response* resp = calloc(1, sizeof(HTTP_Response));
  if (!resp) {
    return ERR_NO_MEMORY;
  }

  _Client->task = scheduler_create_task(_Client, http_client_taskwork);
  if (!_Client->task) {
    free(resp);
    free(req);
    return ERR_BUSY;
  }
  ESP_LOGD(TAG, "URL in init: %s", _URL);

  char* url_copy = strdup(_URL);
  if (!url_copy) {
    scheduler_destroy_task(_Client->task);
    _Client->task = NULL;
    free(req);
    free(resp);
    return ERR_NO_MEMORY;
  }

  _Client->on_success     = _on_success;
  _Client->resp           = resp;
  _Client->req            = req;
  _Client->URL            = url_copy;
  _Client->state          = HTTP_CLIENT_CONNECTING;
  _Client->request_length = 0;
  _Client->bytes_sent     = 0;
  _Client->retries        = 0;
  _Client->next_retry_at  = SystemMonotonicMS();
  URL_Parts url_parts     = {0};
  _Client->url_parts      = url_parts;
  _Client->response_out   = _response_out;
  _Client->context        = _context;
  _Client->timeout_ms     = 0;

  // Check url for http/https
  _Client->chunked          = -1;
  _Client->decoded_body     = NULL;
  _Client->decoded_body_len = 0;
  _Client->resp_buf.addr    = NULL;
  _Client->resp_buf.size    = 0;
  _Client->recv_buf         = &_Client->resp_buf;
  _Client->blocking_out     = NULL;
  _Client->blocking_mode    = 0;
  _Client->content_length   = 0;
  _Client->chunk_remaining  = 0;
  _Client->chunked          = -1;


  return 0;
}

int http_blocking_get(const char* _url, http_data* _out, int _timeout_ms)
{
  if (!_url || !_out) {
    return ERR_INVALID_ARG;
  }

  _out->addr = NULL;
  _out->size = 0;

  return http_blocking_work(_url, HTTP_GET, NULL, _out, _timeout_ms);
}

int http_blocking_post(const char* _url, const http_data* _in, http_data* _out, int _timeout_ms)
{
  if (!_url || !_in || (_in->size < 0)) {
    return ERR_INVALID_ARG;
  }

  if (_out) {
    _out->addr = NULL;
    _out->size = 0;
  }

  return http_blocking_work(_url, HTTP_POST, _in, _out, _timeout_ms);
}

static int http_blocking_work(const char* _url, HTTPMethod _method, const http_data* _in_body,
                              http_data* _out_body, int _timeout_ms)
{
  HTTP_Client* c = calloc(1, sizeof(*c));
  if (!c) {
    return ERR_NO_MEMORY;
  }

  c->URL  = strdup(_url);
  c->req  = calloc(1, sizeof(HTTP_Request));
  c->resp = calloc(1, sizeof(HTTP_Response));
  if (!c->URL || !c->req || !c->resp) {
    http_client_destroy(c);
    return ERR_NO_MEMORY;
  }

  c->resp_buf.addr    = NULL;
  c->resp_buf.size    = 0;
  c->recv_buf         = &c->resp_buf;
  c->decoded_body     = NULL;
  c->decoded_body_len = 0;
  c->content_length   = 0;
  c->chunk_remaining  = 0;
  c->chunked          = -1;

  c->blocking_mode = 1;
  c->blocking_out  = _out_body;
  c->timeout_ms    = _timeout_ms;

  c->method = _method;

  if (_in_body && _in_body->addr && _in_body->size > 0) {
    c->req->body = malloc(_in_body->size);
    if (!c->req->body) {
      http_client_destroy(c);
      return ERR_NO_MEMORY;
    }
    memcpy(c->req->body, _in_body->addr, _in_body->size);
    c->req->body_len = (size_t)_in_body->size;
  }

  c->state = HTTP_CLIENT_CONNECTING;

  uint64_t start = SystemMonotonicMS();

  while (1) {
    uint64_t now = SystemMonotonicMS();
    if (_timeout_ms > 0 && ((int)now - start) > (uint64_t)_timeout_ms) {
      http_client_destroy(c);
      return ERR_IO_TIMEOUT;
    }

    switch (c->state) {
    case HTTP_CLIENT_CONNECTING: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_CONNECTING");
      c->state = http_client_worktask_connecting(c);
      break;
    }

    case HTTP_CLIENT_BUILDING_REQUEST: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_BUILDING_REQUEST");
      c->state = http_client_worktask_build_request(c);
      break;
    }

    case HTTP_CLIENT_SENDING_REQUEST: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_SENDING_REQUEST");
      c->state = http_client_worktask_send_request(c);
      break;
    }

    case HTTP_CLIENT_READING_FIRSTLINE: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_READING_FIRSTLINE");
      c->state = http_client_worktask_read_firstline(c);
      break;
    }

    case HTTP_CLIENT_READING_HEADERS: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_READING_HEADERS");
      c->state = http_client_worktask_read_headers(c);
      break;
    }

    case HTTP_CLIENT_DECIPHER_CHONKINESS: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_DECIPHER_CHONKINESS");
      c->state = http_client_worktask_decipher_chonkiness(c);
      break;
    }

    case HTTP_CLIENT_READING_BODY_CHUNKED: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_READING_BODY_CHUNKED");
      c->state = http_client_worktask_read_body_chunked(c);
      break;
    }

    case HTTP_CLIENT_READING_BODY: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_READING_BODY");
      c->state = http_client_worktask_read_body(c);
      break;
    }

    case HTTP_CLIENT_RETURNING: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_RETURNING");
      c->state = http_client_worktask_returning(c);
      break;
    }

    case HTTP_CLIENT_DISPOSING: {
      ESP_LOGD(TAG, "Blocking: HTTP_CLIENT_DISPOSING");
      http_client_destroy(c);
      return SUCCESS;
    }

    case HTTP_CLIENT_ERROR:
    default:
      http_client_destroy(c);
      return ERR_IO;
    }
  }
}

HTTPClientState http_client_worktask_connecting(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  if (http_parser_url(_Client->URL, (void*)&_Client->url_parts) != SUCCESS) {
    return HTTP_CLIENT_ERROR;
  }
  int result;

  if (_Client->blocking_mode) {
    result = transport_init(&_Client->transport, _Client->url_parts.host, _Client->url_parts.port,
                            _Client->url_parts.scheme, _Client->timeout_ms, true);
  } else {
    result = transport_init(&_Client->transport, _Client->url_parts.host, _Client->url_parts.port,
                            _Client->url_parts.scheme, _Client->timeout_ms, false);
  }

  if (result == ERR_IN_PROGRESS) {
    return HTTP_CLIENT_WAITING_CONNECT;
  }

  if (result != SUCCESS) {
    ESP_LOGE(TAG, "TCP_Client init failed");
    return HTTP_CLIENT_ERROR;
  }

  return HTTP_CLIENT_BUILDING_REQUEST;


  return HTTP_CLIENT_BUILDING_REQUEST;
}

HTTPClientState http_client_worktask_waiting_connect(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  int res = transport_finish_connect(&_Client->transport);
  if (res == SUCCESS) {
    _Client->retries = 0;
    return HTTP_CLIENT_BUILDING_REQUEST;
  }

  if (errno == EINPROGRESS || errno == EALREADY || errno == EAGAIN) {
    _Client->next_retry_at = SystemMonotonicMS() + 50;
    return HTTP_CLIENT_WAITING_CONNECT;
  }

  return HTTP_CLIENT_ERROR;
}

HTTPClientState http_client_worktask_build_request(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  const char* method_str = "GET";

  if (_Client->method == HTTP_POST) {
    method_str = "POST";
  } else if (_Client->method == HTTP_PUT) {
    method_str = "PUT";
  } else if (_Client->method == HTTP_DELETE) {
    method_str = "DELETE";
  }

  const char* path = (_Client->url_parts.path[0] ? _Client->url_parts.path : "/");

  size_t body_len = (_Client->req->body && _Client->req->body_len > 0) ? _Client->req->body_len : 0;
  size_t extra_space = body_len > 0 ? 128 : 0;

  size_t max_len = strlen(_Client->url_parts.host) + strlen(path) + 256 + extra_space + body_len;

  _Client->request_buffer = malloc(max_len);
  if (!_Client->request_buffer) {
    return HTTP_CLIENT_ERROR;
  }

  int hdr_len;

  if (body_len > 0 && (_Client->method == HTTP_POST || _Client->method == HTTP_PUT)) {
    hdr_len = snprintf((char*)_Client->request_buffer, max_len,
                       "%s %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "User-Agent: httpclient\r\n"
                       "Connection: close\r\n"
                       "Content-Length: %zu\r\n"
                       "Content-Type: application/octet-stream\r\n"
                       "\r\n",
                       method_str, path, _Client->url_parts.host, body_len);
  } else {
    hdr_len = snprintf((char*)_Client->request_buffer, max_len,
                       "%s %s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "User-Agent: httpclient\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       method_str, path, _Client->url_parts.host);
  }

  if (hdr_len < 0) {
    free(_Client->request_buffer);
    _Client->request_buffer = NULL;
    return HTTP_CLIENT_ERROR;
  }

  size_t headers_len = (size_t)hdr_len;

  if (headers_len + body_len > max_len) {
    free(_Client->request_buffer);
    _Client->request_buffer = NULL;
    return HTTP_CLIENT_ERROR;
  }

  if (body_len > 0) {
    memcpy(_Client->request_buffer + headers_len, _Client->req->body, body_len);
  }

  _Client->request_length = (int)(headers_len + body_len);
  _Client->bytes_sent     = 0;

  return HTTP_CLIENT_SENDING_REQUEST;
}

HTTPClientState http_client_worktask_send_request(HTTP_Client* _Client)
{

  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  if (_Client->bytes_sent == 0) {
    fflush(stdout);
  }

  size_t remaining = 0;
  remaining        = _Client->request_length - _Client->bytes_sent;


  int written =
      transport_write(&_Client->transport,
                      (const uint8_t*)_Client->request_buffer + _Client->bytes_sent, remaining);

  if (written > 0) {
    _Client->bytes_sent += written;
    if (_Client->bytes_sent >= (size_t)_Client->request_length) {

      _Client->retries = 0;
      return HTTP_CLIENT_READING_FIRSTLINE;
    }

    _Client->next_retry_at = SystemMonotonicMS() + 100;
    return HTTP_CLIENT_SENDING_REQUEST;

  } else if (written == 0) {
    _Client->retries++;
    _Client->next_retry_at = SystemMonotonicMS() + 1000;

    return HTTP_CLIENT_SENDING_REQUEST;

  } else {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      _Client->next_retry_at = SystemMonotonicMS() + 100;
      return HTTP_CLIENT_SENDING_REQUEST;
    }
    if (_Client->retries < 3) {
      _Client->retries++;
      _Client->next_retry_at = SystemMonotonicMS() + 1000;
      return HTTP_CLIENT_SENDING_REQUEST;
    }
    return HTTP_CLIENT_ERROR;
  }

  _Client->retries = 0;
  return HTTP_CLIENT_READING_FIRSTLINE;
}

HTTPClientState http_client_worktask_read_firstline(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  Transport* t = &_Client->transport;

  uint8_t read_buf[8096];
  int     bytes_read = transport_read(t, read_buf, sizeof(read_buf) - 1);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_FIRSTLINE;
    }
    ESP_LOGE(TAG, "%s: recv firstline", strerror(errno));
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0) {
    ESP_LOGE(TAG, "Connection closed by peer");
    return HTTP_CLIENT_ERROR;
  }
  ssize_t bytes_stored =
      buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                    (size_t)bytes_read);

  if (bytes_stored < 0) {
    return HTTP_CLIENT_ERROR;
  }

  if (_Client->recv_buf->size == 0) {
    return HTTP_CLIENT_READING_FIRSTLINE;
  }

  int line_end = http_parser_find_line_end(_Client->recv_buf->addr, _Client->recv_buf->size);
  if (line_end < 0) {
    /*No \r\n found yet*/
    if (_Client->recv_buf->size >= 1024) {
      /*Invalid request*/
      ESP_LOGE(TAG, "Response too large..");
      return HTTP_CLIENT_ERROR;
    }
    /*Keep looking for line end on next work call*/
    return HTTP_CLIENT_READING_FIRSTLINE;
  }

  /*line_end is the index of the first \r*/
  size_t line_len = (size_t)line_end;

  if (line_len == 0 || line_len >= 1024) {
    ESP_LOGE(TAG, "Response too large..");
    return HTTP_CLIENT_ERROR;
  }

  if (http_parser_response_firstline((const char*)_Client->recv_buf->addr, line_len,
                                     _Client->resp) != SUCCESS) {
    /*Add internal error*/
    return HTTP_CLIENT_ERROR;
  }
  /*We have handled first line + 2 for \r\n*/
  ssize_t parsed = line_len + 2;

  /*If there is data remaining after first line shift it to beggining of
   * buffer*/
  if (_Client->recv_buf->size > parsed) {

    memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + parsed,
            _Client->recv_buf->size - parsed);
  }

  /*Remove first line by shrinking the buffer*/
  _Client->recv_buf->size -= parsed;

  return HTTP_CLIENT_READING_HEADERS;
}

HTTPClientState http_client_worktask_read_headers(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  if (_Client->recv_buf->size > 0) {
    int headers_end =
        http_parser_find_headers_end(_Client->recv_buf->addr, (size_t)_Client->recv_buf->size);

    if (headers_end >= 0) {
      size_t parsed_len = (size_t)headers_end + 4; // inkluderar \r\n\r\n

      fflush(stdout);

      if (http_parser_headers((const char*)_Client->recv_buf->addr, parsed_len,
                              &_Client->req->headers) != SUCCESS) {
        return HTTP_CLIENT_ERROR;
      }

      size_t body_already_read = 0;
      if ((size_t)_Client->recv_buf->size > parsed_len) {
        body_already_read = (size_t)_Client->recv_buf->size - parsed_len;
        memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + parsed_len, body_already_read);
      }
      _Client->recv_buf->size = (ssize_t)body_already_read;
      _Client->retries        = 0;

      const char* transfer_encoding_string = NULL;
      if (http_parser_get_header_value(_Client->req->headers, "Transfer-Encoding",
                                       &transfer_encoding_string) == SUCCESS &&
          transfer_encoding_string && strstr(transfer_encoding_string, "chunked")) {
        return HTTP_CLIENT_DECIPHER_CHONKINESS;
      }

      const char* content_length_string = NULL;
      if (http_parser_get_header_value(_Client->req->headers, "Content-Length",
                                       &content_length_string) == SUCCESS &&
          content_length_string) {
        int cl = 0;
        parse_string_to_int(content_length_string, &cl);
        if (cl > 0) {
          _Client->content_length = cl;

          if ((size_t)_Client->recv_buf->size >= (size_t)cl) {
            return HTTP_CLIENT_RETURNING;
          }
          return HTTP_CLIENT_READING_BODY;
        }
      }

      return HTTP_CLIENT_RETURNING;
    }
  }

  uint8_t read_buf[1024];
  int     bytes_read = transport_read(&_Client->transport, read_buf, sizeof(read_buf) - 1);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_HEADERS;
    }
    ESP_LOGE(TAG, "%s: recv headers", strerror(errno));
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0) {
    int headers_end =
        http_parser_find_headers_end(_Client->recv_buf->addr, (size_t)_Client->recv_buf->size);
    if (headers_end >= 0) {
      return http_client_worktask_read_headers(_Client);
    }

    ESP_LOGE(TAG, "Connection closed while reading headers");
    return HTTP_CLIENT_ERROR;
  }

  // NUL-terminera read_buf (säkert eftersom vi läser sizeof-1)
  read_buf[bytes_read] = '\0';

  ssize_t bytes_stored =
      buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                    (size_t)bytes_read);
  if (bytes_stored < 0) {
    return HTTP_CLIENT_ERROR;
  }

  return HTTP_CLIENT_READING_HEADERS;
}

HTTPClientState http_client_worktask_decipher_chonkiness(HTTP_Client* _Client)
{
  if (_Client == NULL) {
    return HTTP_CLIENT_ERROR;
  }


  int line_end = http_parser_find_line_end(_Client->recv_buf->addr, _Client->recv_buf->size);

  if (line_end < 0) {

    // Read until we find end of the line
    uint8_t read_buf[1024];
    int additional_bytes_read = transport_read(&_Client->transport, read_buf, sizeof(read_buf) - 1);

    if (additional_bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        // Read again
        return HTTP_CLIENT_DECIPHER_CHONKINESS;
      }
      ESP_LOGE(TAG, "%s: recv chunk", strerror(errno));
      return HTTP_CLIENT_ERROR;
    }

    if (additional_bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    if (buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                      (size_t)additional_bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_DECIPHER_CHONKINESS;
  }

  ssize_t consume_len = (ssize_t)line_end + 2;
  size_t parse_len   = (size_t)line_end;

  ssize_t chunk_size;

  for (size_t i = 0; i < (size_t)line_end; i++) {
    uint8_t ch = _Client->recv_buf->addr[i];
    if (ch == ';' || ch == ' ' || ch == '\t') {
      parse_len = i;
      break;
    }
  }

  if (parse_len <= 0) {
    return HTTP_CLIENT_ERROR;
  }

  char line[48];
  memcpy(line, _Client->recv_buf->addr, parse_len);
  line[parse_len] = '\0';

  char*              endptr = NULL;
  unsigned long long val    = strtoull(line, &endptr, 16);

  if (endptr == line) {
    return HTTP_CLIENT_ERROR;
  }

  chunk_size = (ssize_t)val;

  if (_Client->recv_buf->size > consume_len) {
    memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + consume_len,
            _Client->recv_buf->size - consume_len);
  }

  _Client->recv_buf->size -= consume_len;

  // ESP_LOGD(TAG, "%.*s", (int)TCP_C->data.size, (char *)TCP_C->data.addr);

  // Find trailing \r\n\r\n
  if (chunk_size == 0) {
    // No trailers just singe \r\n
    if (_Client->recv_buf->size >= 2 && _Client->recv_buf->addr[0] == '\r' &&
        _Client->recv_buf->addr[1] == '\n') {

      if (_Client->recv_buf->size > 2) {

        memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + 2, _Client->recv_buf->size - 2);
      }

      _Client->recv_buf->size -= 2;
      return HTTP_CLIENT_RETURNING;
    }

    // There are trailers
    int traling_end =
        http_parser_find_headers_end(_Client->recv_buf->addr, _Client->recv_buf->size);
    if (traling_end >= 0) {
      ssize_t trailers_consume = (ssize_t)traling_end + 4;
      if (_Client->recv_buf->size > trailers_consume) {
        memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + trailers_consume,
                _Client->recv_buf->size - trailers_consume);
      }
      _Client->recv_buf->size -= trailers_consume;
      return HTTP_CLIENT_RETURNING;
    }

    // Incomplete trailers
    // Read again
    uint8_t read_buf[1024];

    int bytes_read = transport_read(&_Client->transport, read_buf, sizeof(read_buf) - 1);

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return HTTP_CLIENT_DECIPHER_CHONKINESS;
      }
      ESP_LOGE(TAG, "%s: recv trailers", strerror(errno));
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }


    if (buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                      (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_DECIPHER_CHONKINESS;
  }

  _Client->chunk_remaining = (int)chunk_size;
  return HTTP_CLIENT_READING_BODY_CHUNKED;
}

HTTPClientState http_client_worktask_read_body_chunked(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }


  // Do we need more data?
  if (_Client->recv_buf->size < (ssize_t)_Client->chunk_remaining) {
    uint8_t read_buf[1024];
    int     bytes_read = transport_read(&_Client->transport, read_buf, sizeof(read_buf) - 1);

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        // Keep reading
        return HTTP_CLIENT_READING_BODY_CHUNKED;
      }
      ESP_LOGE(TAG, "%s, recv chunk-data", strerror(errno));
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    // If we have read more data

    if (buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                      (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_READING_BODY_CHUNKED;
  }

  size_t old_len = _Client->decoded_body_len;
  size_t add_len = (size_t)_Client->chunk_remaining;
  size_t new_len = old_len + add_len;

  uint8_t* newbuf = realloc(_Client->decoded_body, new_len + 1);
  if (newbuf == NULL) {
    return HTTP_CLIENT_ERROR;
  }
  _Client->decoded_body = newbuf;

  memcpy(_Client->decoded_body + old_len, _Client->recv_buf->addr, add_len);
  _Client->decoded_body_len      = new_len;
  _Client->decoded_body[new_len] = '\0';

  // Move read data out of tcpbuffer
  if (_Client->recv_buf->size > (ssize_t)_Client->chunk_remaining) {
    memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + _Client->chunk_remaining,
            _Client->recv_buf->size - _Client->chunk_remaining);
  }

  _Client->recv_buf->size -= _Client->chunk_remaining;
  _Client->chunk_remaining = 0;

  if (_Client->recv_buf->size < 2) {
    // NO CRLF (End of line) found
    uint8_t read_buf[1024];
    int     bytes_read = transport_read(&_Client->transport, read_buf, sizeof(read_buf));

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return HTTP_CLIENT_READING_BODY_CHUNKED;
      }
      ESP_LOGE(TAG, "%s: chunck recv", strerror(errno));
      return HTTP_CLIENT_ERROR;
    }

    if (bytes_read == 0) {
      // Connection closed
      return HTTP_CLIENT_ERROR;
    }

    if (buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                      (size_t)bytes_read) < 0) {
      return HTTP_CLIENT_ERROR;
    }

    return HTTP_CLIENT_READING_BODY_CHUNKED;
  }

  if (_Client->recv_buf->addr[0] != '\r' || _Client->recv_buf->addr[1] != '\n') {
    return HTTP_CLIENT_ERROR;
  }

  // Consume the CRLF (end of line)
  memmove(_Client->recv_buf->addr, _Client->recv_buf->addr + 2, _Client->recv_buf->size - 2);
  _Client->recv_buf->size -= 2;

  return HTTP_CLIENT_DECIPHER_CHONKINESS;
}

HTTPClientState http_client_worktask_read_body(HTTP_Client* _Client)
{
  if (!_Client) {
    return HTTP_CLIENT_ERROR;
  }

  unsigned int READ_BUF_SIZE = 1024;

  uint8_t read_buf[READ_BUF_SIZE];

  int bytes_read = transport_read(&_Client->transport, read_buf, READ_BUF_SIZE - 1);
  // ESP_LOGD(TAG, "Buf: %s", tcp_buf);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return HTTP_CLIENT_READING_BODY;
    }
    ESP_LOGE(TAG, "%s: recv body", strerror(errno));
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read == 0 && _Client->recv_buf->size < (ssize_t)_Client->content_length) {
    ESP_LOGE(TAG, "Connection closed before full body was recieved");
    return HTTP_CLIENT_ERROR;
  }

  if (bytes_read > 0) {
    read_buf[bytes_read] = '\0';
    ssize_t bytes_stored =
        buffer_append((void**)&_Client->recv_buf->addr, (size_t*)&_Client->recv_buf->size, read_buf,
                      (size_t)bytes_read);

    if (bytes_stored < 0) {
      return HTTP_CLIENT_ERROR;
    }
  }

  if (_Client->recv_buf->size < (ssize_t)_Client->content_length) {
    /*Keep reading body on next work call*/
    return HTTP_CLIENT_READING_BODY;
  }

  _Client->retries = 0;
  return HTTP_CLIENT_RETURNING;
}

HTTPClientState http_client_worktask_returning(HTTP_Client* _Client)
{
  if (!_Client || !_Client->recv_buf) {
    return HTTP_CLIENT_ERROR;
  }

  uint8_t* src     = NULL;
  size_t   src_len = 0;

  if (_Client->decoded_body != NULL && _Client->decoded_body_len > 0) {
    src     = _Client->decoded_body;
    src_len = _Client->decoded_body_len;
  } else {
    src     = (uint8_t*)_Client->recv_buf->addr;
    src_len = (size_t)_Client->recv_buf->size;
  }

  if (_Client->blocking_mode) {
    if (!_Client->blocking_out) {
      return HTTP_CLIENT_ERROR;
    }

    if (_Client->blocking_out->addr) {
      free(_Client->blocking_out->addr);
      _Client->blocking_out->addr = NULL;
      _Client->blocking_out->size = 0;
    }

    uint8_t* buf = malloc(src_len + 1);
    if (!buf) {
      return HTTP_CLIENT_ERROR;
    }

    if (src_len) {
      memcpy(buf, src, src_len);
    }
    buf[src_len]                = '\0';
    _Client->blocking_out->addr = buf;
    _Client->blocking_out->size = (ssize_t)src_len;
    return HTTP_CLIENT_DISPOSING;
  }

  if (!_Client->on_success) {
    return HTTP_CLIENT_ERROR;
  }

  char* response_out = malloc(src_len + 1);
  if (!response_out) {
    return HTTP_CLIENT_ERROR;
  }

  if (src_len) {
    memcpy(response_out, src, src_len);
  }

  response_out[src_len] = '\0';
  _Client->on_success(_Client->context, &response_out);

  return HTTP_CLIENT_DISPOSING;
}

void http_client_taskwork(void* _context, uint64_t _montime)
{
  if (!_context) {
    return;
  }


  (void)_montime;
  HTTP_Client* client = (HTTP_Client*)_context;

  static HTTPClientState last = -1;
  if (client->state != last) {
    last = client->state;
  }
  uint64_t now = SystemMonotonicMS();

  switch (client->state) {

  case HTTP_CLIENT_INITIALIZING: {
    ESP_LOGD(TAG, "HTTP_CLIENT_INITIALIZING");
    break;
  }
  case HTTP_CLIENT_CONNECTING: {
    if (now >= client->next_retry_at) {
      ESP_LOGD(TAG, "HTTP_CLIENT_CONNECTING");
      client->state = http_client_worktask_connecting(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_WAITING_CONNECT: {
    ESP_LOGD(TAG, "HTTP_CLIENT_WAITING_CONNECT");
    client->state = http_client_worktask_waiting_connect(client);
    break;
  }
  case HTTP_CLIENT_BUILDING_REQUEST: {
    ESP_LOGD(TAG, "HTTP_CLIENT_BUILDING_REQUEST");
    client->state = http_client_worktask_build_request(client);
    break;
  }
  case HTTP_CLIENT_SENDING_REQUEST: {
    ESP_LOGD(TAG, "HTTP_CLIENT_SENDING_REQUEST");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_send_request(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_READING_FIRSTLINE: {
    ESP_LOGD(TAG, "HTTP_CLIENT_READING_FIRSTLINE");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_firstline(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_READING_HEADERS: {
    ESP_LOGD(TAG, "HTTP_CLIENT_READING_HEADERS");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_headers(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_DECIPHER_CHONKINESS: {
    ESP_LOGD(TAG, "HTTP_CLIENT_DECIPHER_CHONKINESS");
    client->state = http_client_worktask_decipher_chonkiness(client);
    break;
  }
  case HTTP_CLIENT_READING_BODY_CHUNKED: {
    ESP_LOGD(TAG, "HTTP_CLIENT_READING_BODY_CHUNKED");
    client->state = http_client_worktask_read_body_chunked(client);
    break;
  }

  case HTTP_CLIENT_READING_BODY: {
    ESP_LOGD(TAG, "HTTP_CLIENT_READING_BODY");
    if (now >= client->next_retry_at) {
      client->state = http_client_worktask_read_body(client);
      break;
    }
    break;
  }
  case HTTP_CLIENT_RETURNING: {
    ESP_LOGD(TAG, "HTTP_CLIENT_RETURNING");
    client->state = http_client_worktask_returning(client);
    break;
  }
  case HTTP_CLIENT_ERROR: {
    ESP_LOGE(TAG, "HTTP_CLIENT_ERROR");
    client->state = HTTP_CLIENT_DISPOSING;
    break;
  }
  case HTTP_CLIENT_DISPOSING: {
    ESP_LOGI(TAG, "HTTP_CLIENT_DISPOSING");
    http_client_dispose(client);
    break;
  }
  default: {
    ESP_LOGI(TAG, "HTTP_CLIENT default");
    break;
  }
  }
}

void http_client_dispose(HTTP_Client* _Client)
{
  if (!_Client) {
    return;
  }

  // Stop task if any (safe even if NULL)
  if (_Client->task) {
    scheduler_destroy_task(_Client->task);
    _Client->task = NULL;
  }

  // Transport cleanup
  transport_dispose(&_Client->transport);

  // URL
  if (_Client->URL) {
    free((void*)_Client->URL);
    _Client->URL = NULL;
  }

  // Request/response objects
  if (_Client->req || _Client->resp) {
    http_parser_dispose(_Client->req, _Client->resp);
  }

  if (_Client->req) {
    free(_Client->req);
    _Client->req = NULL;
  }

  if (_Client->resp) {
    free(_Client->resp);
    _Client->resp = NULL;
  }

  // Request buffer
  if (_Client->request_buffer) {
    free(_Client->request_buffer);
    _Client->request_buffer = NULL;
  }

  // Chunk decoded body
  if (_Client->decoded_body) {
    free(_Client->decoded_body);
    _Client->decoded_body     = NULL;
    _Client->decoded_body_len = 0;
  }

  // Internal recv buffer storage (resp_buf)
  if (_Client->resp_buf.addr) {
    free(_Client->resp_buf.addr);
    _Client->resp_buf.addr = NULL;
    _Client->resp_buf.size = 0;
  }
}

void http_client_destroy(HTTP_Client* c)
{
  if (!c)
    return;
  http_client_dispose(c);
  free(c);
}
