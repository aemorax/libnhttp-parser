/*
 * Copyright (c) 2022 Nevergarden
 */

#ifndef NHTTP_PARSER__H
#define NHTTP_PARSER__H

//#ifdef __cplusplus
//extern "C" {
//#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32) )
#define __WINDOWS__
#error "Windows is shit not supported yet"
#endif

#if (defined (__GNUC__)) && defined(NHTTP_API_VISIBLE)
#define NHTTP_PUBLIC(type) __attribute__((visibility("default"))) type
#else
#define NHTTP_PUBLIC(type) type
#endif

#define NHTTP_VERSION_MAJOR 0
#define NHTTP_VERSION_MINOR 1
#define NHTTP_VERSION_PATCH 0

#include <stddef.h>

typedef struct nhttp_parser_tokenizer {
  /* value of current index */
  int current;
  char * string;
  size_t string_size;
} nhttp_parser_tokenizer;

typedef enum token_type {
  TSPECIALS, // "(" | ")" | "<" | ">" | "@" | 
             // "," | ";" | ":" | "\" | <"> |
             // "/" | "[" | "]" | "?" | "=" |
             // "{" | "}" | SP | HT
} token_type;

typedef struct nhttp_token {
  nhttp_parser_tokenizer tokenizer;
  char * string;
  size_t string_size;
  token_type type;
} nhttp_token;

//#ifdef __cplusplus
//}
//#endif

#endif
