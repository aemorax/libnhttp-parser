#include "nhttp-parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CR 13
#define LF 10
#define SP 32

typedef const enum parse_status {
  ERROR_REQUEST_METHOD_MALFORMED = -2,
  ERROR_REQUEST_MALFORMED = -1,  // for preprocessing request
  STATUS_OK = 0,
} n_parse_error_t;

static const int8_t token_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 1, 0, 1, 1, 1, 1, 1,  // 30-39
    0, 0, 1, 1, 0, 1, 1, 0, 1, 1,  // 40,49
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0,  // 50,59
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  // 60,69
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70,79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80,89
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1,  // 90,99
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 100,109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 110,119
    1, 1, 1, 0, 1, 0, 1, 0         // 120,127
};

static const int8_t is_token(const char item) { return token_map[item]; }

typedef struct raw_http_request {
  const char *request;
  size_t request_len;

  const char *request_headers;
  size_t request_headers_len;

  const char *request_message;
  size_t request_message_len;

  const char *method;
  size_t method_len;
  const char *uri;
  size_t uri_len;

  const char *request_version;
  size_t request_version_len;
} raw_http_request;

static const int8_t pre_process_request(raw_http_request *req) {
  const char *buf = req->request;
  const char *endbuf = buf + req->request_len;
  const char *start_pos = buf;

  // Initializing Method
  req->method = buf;
  while (*buf != SP) {
    if (*buf == CR || buf == endbuf) return ERROR_REQUEST_MALFORMED;
    buf++;
  }
  req->method_len = buf - start_pos;
  buf++;

  // Initializing Request-URI
  req->uri = buf;
  start_pos = buf;
  while (*buf != SP) {
    if (*buf == CR || buf == endbuf) return ERROR_REQUEST_MALFORMED;
    buf++;
  }
  req->uri_len = buf - start_pos;
  buf++;

  // Initializing Request-Version
  req->request_version = buf;
  start_pos = buf;
  while (buf - 1 <= endbuf) {
    if (buf == endbuf) return ERROR_REQUEST_MALFORMED;

    if (*(buf + 1) == CR) {
      if (*(buf + 2) == LF) {
        buf++;
        break;
      } else {
        return ERROR_REQUEST_MALFORMED;
      }
    }
    buf++;
  }
  req->request_version_len = buf - start_pos;
  buf += 2;

  // Initializing Request-Headers
  req->request_headers = buf;
  start_pos = buf;
  while (buf - 1 <= endbuf) {
    if (buf == endbuf) return ERROR_REQUEST_MALFORMED;

    if (*buf == CR) {
      if (*(buf + 1) == LF) {
        break;
      } else {
        return ERROR_REQUEST_MALFORMED;
      }
    }
    buf++;
  }
  req->request_headers_len = buf - start_pos;
  buf += 2;

  req->request_message = buf;
  start_pos = buf;
  do {
    buf++;
  } while (buf <= endbuf);
  buf--;
  req->request_message_len = buf - start_pos;

  return STATUS_OK;
}

static const n_parse_error_t parse_request(const char *buf, size_t buf_len,
                                           raw_http_request *request) {
  if (buf_len <= 0) return ERROR_REQUEST_MALFORMED;

  request->request = buf;
  request->request_len = buf_len;

  return pre_process_request(request);
}

#undef CR
#undef LF
#undef SP
