#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

// HTTP state enums that match the C side
enum {
    HTTP_OP_NONE = 0,
    HTTP_OP_IN_PROGRESS = 1,
    HTTP_OP_COMPLETED = 2,
    HTTP_OP_ERROR = 3
};

enum {
    HTTP_STREAM_NOT_STARTED = 0,
    HTTP_STREAM_READY = 1,
    HTTP_STREAM_HAS_CHUNK = 2,
    HTTP_STREAM_COMPLETE = 3,
    HTTP_STREAM_ERROR = 4
};

@interface HttpRequest : NSObject
@property (nonatomic, assign) int statusCode;
@property (nonatomic, strong) NSData *responseData;
@property (nonatomic, strong) NSDictionary *responseHeaders;
@property (nonatomic, assign) int state;
@property (nonatomic, strong) dispatch_semaphore_t semaphore;
@end

@implementation HttpRequest
- (instancetype)init {
    self = [super init];
    if (self) {
        self.state = HTTP_OP_IN_PROGRESS;
        self.semaphore = dispatch_semaphore_create(0);
    }
    return self;
}
@end

@interface HttpStream : NSObject <NSURLSessionDataDelegate>
@property (nonatomic, assign) int statusCode;
@property (nonatomic, strong) NSMutableArray<NSData*> *chunks;
@property (nonatomic, assign) int state;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, assign) BOOL streamComplete;
@property (nonatomic, assign) BOOL hasError;
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) NSURLSessionDataTask *task;
@end

@implementation HttpStream
- (instancetype)init {
    self = [super init];
    if (self) {
        self.state = HTTP_STREAM_NOT_STARTED;
        self.chunks = [NSMutableArray array];
        self.queue = dispatch_queue_create("http_stream_queue", DISPATCH_QUEUE_SERIAL);
        
        // Create a custom session with this object as the delegate
        NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
        // Important: disable response buffering for streaming
        config.URLCache = nil;
        config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        
        // Create session with delegate callbacks on a background queue
        NSOperationQueue *delegateQueue = [[NSOperationQueue alloc] init];
        delegateQueue.maxConcurrentOperationCount = 1;
        self.session = [NSURLSession sessionWithConfiguration:config
                                                     delegate:self
                                                delegateQueue:delegateQueue];
    }
    return self;
}

// NSURLSessionDataDelegate methods for streaming
- (void)URLSession:(NSURLSession *)session 
          dataTask:(NSURLSessionDataTask *)dataTask
