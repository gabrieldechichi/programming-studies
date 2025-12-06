#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#pragma pop_macro("global")
#pragma pop_macro("internal")

enum {
    HTTP_OP_NONE_NATIVE = 0,
    HTTP_OP_IN_PROGRESS_NATIVE = 1,
    HTTP_OP_COMPLETED_NATIVE = 2,
    HTTP_OP_ERROR_NATIVE = 3
};

enum {
    HTTP_STREAM_NOT_STARTED_NATIVE = 0,
    HTTP_STREAM_READY_NATIVE = 1,
    HTTP_STREAM_HAS_CHUNK_NATIVE = 2,
    HTTP_STREAM_COMPLETE_NATIVE = 3,
    HTTP_STREAM_ERROR_NATIVE = 4
};

@interface HttpRequestNative : NSObject
@property (nonatomic, assign) int statusCode;
@property (nonatomic, strong) NSData *responseData;
@property (nonatomic, strong) NSDictionary *responseHeaders;
@property (nonatomic, assign) int state;
@property (nonatomic, strong) dispatch_semaphore_t semaphore;
@end

@implementation HttpRequestNative
- (instancetype)init {
    self = [super init];
    if (self) {
        self.state = HTTP_OP_IN_PROGRESS_NATIVE;
        self.semaphore = dispatch_semaphore_create(0);
    }
    return self;
}
@end

@interface HttpStreamNative : NSObject <NSURLSessionDataDelegate>
@property (nonatomic, assign) int statusCode;
@property (nonatomic, strong) NSMutableArray<NSData*> *chunks;
@property (nonatomic, assign) int state;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, assign) BOOL streamComplete;
@property (nonatomic, assign) BOOL hasError;
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) NSURLSessionDataTask *task;
@end

@implementation HttpStreamNative
- (instancetype)init {
    self = [super init];
    if (self) {
        self.state = HTTP_STREAM_NOT_STARTED_NATIVE;
        self.chunks = [NSMutableArray array];
        self.queue = dispatch_queue_create("http_stream_queue", DISPATCH_QUEUE_SERIAL);

        NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
        config.URLCache = nil;
        config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;

        NSOperationQueue *delegateQueue = [[NSOperationQueue alloc] init];
        delegateQueue.maxConcurrentOperationCount = 1;
        self.session = [NSURLSession sessionWithConfiguration:config
                                                     delegate:self
                                                delegateQueue:delegateQueue];
    }
    return self;
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
didReceiveResponse:(NSURLResponse *)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {

    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;

    dispatch_sync(self.queue, ^{
        self.statusCode = (int)[httpResponse statusCode];
        self.state = HTTP_STREAM_READY_NATIVE;
    });

    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {

    dispatch_sync(self.queue, ^{
        [self.chunks addObject:[data copy]];
        self.state = HTTP_STREAM_HAS_CHUNK_NATIVE;
    });
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error {

    dispatch_sync(self.queue, ^{
        if (error) {
            self.hasError = YES;
            self.state = HTTP_STREAM_ERROR_NATIVE;
        } else {
            self.streamComplete = YES;
            if ([self.chunks count] == 0) {
                self.state = HTTP_STREAM_COMPLETE_NATIVE;
            }
        }
    });
}

- (void)dealloc {
    [self.session invalidateAndCancel];
    [super dealloc];
}
@end

internal NSString* http_method_to_string(int method) {
    switch(method) {
        case 0: return @"GET";
        case 1: return @"POST";
        case 2: return @"PUT";
        case 3: return @"DELETE";
        default: return @"GET";
    }
}

internal void http_parse_headers(const char *headers, int headers_len, NSMutableURLRequest *request) {
    if (!headers || headers_len <= 0) return;

    NSString *headersStr = [[NSString alloc] initWithBytes:headers
                                                    length:headers_len
                                                  encoding:NSUTF8StringEncoding];
    NSArray *lines = [headersStr componentsSeparatedByString:@"\n"];

    for (NSString *line in lines) {
        NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (trimmed.length == 0) continue;

        NSRange colonRange = [trimmed rangeOfString:@":"];
        if (colonRange.location != NSNotFound) {
            NSString *key = [trimmed substringToIndex:colonRange.location];
            NSString *value = [[trimmed substringFromIndex:colonRange.location + 1]
                              stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            [request setValue:value forHTTPHeaderField:key];
        }
    }
}

typedef struct {
  void* native_handle;
  b32 in_use;
} HttpRequestOp;

typedef struct {
  void* native_handle;
  b32 in_use;
} HttpStreamOp;

#define MAX_HTTP_OPS 64
static HttpRequestOp g_http_requests[MAX_HTTP_OPS];
static HttpStreamOp g_http_streams[MAX_HTTP_OPS];
static Mutex g_http_mutex = {0};
static b32 g_http_initialized = false;

internal void http_ops_init(void) {
  if (g_http_initialized) return;

  g_http_mutex = mutex_alloc();
  for (i32 i = 0; i < MAX_HTTP_OPS; i++) {
    g_http_requests[i].in_use = false;
    g_http_requests[i].native_handle = NULL;
    g_http_streams[i].in_use = false;
    g_http_streams[i].native_handle = NULL;
  }
  g_http_initialized = true;
}

internal i32 http_request_allocate(void* native_handle) {
  mutex_take(g_http_mutex);
  for (i32 i = 0; i < MAX_HTTP_OPS; i++) {
    if (!g_http_requests[i].in_use) {
      g_http_requests[i].in_use = true;
      g_http_requests[i].native_handle = native_handle;
      mutex_drop(g_http_mutex);
      return i;
    }
  }
  mutex_drop(g_http_mutex);
  return -1;
}

internal i32 http_stream_allocate(void* native_handle) {
  mutex_take(g_http_mutex);
  for (i32 i = 0; i < MAX_HTTP_OPS; i++) {
    if (!g_http_streams[i].in_use) {
      g_http_streams[i].in_use = true;
      g_http_streams[i].native_handle = native_handle;
      mutex_drop(g_http_mutex);
      return i;
    }
  }
  mutex_drop(g_http_mutex);
  return -1;
}

PlatformHttpRequestOp os_start_http_request(HttpMethod method, const char *url,
                                            int url_len, const char *headers,
                                            int headers_len, const char *body,
                                            int body_len) {
  if (!g_http_initialized) {
    http_ops_init();
  }

  HttpRequestNative *request = [[HttpRequestNative alloc] init];

  NSString *urlStr = [[NSString alloc] initWithBytes:url length:url_len encoding:NSUTF8StringEncoding];

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      @autoreleasepool {
          NSMutableURLRequest *urlRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlStr]];
          [urlRequest setHTTPMethod:http_method_to_string(method)];

          http_parse_headers(headers, headers_len, urlRequest);

          if (body && body_len > 0) {
              [urlRequest setHTTPBody:[NSData dataWithBytes:body length:body_len]];
          }

          NSURLSession *session = [NSURLSession sharedSession];
          NSURLSessionDataTask *task = [session dataTaskWithRequest:urlRequest
              completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                  if (error) {
                      request.state = HTTP_OP_ERROR_NATIVE;
                  } else {
                      request.state = HTTP_OP_COMPLETED_NATIVE;
                      NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
                      request.statusCode = (int)[httpResponse statusCode];
                      request.responseData = data;
                      request.responseHeaders = [httpResponse allHeaderFields];
                  }
                  dispatch_semaphore_signal(request.semaphore);
              }];

          [task resume];
      }
  });

  void* native_handle = (void*)CFBridgingRetain(request);
  if (!native_handle) {
    return -1;
  }

  return http_request_allocate(native_handle);
}

