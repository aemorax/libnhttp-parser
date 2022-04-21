#include "nhttp-parser.h"

#include <stddef.h>
#include <stdint.h>

#define CR 13
#define LF 10
#define SP 32

typedef enum parse_status {
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

// 33, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
// 58, 59 61, 63, 64, 95, 126
static const int8_t reserve_unreserve_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 1, 0, 0, 1, 0, 1, 1,  // 30-39
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 40-49
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 50-59
    0, 1, 0, 1, 1, 1, 1, 1, 1, 1,  // 60-69
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80-89
    1, 0, 0, 0, 0, 1, 0, 1, 1, 1,  // 90-99
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 100-109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 110-119
    1, 1, 1, 0, 0, 0, 1, 0         // 120-127
};

// [48-57], [65-70], [97-102]
static const int8_t hex_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 30-39
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  // 40-49
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0,  // 50-59
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  // 60-69
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-89
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1,  // 90-99
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0,  // 100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 110-119
    0, 0, 0, 0, 0, 0, 0, 0         // 120-127
};

// [48-57]
static const int8_t digit_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0       
};

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

static const int32_t http_version_parse_machine(const char * buf, const char * endbuf) {
  int32_t movement = 0;
  char c = *buf;
STATE_A:
  if(digit_map[c]) {
    movement++;
  } else {
    return -1;
  }
STATE_B:
  c = *(buf+movement);
  if(digit_map[c]) {
    movement++;
    goto STATE_B;
  } else if(c == '.') {
    movement++;
    goto STATE_C;
  } else {
    return -1;
  }
STATE_C:
  c = *(buf+movement);
  if(digit_map[c]) {
    movement++;
    goto STATE_C;
  }

  return movement;
}

static const n_parse_error_t pre_process_request(raw_http_request *req) {
  const char *buf = req->request;
  const char *endbuf = buf + req->request_len;
  const char *start_pos = buf;

  // Initializing Method
  req->method = buf;
  while (*buf != SP) {
    if (*buf == CR || buf == endbuf || !token_map[*buf]) return ERROR_REQUEST_MALFORMED;
    buf++;
  }
  req->method_len = buf - start_pos;
  buf++;

  // Initializing Request-URI
  req->uri = buf;
  start_pos = buf;
  while (*buf != SP) {
    if (*buf == CR || buf > endbuf) return ERROR_REQUEST_MALFORMED;
    // Next line is uric check
#define HEX_CHECK (*buf == '%' && buf+2 <= endbuf && hex_map[*(buf+1)] && hex_map[*(buf+2)])
    if (reserve_unreserve_map[*buf]) {
      buf++;
    } else if( HEX_CHECK ) {
      buf+=3;
    } else if (*buf!=' ') {
      return ERROR_REQUEST_MALFORMED;
    }
#undef HEX_CHECK
  }
  req->uri_len = buf - start_pos;
  buf++;

  // Initializing Request-Version
  req->request_version = buf;
  start_pos = buf;

  // HTTP/d.dCRLF string is at least 9 character length.
  if(buf + 9 < endbuf && *buf == 'H' && *(buf+1) == 'T' && *(buf+2) == 'T' && *(buf+3) == 'P' && *(buf+4) == '/') {
    buf+=5;
    int32_t m = http_version_parse_machine(buf, endbuf);
    if(m==-1) return ERROR_REQUEST_MALFORMED;

    buf+=m;
    if (*(buf) != CR && *(buf +1) != LF) {
      return ERROR_REQUEST_MALFORMED;
    }
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
  req->request_version_len = buf - start_pos;
  buf += 2;

  // Initializing Request-Headers
  req->request_headers = buf;
  start_pos = buf;
  while (buf - 1 <= endbuf) {
    if (buf > endbuf) return ERROR_REQUEST_MALFORMED;

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
                                           nhttp_request_t *req) {
  if (buf_len <= 0) return ERROR_REQUEST_MALFORMED;

  raw_http_request request;
  request.request = buf;
  request.request_len = buf_len;

  n_parse_error_t parse_error = STATUS_OK;
  parse_error = pre_process_request(&request);
  if (parse_error < 0) return parse_error;

  req->method = request.method;
  req->method_len = request.method_len;

  return STATUS_OK;
}

#undef CR
#undef LF
#undef SP
