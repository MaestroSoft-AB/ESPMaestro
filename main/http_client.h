/* ******************************************************************* */
/* *************************** HTTP CLIENT *************************** */
/* ******************************************************************* */
#ifndef HTTPClient_h
#define HTTPClient_h

#include <scheduler.h>
#include <error.h>
#include <http_parser.h>
#include <transport.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  HTTP_CLIENT_INITIALIZING,
  HTTP_CLIENT_CONNECTING,
  HTTP_CLIENT_WAITING_CONNECT,
  HTTP_CLIENT_BUILDING_REQUEST,
  HTTP_CLIENT_SENDING_REQUEST,
  HTTP_CLIENT_READING_FIRSTLINE,
  HTTP_CLIENT_READING_HEADERS,
  HTTP_CLIENT_DECIPHER_CHONKINESS,
  HTTP_CLIENT_READING_BODY_CHUNKED,
  HTTP_CLIENT_READING_BODY,
  HTTP_CLIENT_RETURNING,
  HTTP_CLIENT_DISPOSING,
  HTTP_CLIENT_ERROR,

} HTTPClientState;

typedef void (*http_client_on_success)(void* _context, char** _response);

typedef struct
{
  uint8_t* addr;
  ssize_t  size;
} http_data;

typedef struct
{
  char scheme[6]; // http/https
  char host[128];
  char port[6];
  char path[512];
} URL_Parts;

typedef struct
{
  URL_Parts url_parts;

  uint64_t next_retry_at;

  size_t bytes_sent;
  size_t decoded_body_len;

  Scheduler_Task*        task;
  const char*            URL;
  HTTP_Request*          req;
  HTTP_Response*         resp;
  http_client_on_success on_success;
  void*                  context;
  char**                 response_out;
  uint8_t*               request_buffer;
  uint8_t*               response_buffer;
  uint8_t*               decoded_body; // Might be too small
  http_data*             recv_buf;
  http_data*             blocking_out;

  http_data resp_buf;
  Transport transport;

  int    request_length;
  int    bytes_received;
  int    retries;
  int    content_length;
  int    chunked;
  size_t chunk_remaining;
  int    timeout_ms;

  HTTPClientState state;
  HTTPMethod      method;

  bool blocking_mode;
  /******************************************************* ADD BUFFER AND BUFFER SIZE TO REPLACE TCP
   * BUFFER AND SIZE FOR READING & WRITING *****************************************************/
} HTTP_Client;

/*Blocking API calls*/
/*_out is allocated in this client but needs to be free'd by caller*/
int http_blocking_get(const char* _url, http_data* _out, int _timeout_ms);
int http_blocking_post(const char* _url, const http_data* in, http_data* out, int _timeout_ms);
/**/

int  http_client_initiate(HTTP_Client* _Client, const char* _URL, HTTPMethod _method,
                          http_client_on_success _on_success, void* _context, char** _response_out);
void http_client_dispose(HTTP_Client* _Client);

#endif // HTTPClient_h
