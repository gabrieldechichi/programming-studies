#include "http.h"
#include "os/os.h"
#include "fmt.h"
#include "lib/string.h"

HttpRequest http_get_async(const char *url, Allocator *arena) {
  HttpRequest request = {0};
  request.arena = arena;

  LOG_INFO("Sending http request %", FMT_STR(url));
  request.os_op = os_start_http_request(
      HTTP_METHOD_GET, url, str_len(url), "", 0, "", 0);

  return request;
}

HttpRequest http_post_async(const char *url, const char *headers,
                            const char *body, Allocator *arena) {
  HttpRequest request = {0};
  request.arena = arena;

  LOG_INFO("Sending http POST request %", FMT_STR(url));
  request.os_op = os_start_http_request(
      HTTP_METHOD_POST, url, str_len(url), headers ? headers : "",
      headers ? str_len(headers) : 0, body ? body : "", body ? str_len(body) : 0);

  return request;
}

bool32 http_request_is_complete(HttpRequest *request) {
  if (!request) {
    return true;
  }

  if (request->response_ready) {
    return true;
  }

  HttpOpState state = os_check_http_request(request->os_op);

  if (state == HTTP_OP_COMPLETED || state == HTTP_OP_ERROR) {
    if (state == HTTP_OP_COMPLETED) {
      int32 status_code, headers_len, body_len;
      int32 result = os_get_http_response_info(
          request->os_op, &status_code, &headers_len, &body_len);

      if (result == 0) {
        request->response.status_code = status_code;
        request->response.success = (status_code >= 200 && status_code < 300);

        if (body_len > 0) {
          request->response.body =
              ALLOC_ARRAY(request->arena, char, body_len + 1);
          if (request->response.body) {
            int32 body_result = os_get_http_body(
                request->os_op, request->response.body, body_len);

            if (body_result == 0) {
              request->response.body_len = body_len;
              request->response.body[body_len] = '\0';
            } else {
              request->response.error_message =
                  "Failed to retrieve response body";
              request->response.success = false;
            }
          } else {
            request->response.error_message =
                "Failed to allocate memory for response body";
            request->response.success = false;
          }
        }
      } else {
        request->response.error_message = "Failed to get HTTP response info";
        request->response.success = false;
      }
    } else {
      request->response.error_message = "HTTP request failed";
      request->response.success = false;
    }

    request->response_ready = true;
    return true;
  }

  return false;
}

HttpResponse http_request_get_response(HttpRequest *request) {
  if (!request) {
    HttpResponse error_response = {0};
    error_response.error_message = "Invalid request";
    return error_response;
  }

  if (!request->response_ready) {
    HttpResponse error_response = {0};
    error_response.error_message = "Request not complete";
    return error_response;
  }

  return request->response;
}

void http_response_free(HttpResponse *response, Allocator *arena) {
  UNUSED(response);
  UNUSED(arena);
}

HttpStreamRequest http_stream_get_async(const char *url, Allocator *arena) {
  HttpStreamRequest request = {0};
  request.arena = arena;

  LOG_INFO("Starting http stream request %", FMT_STR(url));
  request.os_op = os_start_http_stream(
      HTTP_METHOD_GET, url, str_len(url), "", 0, "", 0);

  return request;
}

HttpStreamRequest http_stream_post_async(const char *url, const char *headers,
                                         const char *body, Allocator *arena) {
  HttpStreamRequest request = {0};
  request.arena = arena;

  LOG_INFO("Starting http stream POST request %", FMT_STR(url));
  request.os_op = os_start_http_stream(
      HTTP_METHOD_POST, url, str_len(url), headers,
      headers ? str_len(headers) : 0, body, body ? str_len(body) : 0);

  return request;
}

HttpStreamRequest http_stream_post_binary_async(const char *url,
                                                const char *headers,
                                                const char *body,
                                                uint32 body_len,
                                                Allocator *arena) {
  HttpStreamRequest request = {0};
  request.arena = arena;

  LOG_INFO("Starting http stream POST binary request % (% bytes)", FMT_STR(url),
           FMT_UINT(body_len));
  request.os_op =
      os_start_http_stream(HTTP_METHOD_POST, url, str_len(url), headers,
                                 headers ? str_len(headers) : 0, body, body_len);

  return request;
}

bool32 http_stream_is_ready(HttpStreamRequest *request) {
  if (!request) {
    return false;
  }

  if (request->stream_ready || request->has_error) {
    return true;
  }

  HttpStreamState state = os_check_http_stream(request->os_op);

  if (state == HTTP_STREAM_READY || state == HTTP_STREAM_HAS_CHUNK) {
    if (!request->stream_ready) {
      int32 status_code;
      int32 result =
          os_get_http_stream_info(request->os_op, &status_code);
      if (result == 0) {
        request->status_code = status_code;
        request->stream_ready = true;
      } else {
        request->has_error = true;
        request->error_message = "Failed to get stream response info";
      }
    }
    return true;
  } else if (state == HTTP_STREAM_ERROR) {
    request->has_error = true;
    request->error_message = "HTTP stream failed";
    return true;
  }

  return false;
}

bool32 http_stream_has_chunk(HttpStreamRequest *request) {
  if (!http_stream_is_ready(request)) {
    return false;
  }
  if (http_stream_is_complete(request)) {
    return false;
  }
  if (!request || request->has_error || request->stream_complete) {
    return false;
  }

  HttpStreamState state = os_check_http_stream(request->os_op);
  return (state == HTTP_STREAM_HAS_CHUNK);
}

HttpStreamChunk http_stream_get_chunk(HttpStreamRequest *request) {
  HttpStreamChunk chunk = {0};

  if (!request || request->has_error || request->stream_complete) {
    return chunk;
  }

  int32 chunk_size = os_get_http_stream_chunk_size(request->os_op);
  if (chunk_size <= 0) {
    return chunk;
  }

  chunk.chunk_data = ALLOC_ARRAY(request->arena, char, chunk_size);
  if (!chunk.chunk_data) {
    request->has_error = true;
    request->error_message = "Failed to allocate memory for stream chunk";
    return chunk;
  }

  bool32 is_final;
  int32 result = os_get_http_stream_chunk(
      request->os_op, chunk.chunk_data, chunk_size, &is_final);

  if (result == 0) {
    chunk.chunk_len = chunk_size;
    chunk.is_final_chunk = is_final;
    request->total_bytes_received += chunk_size;

    if (is_final) {
      request->stream_complete = true;
    }
  } else {
    request->has_error = true;
    request->error_message = "Failed to retrieve stream chunk";
  }

  return chunk;
}

bool32 http_stream_is_complete(HttpStreamRequest *request) {
  if (!request) {
    return true;
  }

  if (request->stream_complete || request->has_error) {
    return true;
  }

  HttpStreamState state = os_check_http_stream(request->os_op);
  if (state == HTTP_STREAM_COMPLETE) {
    request->stream_complete = true;
    return true;
  }

  return false;
}

bool32 http_stream_has_error(HttpStreamRequest *request) {
  if (!request) {
    return true;
  }

  return request->has_error;
}