#ifndef H_HTTP
#define H_HTTP

#include "os/os.h"
#include "memory.h"
#include "typedefs.h"

typedef struct {
  int32 status_code;
  bool32 success;
  char *body;
  uint32 body_len;
  char *error_message;
} HttpResponse;

typedef struct HttpRequest {
  PlatformHttpRequestOp os_op;
  Allocator *arena;
  HttpResponse response;
  bool32 response_ready;
} HttpRequest;

typedef struct HttpRequest HttpRequest;

// Streaming HTTP types
typedef struct {
  char *chunk_data;
  uint32 chunk_len;
  bool32 is_final_chunk;
} HttpStreamChunk;

typedef struct {
  PlatformHttpStreamOp os_op;
  Allocator *arena;
  bool32 stream_ready;
  bool32 stream_complete;
  bool32 has_error;
  char *error_message;
  uint32 total_bytes_received;
  int32 status_code;
} HttpStreamRequest;

// Regular HTTP functions
HttpRequest http_get_async(const char *url, Allocator *arena);

HttpRequest http_post_async(const char *url, const char *headers,
                            const char *body, Allocator *arena);

bool32 http_request_is_complete(HttpRequest *request);

HttpResponse http_request_get_response(HttpRequest *request);

void http_response_free(HttpResponse *response, Allocator *arena);

// Streaming HTTP functions
HttpStreamRequest http_stream_get_async(const char *url, Allocator *arena);

HttpStreamRequest http_stream_post_async(const char *url, const char *headers,
                                         const char *body, Allocator *arena);

HttpStreamRequest http_stream_post_binary_async(const char *url,
                                                const char *headers,
                                                const char *body,
                                                uint32 body_len,
                                                Allocator *arena);

bool32 http_stream_is_ready(HttpStreamRequest *request);

bool32 http_stream_has_chunk(HttpStreamRequest *request);

HttpStreamChunk http_stream_get_chunk(HttpStreamRequest *request);

bool32 http_stream_is_complete(HttpStreamRequest *request);

bool32 http_stream_has_error(HttpStreamRequest *request);

#endif