HttpOpState os_check_http_request(PlatformHttpRequestOp op_id) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return HTTP_OP_ERROR;
  }

  mutex_take(g_http_mutex);
  if (!g_http_requests[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return HTTP_OP_NONE;
  }

  void* handle = g_http_requests[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return HTTP_OP_ERROR;
  }

  HttpRequestNative *request = (__bridge HttpRequestNative*)handle;
  if (request.state == HTTP_OP_IN_PROGRESS_NATIVE) {
      if (dispatch_semaphore_wait(request.semaphore, DISPATCH_TIME_NOW) == 0) {
          dispatch_semaphore_signal(request.semaphore);
      }
  }

  return (HttpOpState)request.state;
}

int32 os_get_http_response_info(PlatformHttpRequestOp op_id,
                                _out_ int32 *status_code,
                                _out_ int32 *headers_len,
                                _out_ int32 *body_len) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_requests[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_requests[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpRequestNative *request = (__bridge HttpRequestNative*)handle;
  if (request.state != HTTP_OP_COMPLETED_NATIVE && request.state != HTTP_OP_ERROR_NATIVE) {
      return -1;
  }

  *status_code = request.statusCode;

  NSMutableString *headersStr = [NSMutableString string];
  [request.responseHeaders enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
      if (headersStr.length > 0) {
          [headersStr appendString:@"\n"];
      }
      [headersStr appendFormat:@"%@: %@", key, obj];
  }];
  *headers_len = (int)[headersStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

  *body_len = request.responseData ? (int)[request.responseData length] : 0;

  return 0;
}

int32 os_get_http_headers(PlatformHttpRequestOp op_id, char *buffer,
                          int32 buffer_len) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_requests[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_requests[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpRequestNative *request = (__bridge HttpRequestNative*)handle;
  if (!request.responseHeaders) return -1;

  NSMutableString *headersStr = [NSMutableString string];
  [request.responseHeaders enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
      if (headersStr.length > 0) {
          [headersStr appendString:@"\n"];
      }
      [headersStr appendFormat:@"%@: %@", key, obj];
  }];

  const char *headersCStr = [headersStr UTF8String];
  size_t headersLen = strlen(headersCStr);

  if (headersLen > buffer_len) return -1;

  memcpy(buffer, headersCStr, headersLen);
  return 0;
}

int32 os_get_http_body(PlatformHttpRequestOp op_id, char *buffer,
                       int32 buffer_len) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_requests[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_requests[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpRequestNative *request = (__bridge HttpRequestNative*)handle;
  if (!request.responseData) return -1;

  NSUInteger dataLen = [request.responseData length];
  if (dataLen > buffer_len) return -1;

  [request.responseData getBytes:buffer length:dataLen];

  CFRelease(handle);

  mutex_take(g_http_mutex);
  g_http_requests[op_id].native_handle = NULL;
  g_http_requests[op_id].in_use = false;
  mutex_drop(g_http_mutex);

  return 0;
}

PlatformHttpStreamOp os_start_http_stream(HttpMethod method, const char *url,
                                          int url_len, const char *headers,
                                          int headers_len, const char *body,
                                          int body_len) {
  if (!g_http_initialized) {
    http_ops_init();
  }

  HttpStreamNative *stream = [[HttpStreamNative alloc] init];

  NSString *urlStr = [[NSString alloc] initWithBytes:url length:url_len encoding:NSUTF8StringEncoding];

  NSMutableURLRequest *urlRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlStr]];
  [urlRequest setHTTPMethod:http_method_to_string(method)];

  http_parse_headers(headers, headers_len, urlRequest);

  if (body && body_len > 0) {
      [urlRequest setHTTPBody:[NSData dataWithBytes:body length:body_len]];
  }

  stream.task = [stream.session dataTaskWithRequest:urlRequest];
  [stream.task resume];

  void* native_handle = (void*)CFBridgingRetain(stream);
  if (!native_handle) {
    return -1;
  }

  return http_stream_allocate(native_handle);
}