didReceiveResponse:(NSURLResponse *)response
 completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
    
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    
    dispatch_sync(self.queue, ^{
        self.statusCode = (int)[httpResponse statusCode];
        self.state = HTTP_STREAM_READY;
        NSLog(@"Stream started, status code: %d", self.statusCode);
    });
    
    // Continue loading data
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
          dataTask:(NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {
    
    dispatch_sync(self.queue, ^{
        // Add data chunk immediately as it arrives
        [self.chunks addObject:[data copy]];
        self.state = HTTP_STREAM_HAS_CHUNK;
        NSLog(@"Stream chunk received: %lu bytes", (unsigned long)[data length]);
    });
}

- (void)URLSession:(NSURLSession *)session
              task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error {
    
    dispatch_sync(self.queue, ^{
        if (error) {
            NSLog(@"Stream error: %@", error);
            self.hasError = YES;
            self.state = HTTP_STREAM_ERROR;
        } else {
            NSLog(@"Stream completed successfully");
            self.streamComplete = YES;
            if ([self.chunks count] == 0) {
                self.state = HTTP_STREAM_COMPLETE;
            }
            // Keep state as HAS_CHUNK if there are unread chunks
        }
    });
}

- (void)dealloc {
    [self.session invalidateAndCancel];
    [super dealloc];
}
@end

static NSString* methodToString(int method) {
    switch(method) {
        case 0: return @"GET";
        case 1: return @"POST";
        case 2: return @"PUT";
        case 3: return @"DELETE";
        default: return @"GET";
    }
}

static void parseHeaders(const char *headers, int headers_len, NSMutableURLRequest *request) {
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

// Regular HTTP request implementation (non-streaming)
void* http_darwin_request_create(int method, const char *url, int url_len,
                                 const char *headers, int headers_len,
                                 const char *body, int body_len) {
    HttpRequest *request = [[HttpRequest alloc] init];
    
    NSString *urlStr = [[NSString alloc] initWithBytes:url length:url_len encoding:NSUTF8StringEncoding];
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool {
            NSMutableURLRequest *urlRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlStr]];
            [urlRequest setHTTPMethod:methodToString(method)];
            
            parseHeaders(headers, headers_len, urlRequest);
            
            if (body && body_len > 0) {
                [urlRequest setHTTPBody:[NSData dataWithBytes:body length:body_len]];
            }
            
            NSURLSession *session = [NSURLSession sharedSession];
            NSURLSessionDataTask *task = [session dataTaskWithRequest:urlRequest 
                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                    if (error) {
                        request.state = HTTP_OP_ERROR;
                    } else {
                        request.state = HTTP_OP_COMPLETED;
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
    
    return (void*)CFBridgingRetain(request);
}

int http_darwin_request_check(void* requestPtr) {
    HttpRequest *request = (__bridge HttpRequest*)requestPtr;
    if (!request) return HTTP_OP_ERROR;
    
    if (request.state == HTTP_OP_IN_PROGRESS) {
        if (dispatch_semaphore_wait(request.semaphore, DISPATCH_TIME_NOW) == 0) {
            dispatch_semaphore_signal(request.semaphore);
        }
    }
    
    return request.state;
}

int http_darwin_request_get_info(void* requestPtr, int *status_code,
                                 int *headers_len, int *body_len) {
    HttpRequest *request = (__bridge HttpRequest*)requestPtr;
    if (!request || (request.state != HTTP_OP_COMPLETED && request.state != HTTP_OP_ERROR)) {
        return -1;
    }
    
    *status_code = request.statusCode;
    *headers_len = 0; // Not implemented yet
    *body_len = request.responseData ? (int)[request.responseData length] : 0;
    
    return 0;
}

int http_darwin_request_get_body(void* requestPtr, char *buffer, int buffer_len) {
    HttpRequest *request = (__bridge HttpRequest*)requestPtr;
    if (!request || !request.responseData) return -1;
    
    NSUInteger dataLen = [request.responseData length];
    if (dataLen > buffer_len) return -1;
    
    [request.responseData getBytes:buffer length:dataLen];
    return 0;
}

void http_darwin_request_free(void* requestPtr) {
    if (requestPtr) {
        CFRelease(requestPtr);
    }
}

// Streaming HTTP implementation
void* http_darwin_stream_create(int method, const char *url, int url_len,
                                const char *headers, int headers_len,
                                const char *body, int body_len) {
    HttpStream *stream = [[HttpStream alloc] init];
    
    NSString *urlStr = [[NSString alloc] initWithBytes:url length:url_len encoding:NSUTF8StringEncoding];
    NSLog(@"Starting stream request to: %@", urlStr);
    
    // Create the request
    NSMutableURLRequest *urlRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlStr]];
    [urlRequest setHTTPMethod:methodToString(method)];
    
    parseHeaders(headers, headers_len, urlRequest);
    
    if (body && body_len > 0) {
        [urlRequest setHTTPBody:[NSData dataWithBytes:body length:body_len]];
    }
    
    // Start the data task with the stream's session (which has the delegate set)
    stream.task = [stream.session dataTaskWithRequest:urlRequest];
    [stream.task resume];
    
    return (void*)CFBridgingRetain(stream);
}

int http_darwin_stream_check(void* streamPtr) {
    HttpStream *stream = (__bridge HttpStream*)streamPtr;
    if (!stream) return HTTP_STREAM_ERROR;
    
    __block int state;
    dispatch_sync(stream.queue, ^{
        if (stream.hasError) {
            state = HTTP_STREAM_ERROR;
        } else if ([stream.chunks count] > 0) {
            state = HTTP_STREAM_HAS_CHUNK;
        } else if (stream.streamComplete) {
            state = HTTP_STREAM_COMPLETE;
        } else {
            state = stream.state;
        }
    });
    
    return state;
}

int http_darwin_stream_get_info(void* streamPtr, int *status_code) {
    HttpStream *stream = (__bridge HttpStream*)streamPtr;
    if (!stream) return -1;
    
    __block int code;
    dispatch_sync(stream.queue, ^{
        code = stream.statusCode;
    });
    
    *status_code = code;
    return 0;
}

int http_darwin_stream_get_chunk_size(void* streamPtr) {
    HttpStream *stream = (__bridge HttpStream*)streamPtr;
    if (!stream) return 0;
    
    __block int total_size = 0;
    dispatch_sync(stream.queue, ^{
        for (NSData *chunk in stream.chunks) {
            total_size += (int)[chunk length];
        }
    });
    
    return total_size;
}

int http_darwin_stream_get_chunk(void* streamPtr, char *buffer, int buffer_len, int *is_final) {
    HttpStream *stream = (__bridge HttpStream*)streamPtr;
    if (!stream) return -1;
    
    __block int result = 0;
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
        
        *is_final = stream.streamComplete && ([stream.chunks count] == 0) ? 1 : 0;
        
        if (*is_final) {
            stream.state = HTTP_STREAM_COMPLETE;
        } else if ([stream.chunks count] == 0) {
            // No more chunks available right now, but stream is not complete
            // Set state to READY to indicate we're waiting for more data
            stream.state = HTTP_STREAM_READY;
        }
    });
    
    return result;
}

void http_darwin_stream_free(void* streamPtr) {
    if (streamPtr) {
        HttpStream *stream = (__bridge HttpStream*)streamPtr;
        [stream.task cancel];
        [stream.session invalidateAndCancel];
        CFRelease(streamPtr);
    }
}