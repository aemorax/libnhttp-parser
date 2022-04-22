/*
 * Copyright (c) 2022 Nevergarden
 */

#ifndef NHTTP_PARSER__H
#define NHTTP_PARSER__H

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32) )
#define __WINDOWS__
#error "Windows is shit not supported yet"
#endif

#if (defined (__GNUC__)) && defined(NHTTP_API_VISIBLE)
#define NHTTP_PUBLIC(type) __attribute__((visibility("default"))) type
#else
#define NHTTP_PUBLIC(type) type
#endif

#if defined (__GNUC__) || defined (__clang__) || defined (__TINTC__)
#define N_PACK_TIGHT __attribute__ ((packed))
#else
#define N_PACK_TIGHT
#endif

#define NHTTP_VERSION_MAJOR 0
#define NHTTP_VERSION_MINOR 1
#define NHTTP_VERSION_PATCH 0

#include <stddef.h>
#include <stdint.h>

typedef struct N_PACK_TIGHT nhttp_request_header {
  const char * key;
  size_t key_len;
  const char * value;
  size_t value_len;
} nhttp_request_header_t;

typedef struct N_PACK_TIGHT nhttp_request {
  // Method
  const char * method;
  size_t method_len;
  
  // URI
  const char * uri;
  size_t uri_len;

  // Version
  uint8_t version_major;
  uint8_t version_minor;

  // Headers
  const nhttp_request_header_t * headers;
  size_t headers_len;

  // Message
  const char * message;
  size_t message_len;
} nhttp_request_t;

#undef N_PACK_TIGHT

#endif