HttpStreamState os_check_http_stream(PlatformHttpStreamOp op_id) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return HTTP_STREAM_ERROR;
  }

  mutex_take(g_http_mutex);
  if (!g_http_streams[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return HTTP_STREAM_NOT_STARTED;
  }

  void* handle = g_http_streams[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return HTTP_STREAM_ERROR;
  }

  HttpStreamNative *stream = (__bridge HttpStreamNative*)handle;

  __block int state;
  dispatch_sync(stream.queue, ^{
      if (stream.hasError) {
          state = HTTP_STREAM_ERROR_NATIVE;
      } else if ([stream.chunks count] > 0) {
          state = HTTP_STREAM_HAS_CHUNK_NATIVE;
      } else if (stream.streamComplete) {
          state = HTTP_STREAM_COMPLETE_NATIVE;
      } else {
          state = stream.state;
      }
  });

  return (HttpStreamState)state;
}

int32 os_get_http_stream_info(PlatformHttpStreamOp op_id,
                              _out_ int32 *status_code) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_streams[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_streams[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpStreamNative *stream = (__bridge HttpStreamNative*)handle;

  __block int code;
  dispatch_sync(stream.queue, ^{
      code = stream.statusCode;
  });

  *status_code = code;
  return 0;
}

int32 os_get_http_stream_chunk_size(PlatformHttpStreamOp op_id) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_streams[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_streams[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpStreamNative *stream = (__bridge HttpStreamNative*)handle;

  __block int total_size = 0;
  dispatch_sync(stream.queue, ^{
      for (NSData *chunk in stream.chunks) {
          total_size += (int)[chunk length];
      }
  });

  return total_size;
}

int32 os_get_http_stream_chunk(PlatformHttpStreamOp op_id, char *buffer,
                               int32 buffer_len, _out_ bool32 *is_final) {
  if (op_id < 0 || op_id >= MAX_HTTP_OPS) {
    return -1;
  }

  mutex_take(g_http_mutex);
  if (!g_http_streams[op_id].in_use) {
    mutex_drop(g_http_mutex);
    return -1;
  }

  void* handle = g_http_streams[op_id].native_handle;
  mutex_drop(g_http_mutex);

  if (!handle) {
    return -1;
  }

  HttpStreamNative *stream = (__bridge HttpStreamNative*)handle;

  __block int result = 0;
  __block BOOL final = NO;
  dispatch_sync(stream.queue, ^{
      if ([stream.chunks count] == 0) {
          result = -1;
          return;
      }

      int offset = 0;
      for (NSData *chunk in stream.chunks) {
          NSUInteger chunkLen = [chunk length];

          if (offset + chunkLen > buffer_len) {
              result = -1;
              return;
          }

          [chunk getBytes:(buffer + offset) length:chunkLen];
          offset += chunkLen;
      }

      [stream.chunks removeAllObjects];

      final = stream.streamComplete && ([stream.chunks count] == 0);

      if (final) {
          stream.state = HTTP_STREAM_COMPLETE_NATIVE;
      } else if ([stream.chunks count] == 0) {
          stream.state = HTTP_STREAM_READY_NATIVE;
      }
  });

  *is_final = final ? 1 : 0;

  if (final) {
      [stream.task cancel];
      [stream.session invalidateAndCancel];
      CFRelease(handle);

      mutex_take(g_http_mutex);
      g_http_streams[op_id].native_handle = NULL;
      g_http_streams[op_id].in_use = false;
      mutex_drop(g_http_mutex);
  }

  return result;
}
