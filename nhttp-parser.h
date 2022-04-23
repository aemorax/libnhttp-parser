/*
 * Copyright (c) 2022 Nevergarden
 */

#ifndef NHTTP_PARSER__H
#define NHTTP_PARSER__H

#if !defined(__WINDOWS__) && \
    (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#error "Windows is shit not supported yet"
#endif

#if (defined(__GNUC__)) && defined(NHTTP_API_VISIBLE)
#define NHTTP_PUBLIC(type) __attribute__((visibility("default"))) type
#else
#define NHTTP_PUBLIC(type) type
#endif

#if defined(__GNUC__) || defined(__clang__) || defined(__TINTC__)
#define N_PACK_TIGHT __attribute__((packed))
#else
#define N_PACK_TIGHT
#endif

#define NHTTP_VERSION_MAJOR 0
#define NHTTP_VERSION_MINOR 1
#define NHTTP_VERSION_PATCH 0

#include <stddef.h>
#include <stdint.h>

typedef enum parse_status {
  ERROR_REQUEST_MALFORMED = -1,
  STATUS_OK = 0
} n_parse_error_t;

typedef struct N_PACK_TIGHT nhttp_request_header {
  const char* key;
  size_t key_len;
  const char* value;
  size_t value_len;
} nhttp_request_header_t;

typedef struct N_PACK_TIGHT nhttp_raw_request {
  // Method
  const char* method;
  size_t method_len;

  // URI
  const char* uri;
  size_t uri_len;

  // Version
  uint8_t version_major;
  uint8_t version_minor;

  // Headers
  const char* header;
  size_t header_len;
  size_t headers_count;

  // Message
  const char* message;
  size_t message_len;
} nhttp_raw_request_t;

#undef N_PACK_TIGHT
const n_parse_error_t parse_headers(nhttp_raw_request_t* req, nhttp_request_header_t * headers);
const n_parse_error_t parse_request(const char* buf, const char* endbuf,
                                          nhttp_raw_request_t* req);

#endif
