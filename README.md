# libnhttp-parser
A simple http request (only) parser library written in C.  
The main purpose of this library is to be used in web application frameworks, capable of parsing method, uri, http version and headers using state machine and fast maps for comparision.

## Usage
**libnhttp-parser** exposes an enumerator for parse status:
```c
typedef enum parse_status {
  ERROR_REQUEST_MALFORMED = -1,
  STATUS_OK = 0
} n_parse_error_t;
`
negative values for parse error and `STATUS_OK` for a complete parse.

There are two structures exposed to use:
```c
// Header parse result
typedef struct nhttp_request_header {
  // Header name
  const char* key;
  size_t key_len;

  // Header value
  const char* value;
  size_t value_len;
} nhttp_request_header_t;

// Request parse result
typedef struct nhttp_raw_request {
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
} nhttp_raw_request_t
```

Because libnhttp-parser does not allocate any memory, to parse headers you have to know headers buffer location, length and header count and pass it to parser.
`parse_request` function finds these data as it preprocesses the headers so to completely parse a request use:

```c
#include <stdio.h>
#include <string.h>
#include <nhttp-parser.h>

int main() {
  const char * buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  nhttp_raw_request_t request;

  if(parse_request(buffer, buffer+strlen(buffer), &request) == 0) {
    printf("%.*s\n", (int)request.method_len, request.method);
    printf("%.*s\n", (int)request.uri_len, request.uri);

    nhttp_request_header_t headers[request.headers_count];
    if(parse_headers(&request, headers) == 0) {
      int current_header = 0;
      while(current_header < request.headers_count) {
        printf("Name: %.*s, Value: %.*s", 
          (int) headers[current_header].key_len,
          headers[current_header].key,
          (int) headers[current_header].value_len,
          headers[current_header].value);
      }
    }
  }
}
```
