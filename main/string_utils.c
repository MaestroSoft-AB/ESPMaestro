#define _POSIX_C_SOURCE 199309L

#include <limits.h>
#include <error.h>
#include <string_utils.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

/* Concatenates two strings using malloc, strcpy and strcat
 * (goes without saying but needs to be freed by caller) */
char* stringcat(const char* _a, const char* _b)
{
  size_t len = strlen(_a) + strlen(_b) + 1;
  char*  ab  = (char*)malloc(sizeof(char) * len);
  strcpy(ab, _a);
  strcat(ab, _b);

  return ab;
}

#ifndef strdup
/* C89 approved strdup */
char* strdup(const char* _str)
{
  if (!_str)
    return NULL;

  size_t size = strlen(_str) + 1;
  char*  copy = malloc(size);
  if (copy)
    strcpy(copy, _str);
  else
    return NULL;

  return copy;
}
#endif /* strdup */

int parse_string_to_double(const char* _str, double* _double)
{
  if (!_str || !_double)
    return -1;

  int  read_max_len = 36; // precision; max chars to read
  char buf[read_max_len + 1];

  int    str_len    = strlen(_str);
  size_t actual_len = (str_len > read_max_len) ? read_max_len : str_len;

  errno = 0; // reset errno

  strncpy(buf, _str, actual_len);
  buf[actual_len] = '\0';

  char*  endptr;
  double dubb = strtod(buf, &endptr);

  if (errno != 0 || endptr == buf || *endptr != '\0')
    return -1;

  *_double = dubb;

  return 0;
}

int parse_string_to_int(const char* _str, int* _int)
{
  if (!_str || !_int)
    return -1;

  int  read_max_len = 36; // precision; max chars to read
  char buf[read_max_len + 1];

  int    str_len    = strlen(_str);
  size_t actual_len = (str_len > read_max_len) ? read_max_len : str_len;

  errno = 0; // reset errno
  strncpy(buf, _str, actual_len);
  buf[actual_len] = '\0';

  char* endptr;
  long  val = strtol(buf, &endptr, 10); // base 10 integer

  if (errno != 0 || endptr == buf || *endptr != '\0')
    return -1;

  if (val < INT_MIN || val > INT_MAX) {
    return -1;
  }

  *_int = (int)val;

  return 0;
}

ssize_t buffer_append(void** base_ptr, size_t* current_size, const void* input, size_t input_len)
{
  if (!base_ptr || !current_size || !input || input_len == 0) {
    return ERR_INVALID_ARG;
  }

  size_t new_total_size = *current_size + input_len;

  void* temp_ptr = realloc(*base_ptr, new_total_size + 1);

  if (!temp_ptr) {
    perror("realloc");
    return ERR_NO_MEMORY;
  }

  // Update original
  *base_ptr = temp_ptr;

  // Copy data to original buffer
  memcpy((uint8_t*)*base_ptr + *current_size, input, input_len);

  *current_size                        = new_total_size;
  ((uint8_t*)*base_ptr)[*current_size] = 0;

  return (ssize_t)input_len;
}

bool char_is_number(const char _c)
{
  int i;
  for (i = 0; i < 10; i++)
    if ((int)_c == i + 48)
      if ((int)_c == i + 48)
        return true;
  return false;
}
