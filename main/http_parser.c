#include <errno.h>
#include <http_client.h>
#include <linked_list.h>
#include <http_parser.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>

static const char *TAG = "HTTP PARSER";

void http_parser_dispose(HTTP_Request* _Req, HTTP_Response* _Resp);

int http_parser_first_line(const char* _line, size_t _line_len, HTTP_Request* _Req,
                           Linked_List** _params_out)
{

  if (!_line || !_Req || !_params_out || _line_len < 1) {
    return ERR_INVALID_ARG;
  }

  *_params_out = NULL;

  char* line_copy = malloc(_line_len + 1);
  if (!line_copy) {
    ESP_LOGE(TAG, "%s: malloc", strerror(errno));
    return ERR_NO_MEMORY;
  }

  memcpy(line_copy, _line, _line_len);
  line_copy[_line_len] = '\0';

  char* method      = line_copy;
  char* first_space = strchr(line_copy, ' ');
  if (!first_space) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_BAD_FORMAT;
  }
  *first_space = '\0';

  char* request_target = first_space + 1;
  char* second_space   = strchr(request_target, ' ');
  if (!second_space) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_BAD_FORMAT;
  }
  *second_space = '\0';

  char* version = second_space + 1;
  if (*version == 0) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_BAD_FORMAT;
  }

  char* question_mark = strchr(request_target, '?');

  if (!question_mark) {
    /*No query string*/
    _Req->path = strdup(request_target);
    if (!_Req->path) {
      if (line_copy != NULL)
        free(line_copy);
      return ERR_NO_MEMORY;
    }

  } else {
    *question_mark = '\0';
    _Req->path     = strdup(request_target);
    if (!_Req->path) {
      if (line_copy != NULL)
        free(line_copy);
      return ERR_NO_MEMORY;
    }

    *_params_out = linked_list_create();
    if (!*_params_out) {
      if (line_copy != NULL)
        free(line_copy);
      http_parser_dispose(_Req, NULL);
      return ERR_NO_MEMORY;
    }

    char* param_str = question_mark + 1;

    while (param_str && *param_str) {
      char* param_delim = strchr(param_str, '=');
      if (!param_delim)
        break;
      *param_delim    = '\0';
      char* param_key = param_str;

      char* amp = strchr(param_delim + 1, '&');
      char* param_value;

      if (amp) {
        *amp        = '\0';
        param_value = param_delim + 1;
        param_str   = amp + 1; /*Keep looping*/
      } else {
        param_value = param_delim + 1;
        param_str   = NULL; /*Last param*/
      }

      HTTP_Key_Value* param = malloc(sizeof(HTTP_Key_Value));
      if (!param) {
        if (line_copy != NULL)
          free(line_copy);
        http_parser_dispose(_Req, NULL);
        return ERR_NO_MEMORY;
      }
      param->key   = strdup(param_key);
      param->value = strdup(param_value);
      if (!param->key || !param->value) {
        if (line_copy != NULL)
          free(line_copy);
        http_parser_dispose(_Req, NULL);
        return ERR_NO_MEMORY;
      }

      linked_list_item_add(*_params_out, NULL, param);
      _Req->params_count++;
    }
  }

  _Req->method_str = strdup(method);
  _Req->version    = strdup(version);
  if (!_Req->method_str || !_Req->version) {
    if (line_copy != NULL)
      free(line_copy);
    http_parser_dispose(_Req, NULL);
    return ERR_NO_MEMORY;
  }

  _Req->method = http_method_string_to_enum(_Req->method_str);

  if (line_copy != NULL)
    free(line_copy);

  return SUCCESS;
}

