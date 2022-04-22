#include "nhttp-parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

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

static const int32_t http_version_parse_machine(const char * buf, const char * endbuf, nhttp_request_t * req) {
  int32_t movement = 0;
  char c = *buf;
  int32_t minor_pos = 0;
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
    // Special action to parse version to uint8
    char major[movement];
    memcpy(&major, buf, movement);
    req->version_major = atoi(major);
    movement++;
    minor_pos = movement;
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
  char minor[movement-minor_pos];
  memcpy(&minor, buf+minor_pos, movement-minor_pos);
  req->version_minor = atoi(minor);

  return movement;
}

static const n_parse_error_t process_request(const char * buf, const char * endbuf, nhttp_request_t * req) {
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
  // req->request_version = buf;
  start_pos = buf;

  // HTTP/d.dCRLF string is at least 9 character length.
  if(buf + 9 < endbuf && *buf == 'H' && *(buf+1) == 'T' && *(buf+2) == 'T' && *(buf+3) == 'P' && *(buf+4) == '/') {
    buf+=5;
    int32_t m = http_version_parse_machine(buf, endbuf, req);
    if(m==-1) return ERROR_REQUEST_MALFORMED;

    buf+=m;
    if (*(buf) != CR && *(buf +1) != LF) {
      return ERROR_REQUEST_MALFORMED;
    }
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
  // req->request_version_len = buf - start_pos;
  buf += 2;

  // Initializing Request-Headers
  // req->headers = buf;
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
  // req->request_headers_len = buf - start_pos;
  buf += 2;

  req->message = buf;
  start_pos = buf;
  do {
    buf++;
  } while (buf <= endbuf);
  buf--;
  req->message_len = buf - start_pos;

  return STATUS_OK;
}


static const n_parse_error_t parse_request(const char *buf, size_t buf_len,
                                           nhttp_request_t *req) {
  if (buf_len <= 0) return ERROR_REQUEST_MALFORMED;

  n_parse_error_t parse_error = STATUS_OK;
  parse_error = process_request(buf, buf+buf_len, req);
  if (parse_error < 0) return parse_error;

  return STATUS_OK;
}

#undef CR
#undef LF
#undef SP
