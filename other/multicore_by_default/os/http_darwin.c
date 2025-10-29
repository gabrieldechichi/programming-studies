#include "../lib/common.h"
#include "os.h"

/*!
 * HTTP Support for Darwin platforms (macOS and iOS)
 * Forward declarations for Objective-C helper functions implemented in http_darwin_helpers.m
 * */
void* http_darwin_request_create(int method, const char *url, int url_len,
                                 const char *headers, int headers_len, 
                                 const char *body, int body_len);
int http_darwin_request_check(void* request);
int http_darwin_request_get_info(void* request, int32 *status_code, 
                                 int32 *headers_len, int32 *body_len);
int http_darwin_request_get_body(void* request, char *buffer, int32 buffer_len);
void http_darwin_request_free(void* request);

void* http_darwin_stream_create(int method, const char *url, int url_len,
                                const char *headers, int headers_len,
                                const char *body, int body_len);
int http_darwin_stream_check(void* stream);
int http_darwin_stream_get_info(void* stream, int32 *status_code);
int http_darwin_stream_get_chunk_size(void* stream);
int http_darwin_stream_get_chunk(void* stream, char *buffer, int32 buffer_len, 
                                 bool32 *is_final);
void http_darwin_stream_free(void* stream);

// Storage for active requests and streams
static void* g_http_requests[1024] = {0};
static void* g_http_streams[1024] = {0};
static int32 g_next_request_id = 1;
static int32 g_next_stream_id = 1;

PlatformHttpRequestOp os_start_http_request(HttpMethod method, const char *url, int url_len,
                                                  const char *headers, int headers_len,
                                                  const char *body, int body_len) {
    int32 request_id = g_next_request_id++;
    if (request_id >= 1024) request_id = g_next_request_id = 1;
    
    if (g_http_requests[request_id]) {
        http_darwin_request_free(g_http_requests[request_id]);
    }
    
    g_http_requests[request_id] = http_darwin_request_create(
        method, url, url_len, headers, headers_len, body, body_len);
    
    if (!g_http_requests[request_id]) {
        return -1;
    }
    
    return request_id;
}

HttpOpState os_check_http_request(PlatformHttpRequestOp op_id) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_requests[op_id]) {
        return HTTP_OP_ERROR;
    }
    
    return (HttpOpState)http_darwin_request_check(g_http_requests[op_id]);
}

int32 os_get_http_response_info(PlatformHttpRequestOp op_id, int32 *status_code,
                                      int32 *headers_len, int32 *body_len) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_requests[op_id]) {
        return -1;
    }
    
    return http_darwin_request_get_info(g_http_requests[op_id], 
                                        status_code, headers_len, body_len);
}

int32 os_get_http_headers(PlatformHttpRequestOp op_id, char *buffer, int32 buffer_len) {
    UNUSED(op_id);
    UNUSED(buffer);
    UNUSED(buffer_len);
    // Not implemented yet
    return 0;
}

int32 os_get_http_body(PlatformHttpRequestOp op_id, char *buffer, int32 buffer_len) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_requests[op_id]) {
        return -1;
    }
    
    return http_darwin_request_get_body(g_http_requests[op_id], buffer, buffer_len);
}

PlatformHttpStreamOp os_start_http_stream(HttpMethod method, const char *url, int url_len,
                                               const char *headers, int headers_len,
                                               const char *body, int body_len) {
    int32 stream_id = g_next_stream_id++;
    if (stream_id >= 1024) stream_id = g_next_stream_id = 1;
    
    if (g_http_streams[stream_id]) {
        http_darwin_stream_free(g_http_streams[stream_id]);
    }
    
    g_http_streams[stream_id] = http_darwin_stream_create(
        method, url, url_len, headers, headers_len, body, body_len);
    
    if (!g_http_streams[stream_id]) {
        return -1;
    }
    
    return stream_id;
}

HttpStreamState os_check_http_stream(PlatformHttpStreamOp op_id) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_streams[op_id]) {
        return HTTP_STREAM_ERROR;
    }
    
    return (HttpStreamState)http_darwin_stream_check(g_http_streams[op_id]);
}

int32 os_get_http_stream_info(PlatformHttpStreamOp op_id, int32 *status_code) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_streams[op_id]) {
        return -1;
    }
    
    return http_darwin_stream_get_info(g_http_streams[op_id], status_code);
}

int32 os_get_http_stream_chunk_size(PlatformHttpStreamOp op_id) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_streams[op_id]) {
        return 0;
    }
    
    return http_darwin_stream_get_chunk_size(g_http_streams[op_id]);
}

int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                                     int32 buffer_len, bool32 *is_final) {
    if (op_id <= 0 || op_id >= 1024 || !g_http_streams[op_id]) {
        return -1;
    }
    
    return http_darwin_stream_get_chunk(g_http_streams[op_id], 
                                        buffer, buffer_len, is_final);
}