int http_parser_response_firstline(const char* _line, size_t _line_len, HTTP_Response* _Resp)
{
  if (!_line || _line_len == 0 || !_Resp) {
    return ERR_INVALID_ARG;
  }

  char* line_copy = malloc(_line_len + 1);
  if (!line_copy) {
    return ERR_NO_MEMORY;
  }

  memcpy(line_copy, _line, _line_len);
  line_copy[_line_len] = '\0';

  // Format: HTTP-version SP status-code SP reason-phrase
  char* version_string = line_copy;
  char* first_space    = strchr(line_copy, ' ');
  if (!first_space) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_BAD_FORMAT;
  }
  *first_space = '\0';

  char* status_string = first_space + 1;
  char* second_space  = strchr(status_string, ' ');
  if (!second_space) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_BAD_FORMAT;
  }
  *second_space       = '\0';
  char* reason_string = second_space + 1;

  _Resp->version = strdup(version_string);
  if (!_Resp->version) {
    if (line_copy != NULL)
      free(line_copy);
    return ERR_NO_MEMORY;
  }

  _Resp->status_code_string = strdup(status_string);
  if (!_Resp->status_code_string) {
    if (line_copy != NULL)
      free(line_copy);
    http_parser_dispose(NULL, _Resp);
    return ERR_NO_MEMORY;
  }

  _Resp->reason_phrase = strdup(reason_string);
  if (!_Resp->reason_phrase) {
    if (line_copy != NULL)
      free(line_copy);
    http_parser_dispose(NULL, _Resp);
    return ERR_NO_MEMORY;
  }

  int status_int = 0;
  parse_string_to_int(_Resp->status_code_string, &status_int);
  if (strcmp(_Resp->reason_phrase, HttpStatus_reasonPhrase(status_int)) != 0) {
    if (line_copy != NULL)
      free(line_copy);
    http_parser_dispose(NULL, _Resp);
    return ERR_BAD_FORMAT;
  }

  if (line_copy != NULL)
    free(line_copy);
  return SUCCESS;
}

int http_parser_find_line_end(const uint8_t* _buf, size_t _buf_len)
{
  if (_buf_len < 2) {
    return -1;
  }

  size_t i;
  for (i = 0; i + 1 < _buf_len; i++) {
    if (_buf[i] == '\r' && _buf[i + 1] == '\n') {
      return (int)i;
    }
  }
  return -1;
}

int http_parser_find_headers_end(const uint8_t* _buf, size_t _buf_len)
{
  if (_buf_len < 4) {
    return -1;
  }
  size_t i;
  for (i = 0; i + 3 < _buf_len; i++) {
    if (_buf[i] == '\r' && _buf[i + 1] == '\n' && _buf[i + 2] == '\r' && _buf[i + 3] == '\n') {
      return (int)i;
    }
  }
  return -1;
}

