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

// 33, 36, 38, 40, 41, 42, 43, 44, 45
// 46, [48-57], 58, 61, 64, [65,90]
// 95, [97-122], 126
static const int8_t pchar_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 1, 0, 0, 1, 0, 1, 0,  // 30-39
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1,  // 40-49
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0,  // 50-59
    0, 1, 0, 0, 1, 1, 1, 1, 1, 1,  // 60-69
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80-89
    1, 0, 0, 0, 0, 1, 0, 1, 1, 1,  // 90-99
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 100-109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 110-119
    1, 1, 1, 0, 0, 0, 1, 0         // 120-127
};

static const int8_t digit_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 30-39
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  // 40-49
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0,  // 50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60-69
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 000-009
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 110-119
    0, 0, 0, 0, 0, 0, 0, 0         // 120-127
};


static const int8_t alpha_map[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10-19
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 30-39
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 50-59
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  // 60-69
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70-79
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80-89
    1, 0, 0, 0, 0, 0, 0, 1, 1, 1,  // 90-99
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 100-109
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 110-119
    1, 1, 1, 0, 0, 0, 0, 0         // 120-127
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

// returns buffer movement or zero on wrong match
static const int8_t pchar_machine(const char * buf) {
  // check if escaped
  if(*buf=='%') {
    if(hex_map[*(buf+1)]) {
      if(hex_map[*(buf+2)]) {
        return 3;
      }
    } else {
      return 0;
    }
  } else if (pchar_map[*buf]) {
    return 1;
  }
  return 0;
}

static const int8_t scheme_test(const char buf) {
  // maybe turn alpha_map into alpha_num_map and check two together
  return alpha_map[buf] || (buf > 47 && buf < 57) || buf == '+' || buf == '-' || buf =='.';
}

static const uint32_t scheme_machine(const char * buf, const char * end_buf) {
  unsigned int pos = 0;
  if(buf == end_buf) {
    return 0;
  } //unlikly 
  // first must be alpha
  if((*buf > 64 && *buf < 91 ) || (*buf > 96 && *buf < 123)) {
    pos++;
    while(buf+pos<=end_buf || scheme_test(*(buf+pos))) {
      pos++;
    }
    return pos;
  }
  return pos;
}

static const uint32_t userinfo_parse_machine(const char *buf, const char * end_buf) {
  uint32_t movement = 0;
  const char * c;
  while(buf+movement<end_buf)
  {
    c = (buf+movement);
    if(pchar_map[*c] || *c == ';') {
      if(*c=='@')
        return movement-1;
      movement++;
    } else if(*c == '%') {
        if(hex_map[*(c+1)] && hex_map[*(c+2)]) {
          movement+=2;
        } else {
          return 0;
        }
    } else {
        return 0;
    }
  }
  return movement;
}

#define DIGIT_DOT(next_state)     \
  if (buf + movement < end_buf) { \
    c = *(buf + movement);        \
    if (digit_map[c]) {           \
      movement++;                 \
      goto next_state;            \
    } else if (c == '.') {        \
      movement++;                 \
    } else {                      \
      return 0;                   \
    }                             \
  }

#define DIGIT_ONLY                \
  if (buf + movement < end_buf) { \
    c = *(buf + movement);        \
    if (digit_map[c]) {           \
      movement++;                 \
    } else {                      \
      return 0;                   \
    }                             \
  }

static const uint32_t ipv4address_parse_machine(const char * buf, const char * end_buf) {
  uint32_t movement = 0;
  char c = *(buf);
STATE_A:
  if(digit_map[c]) {
    movement++;
  } else {
    return 0;
  }
STATE_B:
  DIGIT_DOT(STATE_B)
STATE_C:
  DIGIT_ONLY
STATE_D:
  DIGIT_DOT(STATE_D)
STATE_E:
  DIGIT_ONLY
STATE_F:
  DIGIT_DOT(STATE_F)
STATE_G:
  DIGIT_ONLY
STATE_H:
  c = *(buf+movement);
  if(digit_map[c]) {
    movement++;
    goto STATE_H;
  }
  return movement;
}

#undef DIGIT_DOT
#undef DIGIT_ONLY


static const uint32_t domainlabel_parse_machine(const char* buf, const char * end_buf) {
  uint32_t movement = 0;
  char c = *(buf);
STATE_A:
  c = *(buf+movement);
  if(digit_map[c] || alpha_map[c]) {
    movement++;
  } else {
    return 0;
  }
STATE_B:
  c = *(buf+movement);
  if(digit_map[c] || alpha_map[c]) {
    movement++;
    goto STATE_B;
  } else if (c == '-') {
    movement++;
    goto STATE_A;
  }
  return movement;
}

static const uint32_t hostname_parse_machine(const char* buf, const char*end_buf) {
  uint32_t movement = 0;
  
  return movement;
}

static const uint32_t hostport_parse_machine(const char* buf, const char* end_buf) {
  uint32_t movement = 0;
  movement+=hostname_parse_machine(buf, end_buf);
  return movement;
}

// server = [ [userinfo "@"] hostport ]
static const uint32_t server_parse_machine(const char* buf, const char* end_buf) {
  uint32_t movement = 0;
  movement+=userinfo_parse_machine(buf, end_buf);
  if(!movement && buf < end_buf && *buf=='@') {
    // if userinfo was null @ can't be the first char
    return 0;
  }

  movement+=hostport_parse_machine(buf+movement, end_buf);

  return movement;
}

static const uint32_t authority_machine(const char * buf, const char * end_buf) {
  uint32_t movement = 0;
  
  return movement;
}

static const uint32_t net_path_machine(const char * buf, const char * end_buf) {
  uint32_t movement = 0;
  if(buf <= end_buf) {
    if(*(buf+movement)=='/') {
      movement++;
      if(buf+movement <= end_buf) {
        if(*(buf+movement) == '/') {
          movement++;
          authority_machine(buf+movement, end_buf);
        }
      }
    }
  }
  return movement;
}

// / ( / authority [ abs_path ] | path_segments) [ ? query ]
static const uint32_t hier_part_machine(const char * buf, const char * end_buf) {
  uint32_t movement = 0;
  if(buf <= end_buf) {
    movement += net_path_machine(buf+movement, end_buf);
  }
  return movement;
}

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

static const n_parse_error_t pre_process_request(raw_http_request *req) {
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

static const n_parse_error_t validate_raw_request_method(
    raw_http_request *request) {
  const char *method_b = request->method;
  const char *method_e = request->method + request->method_len;
  while (method_b != method_e) {
    if (!is_token(*method_b)) return ERROR_REQUEST_METHOD_MALFORMED;
    method_b++;
  }
  return STATUS_OK;
}

static const n_parse_error_t parse_request_uri(raw_http_request * request) {
  const char * buf = request->uri;
  const char * end_buf = request->uri+request->uri_len;
  uint32_t movement = 0;

  // absoluteURI
  movement = scheme_machine(buf, end_buf);
  if(movement) {
    // only in absoluteURI if this fails all fail
    if(*(buf+movement) == ':') {
      // hier_part
      
    } else {
      movement = 0;
    }
  }
  // relativeURI
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

  parse_error = validate_raw_request_method(&request);
  if (parse_error < 0) return parse_error;
  req->method = request.method;
  req->method_len = request.method_len;

  return STATUS_OK;
}

#include <string.h>
#include <stdio.h>

int main() {
  const char * buf = "129.129.19291.129";
  const char * buf_end = buf+(strlen(buf));

  printf("%d\n", domainlabel_parse_machine(buf, buf_end));
}

#undef CR
#undef LF
#undef SP
