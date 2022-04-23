#include "nhttp-parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CR 13
#define LF 10
#define SP 32
#define HT 9

static const int8_t token_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0};

// 33, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
// 58, 59 61, 63, 64, 95, 126
static const int8_t reserve_unreserve_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0};

// [48-57], [65-70], [97-102]
static const int8_t hex_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// [48-57]
static const int8_t digit_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const int8_t text_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0};

static const uint32_t predict_http_headers_count(const char *buf,
                                                 const char *endbuff) {
  uint32_t count = 0;

  while (buf < endbuff) {
    if (*(buf) == CR && *(buf + 1) == LF) {
      count++;
      buf++;
    }
    buf++;
  }
  return count;
}

static const int32_t http_version_parse_machine(const char *buf,
                                                const char *endbuf,
                                                nhttp_raw_request_t *req) {
  int32_t movement = 0;
  char c = *buf;
  int32_t minor_pos = 0;
STATE_A:
  if (digit_map[c]) {
    movement++;
  } else {
    return -1;
  }
STATE_B:
  c = *(buf + movement);
  if (digit_map[c]) {
    movement++;
    goto STATE_B;
  } else if (c == '.') {
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
  c = *(buf + movement);
  if (digit_map[c]) {
    movement++;
    goto STATE_C;
  }
  char minor[movement - minor_pos];
  memcpy(&minor, buf + minor_pos, movement - minor_pos);
  req->version_minor = atoi(minor);

  return movement;
}

const n_parse_error_t parse_headers(nhttp_raw_request_t *req, nhttp_request_header_t * headers) {
  const char * buf = req->header;
  const char * endbuf = req->header+req->header_len;
  uint32_t count = 0;
  uint32_t key_size = 0;
  uint32_t value_size = 0;

  while(count < req->headers_count) {
STATE_A:
  if(buf > endbuf)
    return ERROR_REQUEST_MALFORMED;

  if(token_map[*buf]) {
    headers[count].key = buf;
    key_size++;
    buf++;
    goto STATE_B;
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
STATE_B:
  if(buf > endbuf)
    return ERROR_REQUEST_MALFORMED;

  if(token_map[*buf]) {
    key_size++;
    buf++;
    goto STATE_B;
  }
  else if(*buf == ':') {
    headers[count].key_len = key_size;
    key_size = 0;
    buf++;
    goto STATE_C;
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
STATE_C:
  if(buf > endbuf)
    return ERROR_REQUEST_MALFORMED;

  if(text_map[*buf]) {
    headers[count].value = buf;
    value_size++;
    buf++;
    goto STATE_D;
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
STATE_D:
  if(buf > endbuf)
    return ERROR_REQUEST_MALFORMED;

  if(text_map[*buf]) {
    value_size++;
    buf++;
    goto STATE_D;
  } else if(*buf == CR && *(buf+1) == LF) {
    headers[count].value_len = value_size;
    value_size = 0; 
    buf+=2;
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
  count++;
  }
    
  return STATUS_OK;
}

const n_parse_error_t parse_request(const char *buf, const char *endbuf,
                                          nhttp_raw_request_t *req) {
  const char *start_pos = buf;

  // Initializing Method
  req->method = buf;
  while (*buf != SP) {
    if (*buf == CR || buf == endbuf || !token_map[*buf])
      return ERROR_REQUEST_MALFORMED;
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
#define HEX_CHECK                                             \
  (*buf == '%' && buf + 2 <= endbuf && hex_map[*(buf + 1)] && \
   hex_map[*(buf + 2)])
    if (reserve_unreserve_map[*buf]) {
      buf++;
    } else if (HEX_CHECK) {
      buf += 3;
    } else if (*buf != ' ') {
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
  if (buf + 9 < endbuf && *buf == 'H' && *(buf + 1) == 'T' &&
      *(buf + 2) == 'T' && *(buf + 3) == 'P' && *(buf + 4) == '/') {
    buf += 5;
    int32_t m = http_version_parse_machine(buf, endbuf, req);
    if (m == -1) return ERROR_REQUEST_MALFORMED;

    buf += m;
    if (*(buf) != CR && *(buf + 1) != LF) {
      return ERROR_REQUEST_MALFORMED;
    }
  } else {
    return ERROR_REQUEST_MALFORMED;
  }
  // req->request_version_len = buf - start_pos;
  buf += 2;

  // Initializing Request-Headers
  req->header = buf;
  start_pos = buf;

STATE_A:
  if (buf > endbuf) return ERROR_REQUEST_MALFORMED;
  if (*buf == CR && *(buf + 1) == LF && buf + 1 < endbuf) goto END;
  buf++;
STATE_B:
  if (buf > endbuf) return ERROR_REQUEST_MALFORMED;
  if (*buf == CR && *(buf + 1) == LF && buf + 1 < endbuf) {
    buf += 2;
    goto STATE_A;
  }
  buf++;
  goto STATE_B;
END:
  req->header_len = buf - start_pos;
  buf += 2;

  req->headers_count =
      predict_http_headers_count(req->header, req->header + req->header_len);

  req->message = buf;
  start_pos = buf;
  while (buf < endbuf) {
    buf++;
  }
  req->message_len = buf - start_pos;

  return STATUS_OK;
}

#undef CR
#undef LF
#undef SP
