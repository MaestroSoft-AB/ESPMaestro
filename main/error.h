#ifndef __ERROR_H__
#define __ERROR_H__

/*
 *
 * All functions should return:
 *   - SUCCESS (0) on success
 *   - A negative ErrorCode value on failure
 *
 * This allows simple checks such as:
 *      if (res < 0) { handle error }
 */

typedef enum {

  /* ------------------------------------------------------------
   * Success
   * ------------------------------------------------------------ */
  SUCCESS = 0, /**< Operation completed successfully */

  /* ------------------------------------------------------------
   * Argument / Input Errors     (-1 to -9)
   * ------------------------------------------------------------ */
  ERR_INVALID_ARG = -1, /**< One or more arguments were invalid */
  ERR_BAD_FORMAT = -3,  /**< Input data was syntactically incorrect */

  /* ------------------------------------------------------------
   * Memory Errors               (-10 to -19)
   * ------------------------------------------------------------ */
  ERR_NO_MEMORY = -10, /**< Memory allocation failed */

  /* ------------------------------------------------------------
   * I/O or System Errors        (-20 to -29)
   * ------------------------------------------------------------ */
  ERR_IO = -20,          /**< General read/write or system-level I/O error */
  ERR_TIMEOUT = -21,     /**< Operation timed out */
  ERR_WOULD_BLOCK = -22, /**< Non-blocking operation would block */
  ERR_CONNECTION_LOST = -23, /**< Connection dropped unexpectedly */
  ERR_CONNECTION_FAIL = -24, /**< Failed to establish a connection */
  ERR_BUSY = -25,
  ERR_IN_PROGRESS = -26, /*Connection in progress*/

  /* ------------------------------------------------------------
   * Parse / Protocol Errors     (-30 to -39)
   * ------------------------------------------------------------ */
  ERR_PARSE = -30,              /**< Failed to parse input or message */
  ERR_JSON_PARSE = -31,         /**< Failed to parse input or message */
  ERR_JSON_OBJ_NOT_FOUND = -32, /**< Failed to parse input or message */

  /* ------------------------------------------------------------
   * Data Errors                 (-40 to -49)
   * ------------------------------------------------------------ */
  ERR_NOT_FOUND = -40, /**< Requested item or resource not found */

  /* ------------------------------------------------------------
   * Internal / Fatal Errors     (-100 and below)
   * ------------------------------------------------------------ */
  ERR_INTERNAL = -100, /**< Internal invariant failure or unexpected state */
  ERR_FATAL = -101     /**< Unrecoverable error; system must terminate */

} ErrorCode;

#endif /* __ERROR_H__ */
