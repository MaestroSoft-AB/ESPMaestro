#ifndef __HTTP_SERVER_PARSER_H_
#define __HTTP_SERVER_PARSER_H_

/* ******************************************************************* */
/* ************************** HTTP PARSING *************************** */
/* ******************************************************************* */

#include "linked_list.h"
#include <http_status_codes.h>
#include <error.h>
#include <string_utils.h>

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define HTTP_REQUEST_MAX_PARAMS 10
#define HTTP_RESPONSE_FIRSTLINE_TEMPLATE "HTTP/1.1 %i %s\r\n"

typedef enum
{
  HTTP_OPTIONS,
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_DOWNLOAD,
  HTTP_INVALID

} HTTPMethod;

typedef struct
{
  char* firstline;
  char* headers;
  char* body;
  char* full_response;
  char* version;
  char* reason_phrase;
  char* status_code_string;

  HttpStatus_Code status_code;

} HTTP_Response;

typedef struct
{
  char* method_str;
  char* path;
  char* query; // maybe we don't need to save this now that we have array of params
  char* version;
  char* body;

  Linked_List* params;
  Linked_List* headers;

  HTTPMethod method;

  int params_count;
  int firstline_len; // To set pointer for headers parsing
  int headers_len;
  int body_len;

} HTTP_Request;

typedef struct
{
  char* key;
  char* value;

} HTTP_Key_Value;

HTTPMethod  http_method_string_to_enum(const char* _method_str);
const char* http_method_enum_to_string(HTTPMethod _method);
int         http_parser_first_line(const char* _line, size_t _line_len, HTTP_Request* _Req,
                                   Linked_List** _params_out);
int         http_parser_find_line_end(const uint8_t* _buf, size_t _buf_len);
int         http_parser_find_headers_end(const uint8_t* _buf, size_t _buf_len);
int         http_parser_headers(const char* _buf, size_t _buf_len, Linked_List** _headers_out);
void        http_parser_dispose_linked_list(Linked_List* _list);
int  http_parser_get_header_value(Linked_List* _headers, char* _name, const char** _out_value);
int  http_parser_response_firstline(const char* _line, size_t _line_len, HTTP_Response* _Resp);
int  http_parser_url(const char* _URL, void* _Context);
void http_parser_dispose(HTTP_Request* _Req, HTTP_Response* _Resp);

#endif