int http_parser_headers(const char* _buf, size_t _buf_len, Linked_List** _headers_out)
{
  if (!_buf || !_headers_out) {
    return ERR_INVALID_ARG;
  }

  *_headers_out = linked_list_create();
  if (!*(_headers_out)) {
    return ERR_NO_MEMORY;
  }

  size_t start = 0;
  while (start < _buf_len) {

    int    line_end = -1;
    size_t i;

    for (i = start; i + 1 < _buf_len; i++) {
      if (_buf[i] == '\r' && _buf[i + 1] == '\n') {
        line_end = (int)i;
        break;
      }
    }

    if (line_end < 0)
      break;

    size_t line_len = (size_t)line_end - start;

    if (line_len == 0) {
      start = line_end + 2;
      break;
    }

    char* line = (char*)malloc(line_len + 1);
    if (!line) {
      ESP_LOGE(TAG, "%s: malloc", strerror(errno));
      http_parser_dispose_linked_list(*_headers_out);
      *_headers_out = NULL;
      return ERR_NO_MEMORY;
    }

    memcpy(line, _buf + start, line_len);
    line[line_len] = '\0';

    char* colon = strchr(line, ':');
    if (!colon) {
      if (line != NULL)
        free(line);
      http_parser_dispose_linked_list(*_headers_out);
      *_headers_out = NULL;
      return ERR_BAD_FORMAT;
    }

    *colon      = '\0';
    char* key   = line;
    char* value = colon + 1;

    /*Ignore leading whitespace or tabs by moving pointer forward*/
    while (*value == ' ' || *value == '\t') {
      value++;
    }

    /*Remove trailing whitespace from key*/
    /*end is '\0' so we want to look for the char before it*/
    char* end = key + (strlen(key));
    while (end > key && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {

      *--end = '\0';
    }

    /*Same thing for value, remove whitespace or unwanted chars*/
    end = value + strlen(value);
    while (end > value &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
      *--end = '\0';
    }

    HTTP_Key_Value* header = (HTTP_Key_Value*)malloc(sizeof(HTTP_Key_Value));
    if (!header) {
      ESP_LOGE(TAG, "%s: malloc", strerror(errno));
      if (line != NULL)
        free(line);
      http_parser_dispose_linked_list(*_headers_out);
      *_headers_out = NULL;
      return ERR_NO_MEMORY;
    }

    header->key   = strdup(key);
    header->value = strdup(value);

    if (!header->key || !header->value) {
      ESP_LOGE(TAG, "%s: strdup", strerror(errno));
      free(header->key);
      free(header->value);
      free(header);
      if (line != NULL)
        free(line);
      http_parser_dispose_linked_list(*_headers_out);
      *_headers_out = NULL;
      return ERR_NO_MEMORY;
    }

    linked_list_item_add(*(_headers_out), NULL, header);

    if (line != NULL)
      free(line);
    start = line_end + 2;
  }

  return SUCCESS;
}

const char* http_build_full_response(int _status_code, const char* _headers, const char* _body)
{
  (void)_body;
  (void)_headers;
  /* Build firstline */
  const char*  reason_phrase = HttpStatus_reasonPhrase(_status_code);
  char         firstline[128];
  unsigned int firstline_len =
      snprintf(firstline, 128, HTTP_RESPONSE_FIRSTLINE_TEMPLATE, _status_code, reason_phrase);
  firstline[firstline_len] = '\0';

  // Yesyes will finish later
  return NULL;
}

HTTPMethod http_method_string_to_enum(const char* _method_str)
{
  if (strcmp(_method_str, "GET") == 0)
    return HTTP_GET;
  if (strcmp(_method_str, "OPTIONS") == 0)
    return HTTP_OPTIONS;
  if (strcmp(_method_str, "POST") == 0)
    return HTTP_POST;
  if (strcmp(_method_str, "PUT") == 0)
    return HTTP_PUT;
  if (strcmp(_method_str, "DELETE") == 0)
    return HTTP_DELETE;
  if (strcmp(_method_str, "DOWNLOAD") == 0)
    return HTTP_DOWNLOAD;

  return HTTP_INVALID;
}

const char* http_method_enum_to_string(HTTPMethod _method)
{
  if (_method == HTTP_OPTIONS)
    return "OPTIONS";
  if (_method == HTTP_GET)
    return "GET";
  if (_method == HTTP_POST)
    return "POST";
  if (_method == HTTP_PUT)
    return "PUT";
  if (_method == HTTP_DELETE)
    return "DELETE";
  if (_method == HTTP_DOWNLOAD)
    return "DOWNLOAD";
  if (_method == HTTP_INVALID)
    return "INVALID";

  return NULL;
}

int http_parser_get_header_value(Linked_List* _headers, char* _name, const char** _out_value)
{

  if (!_headers || !_name || !_out_value) {
    return ERR_INVALID_ARG;
  }

  linked_list_foreach(_headers, node)
  {
    HTTP_Key_Value* h = (HTTP_Key_Value*)node->item;
    if (!h || !h->key || !h->value) {
      continue;
    }

    if (strcmp(h->key, _name) == 0) {
      *(_out_value) = h->value;
      return SUCCESS;
    }
  }

  return ERR_NOT_FOUND;
}

void http_parser_dispose_linked_list(Linked_List* _list)
{
  if (!_list)
    return;

  linked_list_foreach(_list, node)
  {
    HTTP_Key_Value* h = (HTTP_Key_Value*)node->item;
    if (h->key != NULL) {
      free(h->key);
    }
    if (h->value != NULL) {
      free(h->value);
    }
    if (h != NULL) {
      free(h);
    }
  }
  linked_list_items_dispose(_list);
  linked_list_destroy(&_list);
}

int http_parser_url(const char* _URL, void* _Context)
{
  if (!_URL || !_Context) {
    return ERR_INVALID_ARG;
  }

  URL_Parts* url_parts = (URL_Parts*)_Context;

  url_parts->scheme[0] = '\0';
  url_parts->host[0]   = '\0';
  url_parts->port[0]   = '\0';
  url_parts->path[0]   = '\0';

  const char* url_ptr = _URL;

  /*-----------------------SCHEME (HTTP/HTTPS)------------------------------*/
  const char* scheme_end = strstr(url_ptr, "://");
  if (!scheme_end) {
    return ERR_BAD_FORMAT;
  }

  size_t scheme_len = (size_t)(scheme_end - url_ptr);
  if (scheme_len == 0 || scheme_len >= sizeof(url_parts->scheme)) {
    return ERR_BAD_FORMAT;
  }

  memcpy(url_parts->scheme, url_ptr, scheme_len);
  url_parts->scheme[scheme_len] = '\0';

  // move past ://
  url_ptr = scheme_end + 3;

  /*-----------------------HOST------------------------------*/
  const char* host_start = url_ptr;
  const char* slash      = strchr(url_ptr, '/');
  const char* colon      = strchr(url_ptr, ':');

  const char* host_end = NULL;

  // : is only the separtor if it comes before /
  if (colon && (!slash || colon < slash)) {
    host_end = colon;
  } else if (slash) {
    host_end = slash;
  } else {
    host_end = url_ptr + strlen(url_ptr);
  }

  size_t host_len = (size_t)(host_end - host_start);
  if (host_len == 0 || host_len >= sizeof(url_parts->host)) {
    return ERR_BAD_FORMAT;
  }

  memcpy(url_parts->host, host_start, host_len);
  url_parts->host[host_len] = '\0';

  /*---------------------PORT (if specified)---------------------------*/
  const char* path_start = host_end;

  if (host_end == colon) {
    const char* port_start = colon + 1;
    const char* url_end    = _URL + strlen(_URL);
    const char* port_end   = slash ? slash : url_end;

    if (port_start == port_end) {
      return ERR_BAD_FORMAT;
    }

    size_t port_len = (size_t)(port_end - port_start);
    if (port_len >= sizeof(url_parts->port)) {
      return ERR_BAD_FORMAT;
    }

    /*Validate port*/
    for (const char* ptr = port_start; ptr < port_end; ptr++) {
      if (!isdigit((unsigned char)*ptr)) {
        return ERR_BAD_FORMAT;
      }
    }

    memcpy(url_parts->port, port_start, port_len);
    url_parts->port[port_len] = '\0';

    path_start = port_end;
  }

  /*-----------------PATH---------------------------------------*/

  if (*path_start == '/') {
    size_t path_len = strlen(path_start);
    if (path_len >= sizeof(url_parts->path)) {
      return ERR_BAD_FORMAT;
    }

    memcpy(url_parts->path, path_start, path_len + 1);

  } else {
    // No path
    url_parts->path[0] = '/';
    url_parts->path[1] = '\0';
  }

  /*---------------------DEFAULT PORT---------------------------*/
  if (url_parts->port[0] == '\0') {
    // convert scheme to lowercase for safe comparison
    char scheme_lower[6] = {0};
    for (int i = 0; url_parts->scheme[i] && i < 5; i++) {
      scheme_lower[i] = (char)tolower((unsigned char)url_parts->scheme[i]);
    }

    if (strcmp(scheme_lower, "https") == 0) {
      strcpy(url_parts->port, "443");
    } else if (strcmp(scheme_lower, "http") == 0) {
      strcpy(url_parts->port, "80");
    } else {
      return ERR_BAD_FORMAT;
    }
  }

  return SUCCESS;
}

void http_parser_dispose(HTTP_Request* _Req, HTTP_Response* _Resp)
{
  if (_Req != NULL) {
    if (_Req->method_str != NULL) {
      free(_Req->method_str);
      _Req->method_str = NULL;
    }

    if (_Req->path != NULL) {
      free(_Req->path);
      _Req->path = NULL;
    }

    if (_Req->query != NULL) {
      free(_Req->query);
      _Req->query = NULL;
    }

    if (_Req->version != NULL) {
      free(_Req->version);
      _Req->version = NULL;
    }

    if (_Req->body != NULL) {
      free(_Req->body);
      _Req->body = NULL;
    }

    http_parser_dispose_linked_list(_Req->params);
    _Req->params = NULL;
    http_parser_dispose_linked_list(_Req->headers);
    _Req->headers = NULL;
  }

  if (_Resp != NULL) {

    if (_Resp->firstline != NULL) {
      free(_Resp->firstline);
      _Resp->firstline = NULL;
    }

    if (_Resp->headers != NULL) {
      free(_Resp->headers);
      _Resp->headers = NULL;
    }

    if (_Resp->body != NULL) {
      free(_Resp->body);
      _Resp->body = NULL;
    }

    if (_Resp->full_response != NULL) {
      free(_Resp->full_response);
      _Resp->full_response = NULL;
    }

    if (_Resp->version != NULL) {
      free(_Resp->version);
      _Resp->version = NULL;
    }

    if (_Resp->reason_phrase != NULL) {
      free(_Resp->reason_phrase);
      _Resp->reason_phrase = NULL;
    }

    if (_Resp->status_code_string != NULL) {
      free(_Resp->status_code_string);
      _Resp->status_code_string = NULL;
    }
  }
}
