#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <stdatomic.h>

#pragma pop_macro("global")
#pragma pop_macro("internal")

#include "os_video.h"
#include "lib/memory.h"
#include "lib/fmt.h"

#ifdef IOS
extern const char* ios_get_bundle_resource_path(const char *relative_path);
#endif

#define AUDIO_BUFFER_CAPACITY (48000 * 2 * 2 * 4)
#define DECODE_BUFFER_SIZE 4
#define PREFILL_FRAME_COUNT 2
#define PREFILL_TIMEOUT_MS 50
#define PREFILL_MAX_ATTEMPTS 20
#define MAX_FRAME_SKIP 4

typedef struct {
    u32 width;
    u32 height;
    f32 duration_seconds;
    f32 framerate;
    b32 has_audio;
    u32 audio_sample_rate;
    u32 audio_channels;
} VideoInfoNative;

typedef struct {
    CVPixelBufferRef pixelBuffer;
    CVMetalTextureRef texture;
    double presentationTime;
} VideoDecodedFrame;

@interface VideoDecoderNative : NSObject

@property(nonatomic, strong) AVAsset *asset;
@property(nonatomic, strong) AVAssetReader *reader;
@property(nonatomic, strong) AVAssetReaderTrackOutput *videoOutput;
@property(nonatomic, strong) AVAssetReader *audioReader;
@property(nonatomic, strong) AVAssetReaderTrackOutput *audioOutput;

@property(nonatomic, assign) id<MTLDevice> mtlDevice;
@property(nonatomic, assign) id<MTLCommandQueue> mtlCommandQueue;
@property(nonatomic, assign) CVMetalTextureCacheRef textureCache;
@property(nonatomic, strong) MPSImageBilinearScale *imageScaler;

- (void)setTextureCacheRef:(CVMetalTextureCacheRef)cache;

@property(nonatomic, assign) uint32_t width;
@property(nonatomic, assign) uint32_t height;
@property(nonatomic, assign) float duration;
@property(nonatomic, assign) float framerate;
@property(nonatomic, assign) double frameDuration;
@property(nonatomic, assign) double currentTime;
@property(nonatomic, assign) double timeAccumulator;

@property(nonatomic, assign) int hasAudio;
@property(nonatomic, assign) uint32_t audioSampleRate;
@property(nonatomic, assign) uint32_t audioChannels;

@property(nonatomic, assign) BOOL isPlaying;
@property(nonatomic, assign) BOOL isEOF;
@property(nonatomic, assign) BOOL loop;
@property(nonatomic, assign) BOOL needsFirstFrame;

@property(nonatomic, assign) CVMetalTextureRef currentDisplayTexture;
@property(nonatomic, assign) CVPixelBufferRef currentDisplayPixelBuffer;

@property(nonatomic, strong) dispatch_queue_t decodeQueue;
@property(nonatomic, strong) dispatch_semaphore_t framesAvailable;
@property(nonatomic, strong) dispatch_semaphore_t slotsAvailable;
@property(nonatomic, assign) BOOL decodeThreadStarted;

@property(nonatomic, assign) uint8_t *audioBuffer;
@property(nonatomic, assign) uint32_t audioBufferCapacity;
@property(nonatomic, assign) uint8_t *pendingAudioData;
@property(nonatomic, assign) uint32_t pendingAudioLength;
@property(nonatomic, assign) uint32_t pendingAudioOffset;

- (BOOL)setupReaderAtTime:(CMTime)startTime;
- (BOOL)setupAudioReaderAtTime:(CMTime)startTime;
- (void)decodeNextFrameToBuffer;
- (void)decodeAudioSamples;
- (void)startDecodeThread;
- (void)stopDecodeThread;
- (uint32_t)frameBufferCount;
- (uint32_t)frameBufferFreeSlots;
- (BOOL)frameBufferPush:(CVPixelBufferRef)pixelBuffer texture:(CVMetalTextureRef)texture pts:(double)pts;
- (BOOL)frameBufferPop:(VideoDecodedFrame *)outFrame;
- (BOOL)frameBufferPeek:(double *)outPts;
- (void)frameBufferFlush;
- (void)requestSeekToTime:(double)time;

@end

@implementation VideoDecoderNative {
    VideoDecodedFrame _frameBuffer[DECODE_BUFFER_SIZE];
    _Atomic(uint32_t) _frameReadIndex;
    _Atomic(uint32_t) _frameWriteIndex;
    _Atomic(BOOL) _stopDecodeThread;
    _Atomic(BOOL) _seekRequested;
    _Atomic(double) _seekTarget;
    _Atomic(uint32_t) _audioReadPos;
    _Atomic(uint32_t) _audioWritePos;
}

- (void)setTextureCacheRef:(CVMetalTextureCacheRef)cache {
    _textureCache = cache;
}

- (void)dealloc {
    [self stopDecodeThread];
    [self frameBufferFlush];
    [self flushAudioBuffer];

    if (self.currentDisplayTexture) {
        CFRelease(self.currentDisplayTexture);
    }
    if (self.currentDisplayPixelBuffer) {
        CVPixelBufferRelease(self.currentDisplayPixelBuffer);
    }
    if (self.reader) {
        [self.reader cancelReading];
    }
    if (self.audioReader) {
        [self.audioReader cancelReading];
    }
    if (self.textureCache) {
        CVMetalTextureCacheFlush(self.textureCache, 0);
        CFRelease(self.textureCache);
    }
    if (self.audioBuffer) {
        free(self.audioBuffer);
    }
    [super dealloc];
}

- (void)releaseCurrentDisplayFrame {
    if (self.currentDisplayTexture) {
        CFRelease(self.currentDisplayTexture);
        self.currentDisplayTexture = NULL;
    }
    if (self.currentDisplayPixelBuffer) {
        CVPixelBufferRelease(self.currentDisplayPixelBuffer);
        self.currentDisplayPixelBuffer = NULL;
    }
}

- (void)setCurrentDisplayFrame:(VideoDecodedFrame *)frame {
    [self releaseCurrentDisplayFrame];
    self.currentDisplayTexture = frame->texture;
    self.currentDisplayPixelBuffer = frame->pixelBuffer;
}

- (BOOL)setupReaderAtTime:(CMTime)startTime {
    if (self.reader) {
        [self.reader cancelReading];
        self.reader = nil;
        self.videoOutput = nil;
    }

    NSError *error = nil;
    self.reader = [AVAssetReader assetReaderWithAsset:self.asset error:&error];
    if (error) {
        NSLog(@"Failed to create asset reader: %@", error);
        return NO;
    }

    AVAssetTrack *videoTrack = [[self.asset tracksWithMediaType:AVMediaTypeVideo] firstObject];
    if (!videoTrack) {
        NSLog(@"No video track found");
        return NO;
    }

    NSDictionary *outputSettings = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (id)kCVPixelBufferMetalCompatibilityKey: @YES
    };

    self.videoOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:videoTrack
                                                                  outputSettings:outputSettings];
    self.videoOutput.alwaysCopiesSampleData = NO;

    if ([self.reader canAddOutput:self.videoOutput]) {
        [self.reader addOutput:self.videoOutput];
    } else {
        NSLog(@"Cannot add video output to reader");
        return NO;
    }

    CMTimeRange timeRange = CMTimeRangeMake(startTime, CMTimeSubtract(self.asset.duration, startTime));
    self.reader.timeRange = timeRange;

    if (![self.reader startReading]) {
        NSLog(@"Failed to start reading: %@", self.reader.error);
        return NO;
    }

    return YES;
}

- (BOOL)setupAudioReaderAtTime:(CMTime)startTime {
    if (self.audioReader) {
        [self.audioReader cancelReading];
        self.audioReader = nil;
        self.audioOutput = nil;
    }

    NSArray *audioTracks = [self.asset tracksWithMediaType:AVMediaTypeAudio];
    if (audioTracks.count == 0) {
        return NO;
    }

    NSError *error = nil;
    self.audioReader = [AVAssetReader assetReaderWithAsset:self.asset error:&error];
    if (error) {
        NSLog(@"Failed to create audio reader: %@", error);
        return NO;
    }

    AVAssetTrack *audioTrack = [audioTracks firstObject];

    NSDictionary *outputSettings = @{
        AVFormatIDKey: @(kAudioFormatLinearPCM),
        AVLinearPCMBitDepthKey: @16,
        AVLinearPCMIsFloatKey: @NO,
        AVLinearPCMIsBigEndianKey: @NO,
        AVLinearPCMIsNonInterleaved: @NO
    };

    self.audioOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:audioTrack
                                                                  outputSettings:outputSettings];
    self.audioOutput.alwaysCopiesSampleData = NO;

    if ([self.audioReader canAddOutput:self.audioOutput]) {
        [self.audioReader addOutput:self.audioOutput];
    } else {
        NSLog(@"Cannot add audio output to reader");
        return NO;
    }

    CMTimeRange timeRange = CMTimeRangeMake(startTime, CMTimeSubtract(self.asset.duration, startTime));
    self.audioReader.timeRange = timeRange;

    if (![self.audioReader startReading]) {
        NSLog(@"Failed to start audio reading: %@", self.audioReader.error);
        return NO;
    }

    return YES;
}

- (uint32_t)frameBufferCount {
    uint32_t w = atomic_load(&_frameWriteIndex);
    uint32_t r = atomic_load(&_frameReadIndex);
    return (w - r + DECODE_BUFFER_SIZE) % DECODE_BUFFER_SIZE;
}

- (uint32_t)frameBufferFreeSlots {
    return DECODE_BUFFER_SIZE - 1 - [self frameBufferCount];
}

- (BOOL)frameBufferPush:(CVPixelBufferRef)pixelBuffer texture:(CVMetalTextureRef)texture pts:(double)pts {
    uint32_t w = atomic_load(&_frameWriteIndex);
    uint32_t nextW = (w + 1) % DECODE_BUFFER_SIZE;
    if (nextW == atomic_load(&_frameReadIndex)) {
        return NO;
    }
    _frameBuffer[w].pixelBuffer = pixelBuffer;
    _frameBuffer[w].texture = texture;
    _frameBuffer[w].presentationTime = pts;
    atomic_thread_fence(memory_order_release);
    atomic_store(&_frameWriteIndex, nextW);
    return YES;
}

- (BOOL)frameBufferPop:(VideoDecodedFrame *)outFrame {
    uint32_t r = atomic_load(&_frameReadIndex);
    atomic_thread_fence(memory_order_acquire);
    if (r == atomic_load(&_frameWriteIndex)) {
        return NO;
    }
    outFrame->pixelBuffer = _frameBuffer[r].pixelBuffer;
    outFrame->texture = _frameBuffer[r].texture;
    outFrame->presentationTime = _frameBuffer[r].presentationTime;
    _frameBuffer[r].pixelBuffer = NULL;
    _frameBuffer[r].texture = NULL;
    atomic_thread_fence(memory_order_release);
    atomic_store(&_frameReadIndex, (r + 1) % DECODE_BUFFER_SIZE);
    return YES;
}

- (BOOL)frameBufferPeek:(double *)outPts {
    uint32_t r = atomic_load(&_frameReadIndex);
    atomic_thread_fence(memory_order_acquire);
    if (r == atomic_load(&_frameWriteIndex)) {
        return NO;
    }
    *outPts = _frameBuffer[r].presentationTime;
    return YES;
}

- (void)frameBufferFlush {
    uint32_t r = atomic_load(&_frameReadIndex);
    uint32_t w = atomic_load(&_frameWriteIndex);
    while (r != w) {
        if (_frameBuffer[r].texture) {
            CFRelease(_frameBuffer[r].texture);
            _frameBuffer[r].texture = NULL;
        }
        if (_frameBuffer[r].pixelBuffer) {
            CVPixelBufferRelease(_frameBuffer[r].pixelBuffer);
            _frameBuffer[r].pixelBuffer = NULL;
        }
        r = (r + 1) % DECODE_BUFFER_SIZE;
    }
    atomic_store(&_frameReadIndex, 0);
    atomic_store(&_frameWriteIndex, 0);
}

- (void)decodeNextFrameToBuffer {
    if (self.reader.status != AVAssetReaderStatusReading) {
        if (self.reader.status == AVAssetReaderStatusCompleted) {
            self.isEOF = YES;
        }
        return;
    }

    CMSampleBufferRef sampleBuffer = [self.videoOutput copyNextSampleBuffer];
    if (!sampleBuffer) {
        if (self.reader.status == AVAssetReaderStatusCompleted) {
            self.isEOF = YES;
        }
        return;
    }

    CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    double presentationTime = CMTimeGetSeconds(pts);

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) {
        CFRelease(sampleBuffer);
        return;
    }

    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);

    CVMetalTextureRef metalTexture = NULL;
    CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        self.textureCache,
        imageBuffer,
        NULL,
        MTLPixelFormatBGRA8Unorm,
        width,
        height,
        0,
        &metalTexture
    );

    if (result == kCVReturnSuccess && metalTexture) {
        CVPixelBufferRetain(imageBuffer);
        if ([self frameBufferPush:imageBuffer texture:metalTexture pts:presentationTime]) {
            dispatch_semaphore_signal(self.framesAvailable);
        } else {
            CFRelease(metalTexture);
            CVPixelBufferRelease(imageBuffer);
        }
    }

    CFRelease(sampleBuffer);
}

- (void)decodeThreadLoop {
    BOOL videoEOF = NO;
    BOOL audioEOF = NO;

    while (!atomic_load(&_stopDecodeThread)) {
        @autoreleasepool {
            if (atomic_load(&_seekRequested)) {
                double seekTime = atomic_load(&_seekTarget);
                [self frameBufferFlush];
                if (self.hasAudio) {
                    [self flushAudioBuffer];
                }

                CMTime cmSeekTime = CMTimeMakeWithSeconds(seekTime, 600);
                [self setupReaderAtTime:cmSeekTime];
                if (self.hasAudio) {
                    [self setupAudioReaderAtTime:cmSeekTime];
                }

                videoEOF = NO;
                audioEOF = NO;
                self.isEOF = NO;
                atomic_store(&_seekRequested, NO);
                dispatch_semaphore_signal(self.slotsAvailable);
            }

            BOOL videoBufferFull = [self frameBufferFreeSlots] == 0;
            BOOL audioBufferFull = !self.hasAudio || [self audioBufferFreeSpace] == 0;

            if (videoBufferFull && audioBufferFull) {
                dispatch_semaphore_wait(self.slotsAvailable, dispatch_time(DISPATCH_TIME_NOW, 16 * NSEC_PER_MSEC));
                continue;
            }

            if (!videoBufferFull && !videoEOF) {
                [self decodeNextFrameToBuffer];
                if (self.isEOF) {
                    videoEOF = YES;
                    self.isEOF = NO;
                }
            }

            if (!audioBufferFull && !audioEOF && self.hasAudio) {
                [self decodeAudioSamples];
                if (self.audioReader.status == AVAssetReaderStatusCompleted) {
                    audioEOF = YES;
                }
            }

            if (videoEOF && (audioEOF || !self.hasAudio)) {
                if (self.loop) {
                    CMTime seekTime = kCMTimeZero;
                    [self setupReaderAtTime:seekTime];
                    if (self.hasAudio) {
                        [self setupAudioReaderAtTime:seekTime];
                    }
                    videoEOF = NO;
                    audioEOF = NO;
                } else {
                    self.isEOF = YES;
                    break;
                }
            }
        }
    }
}

- (void)startDecodeThread {
    if (self.decodeThreadStarted) return;

    atomic_store(&_stopDecodeThread, NO);
    self.framesAvailable = dispatch_semaphore_create(0);
    self.slotsAvailable = dispatch_semaphore_create(1);
    self.decodeQueue = dispatch_queue_create("com.hz.videodecode", DISPATCH_QUEUE_SERIAL);

    self.decodeThreadStarted = YES;

    dispatch_async(self.decodeQueue, ^{
        [self decodeThreadLoop];
    });
}

- (void)stopDecodeThread {
    if (!self.decodeThreadStarted) return;

    atomic_store(&_stopDecodeThread, YES);
    dispatch_semaphore_signal(self.slotsAvailable);

    dispatch_sync(self.decodeQueue, ^{});

    self.decodeQueue = nil;
    self.framesAvailable = nil;
    self.slotsAvailable = nil;
    self.decodeThreadStarted = NO;
}

- (void)requestSeekToTime:(double)time {
    atomic_store(&_seekTarget, time);
    atomic_store(&_seekRequested, YES);
    dispatch_semaphore_signal(self.slotsAvailable);
}

- (void)writePendingAudioToBuffer {
    if (self.pendingAudioLength == 0) return;

    uint32_t freeSpace = [self audioBufferFreeSpace];
    if (freeSpace == 0) return;

    uint32_t remaining = self.pendingAudioLength - self.pendingAudioOffset;
    uint32_t toWrite = (remaining > freeSpace) ? freeSpace : remaining;

    uint32_t w = atomic_load(&_audioWritePos);
    uint32_t firstChunk = self.audioBufferCapacity - w;
    if (firstChunk > toWrite) firstChunk = toWrite;

    memcpy(self.audioBuffer + w, self.pendingAudioData + self.pendingAudioOffset, firstChunk);
    if (toWrite > firstChunk) {
        memcpy(self.audioBuffer, self.pendingAudioData + self.pendingAudioOffset + firstChunk, toWrite - firstChunk);
    }

    atomic_thread_fence(memory_order_release);
    atomic_store(&_audioWritePos, (w + toWrite) % self.audioBufferCapacity);

    self.pendingAudioOffset += toWrite;
    if (self.pendingAudioOffset >= self.pendingAudioLength) {
        free(self.pendingAudioData);
        self.pendingAudioData = NULL;
        self.pendingAudioLength = 0;
        self.pendingAudioOffset = 0;
    }
}

- (void)decodeAudioSamples {
    if (!self.hasAudio || !self.audioOutput) return;

    [self writePendingAudioToBuffer];

    if (self.pendingAudioLength > 0) return;
    if (self.audioReader.status != AVAssetReaderStatusReading) return;

    uint32_t freeSpace = [self audioBufferFreeSpace];
    if (freeSpace == 0) return;

    CMSampleBufferRef sampleBuffer = [self.audioOutput copyNextSampleBuffer];
    if (!sampleBuffer) return;

    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer) {
        CFRelease(sampleBuffer);
        return;
    }

    size_t totalLength = CMBlockBufferGetDataLength(blockBuffer);
    if (totalLength == 0) {
        CFRelease(sampleBuffer);
        return;
    }

    uint8_t *tempBuffer = malloc(totalLength);
    if (!tempBuffer) {
        CFRelease(sampleBuffer);
        return;
    }

    OSStatus status = CMBlockBufferCopyDataBytes(blockBuffer, 0, totalLength, tempBuffer);
    CFRelease(sampleBuffer);

    if (status != kCMBlockBufferNoErr) {
        free(tempBuffer);
        return;
    }

    uint32_t toWrite = ((uint32_t)totalLength > freeSpace) ? freeSpace : (uint32_t)totalLength;

    uint32_t w = atomic_load(&_audioWritePos);
    uint32_t firstChunk = self.audioBufferCapacity - w;
    if (firstChunk > toWrite) firstChunk = toWrite;

    memcpy(self.audioBuffer + w, tempBuffer, firstChunk);
    if (toWrite > firstChunk) {
        memcpy(self.audioBuffer, tempBuffer + firstChunk, toWrite - firstChunk);
    }

    atomic_thread_fence(memory_order_release);
    atomic_store(&_audioWritePos, (w + toWrite) % self.audioBufferCapacity);

    if (toWrite < totalLength) {
        self.pendingAudioData = tempBuffer;
        self.pendingAudioLength = (uint32_t)totalLength;
        self.pendingAudioOffset = toWrite;
    } else {
        free(tempBuffer);
    }
}

- (uint32_t)audioBufferAvailable {
    int64_t w = atomic_load(&_audioWritePos);
    int64_t r = atomic_load(&_audioReadPos);
    atomic_thread_fence(memory_order_acquire);
    int64_t capacity = self.audioBufferCapacity;
    return (uint32_t)((w - r + capacity) % capacity);
}

- (uint32_t)audioBufferFreeSpace {
    return self.audioBufferCapacity - 1 - [self audioBufferAvailable];
}

- (void)writeAudioData:(uint8_t *)data length:(uint32_t)length {
    uint32_t freeSpace = [self audioBufferFreeSpace];
    if (length > freeSpace) {
        length = freeSpace;
    }
    if (length == 0) return;

    uint32_t w = atomic_load(&_audioWritePos);
    uint32_t firstChunk = self.audioBufferCapacity - w;
    if (firstChunk > length) firstChunk = length;

    memcpy(self.audioBuffer + w, data, firstChunk);
    if (length > firstChunk) {
        memcpy(self.audioBuffer, data + firstChunk, length - firstChunk);
    }

    atomic_thread_fence(memory_order_release);
    atomic_store(&_audioWritePos, (w + length) % self.audioBufferCapacity);
}

- (uint32_t)readAudioData:(uint8_t *)data maxLength:(uint32_t)maxLength {
    uint32_t available = [self audioBufferAvailable];
    if (maxLength > available) {
        maxLength = available;
    }
    if (maxLength == 0) return 0;

    uint32_t r = atomic_load(&_audioReadPos);
    uint32_t firstChunk = self.audioBufferCapacity - r;
    if (firstChunk > maxLength) firstChunk = maxLength;

    memcpy(data, self.audioBuffer + r, firstChunk);
    if (maxLength > firstChunk) {
        memcpy(data + firstChunk, self.audioBuffer, maxLength - firstChunk);
    }

    atomic_thread_fence(memory_order_release);
    atomic_store(&_audioReadPos, (r + maxLength) % self.audioBufferCapacity);
    return maxLength;
}

- (void)flushAudioBuffer {
    atomic_store(&_audioWritePos, 0);
    atomic_store(&_audioReadPos, 0);
    if (self.pendingAudioData) {
        free(self.pendingAudioData);
        self.pendingAudioData = NULL;
        self.pendingAudioLength = 0;
        self.pendingAudioOffset = 0;
    }
}

@end

internal void *video_darwin_create_from_file(const char *file_path, void *mtl_device) {
    @autoreleasepool {
        if (!file_path || !mtl_device) {
            return NULL;
        }

        NSString *pathStr = [NSString stringWithUTF8String:file_path];
        NSURL *fileURL = [NSURL fileURLWithPath:pathStr];

        if (![[NSFileManager defaultManager] fileExistsAtPath:pathStr]) {
            NSLog(@"Video file not found: %@", pathStr);
            return NULL;
        }

        VideoDecoderNative *decoder = [[VideoDecoderNative alloc] init];
        decoder.mtlDevice = (__bridge id<MTLDevice>)mtl_device;
        decoder.mtlCommandQueue = [decoder.mtlDevice newCommandQueue];

        CVMetalTextureCacheRef textureCacheRef = NULL;
        CVReturn result = CVMetalTextureCacheCreate(
            kCFAllocatorDefault,
            NULL,
            decoder.mtlDevice,
            NULL,
            &textureCacheRef
        );

        if (result == kCVReturnSuccess && textureCacheRef) {
            [decoder setTextureCacheRef:textureCacheRef];
        }

        if (result != kCVReturnSuccess) {
            NSLog(@"Failed to create Metal texture cache: %d", result);
            [decoder release];
            return NULL;
        }

        decoder.asset = [AVAsset assetWithURL:fileURL];

        AVAssetTrack *videoTrack = [[decoder.asset tracksWithMediaType:AVMediaTypeVideo] firstObject];
        if (!videoTrack) {
            NSLog(@"No video track in asset");
            [decoder release];
            return NULL;
        }

        CGSize naturalSize = videoTrack.naturalSize;
        decoder.width = (uint32_t)naturalSize.width;
        decoder.height = (uint32_t)naturalSize.height;
        decoder.duration = (float)CMTimeGetSeconds(decoder.asset.duration);
        decoder.framerate = videoTrack.nominalFrameRate;

        if (decoder.framerate > 0) {
            decoder.frameDuration = 1.0 / decoder.framerate;
        } else {
            decoder.frameDuration = 1.0 / 30.0;
        }

        NSArray *audioTracks = [decoder.asset tracksWithMediaType:AVMediaTypeAudio];
        if (audioTracks.count > 0) {
            decoder.hasAudio = YES;
            AVAssetTrack *audioTrack = [audioTracks firstObject];
            CMFormatDescriptionRef formatDesc = (__bridge CMFormatDescriptionRef)[audioTrack.formatDescriptions firstObject];
            if (formatDesc) {
                const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(formatDesc);
                if (asbd) {
                    decoder.audioSampleRate = (uint32_t)asbd->mSampleRate;
                    decoder.audioChannels = (uint32_t)asbd->mChannelsPerFrame;
                }
            }
            if (decoder.audioSampleRate == 0) decoder.audioSampleRate = 48000;
            if (decoder.audioChannels == 0) decoder.audioChannels = 2;

            decoder.audioBufferCapacity = AUDIO_BUFFER_CAPACITY;
            decoder.audioBuffer = calloc(1, AUDIO_BUFFER_CAPACITY);
            [decoder flushAudioBuffer];
        }

        if (![decoder setupReaderAtTime:kCMTimeZero]) {
            [decoder release];
            return NULL;
        }

        if (decoder.hasAudio) {
            [decoder setupAudioReaderAtTime:kCMTimeZero];
        }

        decoder.needsFirstFrame = YES;

        return (void *)CFBridgingRetain(decoder);
    }
}

internal void video_darwin_destroy(void *decoder_ptr) {
    if (decoder_ptr) {
        CFRelease(decoder_ptr);
    }
}

internal void video_darwin_get_info(void *decoder_ptr, VideoInfoNative *info) {
    if (!decoder_ptr || !info) return;

    VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
    info->width = decoder.width;
    info->height = decoder.height;
    info->duration_seconds = decoder.duration;
    info->framerate = decoder.framerate;
    info->has_audio = decoder.hasAudio;
    info->audio_sample_rate = decoder.audioSampleRate;
    info->audio_channels = decoder.audioChannels;
}

internal b32 video_darwin_update(void *decoder_ptr, f64 dt, b32 is_playing) {
    @autoreleasepool {
        if (!decoder_ptr) return 0;

        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;

        if (!is_playing) return 0;

        decoder.timeAccumulator += dt;

        int needFrame = decoder.needsFirstFrame || (decoder.timeAccumulator >= decoder.frameDuration);
        if (!needFrame) {
            return 0;
        }

        VideoDecodedFrame frame = {0};
        if (![decoder frameBufferPop:&frame]) {
            return 0;
        }

        dispatch_semaphore_signal(decoder.slotsAvailable);

        double targetTime = decoder.currentTime + decoder.timeAccumulator;
        uint32_t skipped = 0;

        while (skipped < MAX_FRAME_SKIP) {
            double nextPts = 0;
            if (![decoder frameBufferPeek:&nextPts]) {
                break;
            }
            if (nextPts > targetTime) {
                break;
            }
            VideoDecodedFrame nextFrame = {0};
            if (![decoder frameBufferPop:&nextFrame]) {
                break;
            }
            if (frame.texture) {
                CFRelease(frame.texture);
            }
            if (frame.pixelBuffer) {
                CVPixelBufferRelease(frame.pixelBuffer);
            }
            frame = nextFrame;
            dispatch_semaphore_signal(decoder.slotsAvailable);
            skipped++;
        }

        [decoder setCurrentDisplayFrame:&frame];
        decoder.currentTime = frame.presentationTime;

        if (decoder.needsFirstFrame) {
            decoder.needsFirstFrame = NO;
            decoder.timeAccumulator = 0;
        } else {
            decoder.timeAccumulator -= decoder.frameDuration * (1 + skipped);
            if (decoder.timeAccumulator < 0) {
                decoder.timeAccumulator = 0;
            }
        }

        return 1;
    }
}

internal b32 video_darwin_blit_to_texture(void *decoder_ptr, void *dst_texture) {
    @autoreleasepool {
        if (!decoder_ptr || !dst_texture) return 0;

        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
        if (!decoder.currentDisplayTexture) return 0;

        id<MTLTexture> srcTexture = CVMetalTextureGetTexture(decoder.currentDisplayTexture);
        if (!srcTexture) return 0;

        id<MTLTexture> dstTexture = (__bridge id<MTLTexture>)dst_texture;

        id<MTLCommandBuffer> commandBuffer = [decoder.mtlCommandQueue commandBuffer];
        if (!commandBuffer) return 0;

        if (!decoder.imageScaler) {
            decoder.imageScaler = [[MPSImageBilinearScale alloc] initWithDevice:decoder.mtlDevice];
        }

        [decoder.imageScaler encodeToCommandBuffer:commandBuffer
                                     sourceTexture:srcTexture
                                destinationTexture:dstTexture];

        [commandBuffer commit];

        return 1;
    }
}

internal void *video_darwin_get_current_texture(void *decoder_ptr) {
    if (!decoder_ptr) return NULL;

    VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
    if (!decoder.currentDisplayTexture) return NULL;

    id<MTLTexture> texture = CVMetalTextureGetTexture(decoder.currentDisplayTexture);
    return (__bridge void *)texture;
}

internal void video_darwin_play(void *decoder_ptr) {
    @autoreleasepool {
        if (!decoder_ptr) return;
        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;

        if (!decoder.isPlaying) {
            decoder.needsFirstFrame = YES;
            decoder.timeAccumulator = 0;
            [decoder startDecodeThread];

            uint32_t attempts = 0;
            while ([decoder frameBufferCount] < PREFILL_FRAME_COUNT && attempts < PREFILL_MAX_ATTEMPTS) {
                dispatch_semaphore_wait(decoder.framesAvailable, dispatch_time(DISPATCH_TIME_NOW, PREFILL_TIMEOUT_MS * NSEC_PER_MSEC));
                if (decoder.isEOF) {
                    break;
                }
                attempts++;
            }
        }
        decoder.isPlaying = YES;
    }
}

internal void video_darwin_pause(void *decoder_ptr) {
    @autoreleasepool {
        if (!decoder_ptr) return;
        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
        [decoder stopDecodeThread];
        decoder.isPlaying = NO;
    }
}

internal void video_darwin_seek(void *decoder_ptr, f64 time_seconds) {
    @autoreleasepool {
        if (!decoder_ptr) return;

        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;

        if (time_seconds < 0) time_seconds = 0;
        if (time_seconds >= decoder.duration) {
            decoder.isEOF = YES;
            return;
        }

        [decoder releaseCurrentDisplayFrame];

        decoder.currentTime = time_seconds;
        decoder.timeAccumulator = 0;
        decoder.needsFirstFrame = YES;
        decoder.isEOF = NO;

        if (decoder.decodeThreadStarted) {
            [decoder requestSeekToTime:time_seconds];
        } else {
            CMTime seekTime = CMTimeMakeWithSeconds(time_seconds, 600);
            [decoder frameBufferFlush];
            [decoder setupReaderAtTime:seekTime];
            if (decoder.hasAudio) {
                [decoder setupAudioReaderAtTime:seekTime];
                [decoder flushAudioBuffer];
            }
        }
    }
}

internal f64 video_darwin_get_current_time(void *decoder_ptr) {
    if (!decoder_ptr) return 0.0;
    VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
    return decoder.currentTime;
}

internal b32 video_darwin_is_eof(void *decoder_ptr) {
    if (!decoder_ptr) return 1;
    VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
    return decoder.isEOF ? 1 : 0;
}

internal void video_darwin_set_loop(void *decoder_ptr, b32 loop) {
    if (!decoder_ptr) return;
    VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
    decoder.loop = loop ? YES : NO;
}

internal u32 video_darwin_read_audio(void *decoder_ptr, u8 *buffer, u32 max_bytes) {
    @autoreleasepool {
        if (!decoder_ptr || !buffer) return 0;

        VideoDecoderNative *decoder = (__bridge VideoDecoderNative *)decoder_ptr;
        if (!decoder.hasAudio) return 0;

        return [decoder readAudioData:buffer maxLength:max_bytes];
    }
}

struct OsVideoPlayer {
    void *native_decoder;
    OsVideoState state;
    b32 loop;

    f64 duration;
    u32 width;
    u32 height;
    u32 output_width;
    u32 output_height;

    void *output_textures[2];
    u32 write_index;

    b32 has_audio;
    u32 audio_sample_rate;
    u32 audio_channels;

    Allocator *allocator;
};

static b32 g_video_initialized = false;

b32 os_video_init(void) {
    if (g_video_initialized) {
        return true;
    }
    g_video_initialized = true;
    return true;
}

void os_video_shutdown(void) {
    g_video_initialized = false;
}

OsVideoPlayer *os_video_player_create(OsVideoPlayerDesc *desc, Allocator *allocator) {
    if (!desc || !desc->file_path) {
        LOG_ERROR("Invalid video player descriptor");
        return NULL;
    }

    if (!desc->device) {
        LOG_ERROR("Metal device is required");
        return NULL;
    }

    if (!g_video_initialized) {
        LOG_ERROR("Video system not initialized - call os_video_init first");
        return NULL;
    }

#ifdef IOS
    const char *file_path = ios_get_bundle_resource_path(desc->file_path);
#else
    const char *file_path = desc->file_path;
#endif

    void *native = video_darwin_create_from_file(file_path, desc->device);
    if (!native) {
        LOG_ERROR("Failed to create native video decoder for %", FMT_STR(desc->file_path));
        return NULL;
    }

    OsVideoPlayer *player = ALLOC(allocator, OsVideoPlayer);
    if (!player) {
        video_darwin_destroy(native);
        return NULL;
    }

    memset(player, 0, sizeof(OsVideoPlayer));
    player->native_decoder = native;
    player->allocator = allocator;
    player->state = OS_VIDEO_STATE_IDLE;
    player->loop = desc->loop;

    VideoInfoNative info = {0};
    video_darwin_get_info(native, &info);

    player->width = info.width;
    player->height = info.height;
    player->duration = (f64)info.duration_seconds;
    player->has_audio = info.has_audio;
    player->audio_sample_rate = info.audio_sample_rate;
    player->audio_channels = info.audio_channels;

    if (desc->output_width > 0 && desc->output_height > 0) {
        player->output_width = desc->output_width;
        player->output_height = desc->output_height;
    } else if (desc->output_height > 0 && player->height > 0) {
        player->output_height = desc->output_height;
        player->output_width = (player->width * desc->output_height) / player->height;
    } else if (desc->output_width > 0 && player->width > 0) {
        player->output_width = desc->output_width;
        player->output_height = (player->height * desc->output_width) / player->width;
    } else {
        player->output_width = player->width;
        player->output_height = player->height;
    }

    video_darwin_set_loop(native, desc->loop);

    LOG_INFO("Video loaded: %x% -> %x%, duration: % seconds",
        FMT_UINT(player->width),
        FMT_UINT(player->height),
        FMT_UINT(player->output_width),
        FMT_UINT(player->output_height),
        FMT_FLOAT(player->duration));

    return player;
}

void os_video_player_destroy(OsVideoPlayer *player) {
    if (!player) {
        return;
    }

    if (player->native_decoder) {
        video_darwin_destroy(player->native_decoder);
        player->native_decoder = NULL;
    }
}

void os_video_player_play(OsVideoPlayer *player) {
    if (!player || !player->native_decoder) return;
    video_darwin_play(player->native_decoder);
    player->state = OS_VIDEO_STATE_PLAYING;
}

void os_video_player_pause(OsVideoPlayer *player) {
    if (!player || !player->native_decoder) return;
    video_darwin_pause(player->native_decoder);
    player->state = OS_VIDEO_STATE_PAUSED;
}

void os_video_player_stop(OsVideoPlayer *player) {
    if (!player || !player->native_decoder) return;
    video_darwin_pause(player->native_decoder);
    video_darwin_seek(player->native_decoder, 0);
    player->state = OS_VIDEO_STATE_IDLE;
}

void os_video_player_seek(OsVideoPlayer *player, f64 time_seconds) {
    if (!player || !player->native_decoder) return;
    video_darwin_seek(player->native_decoder, time_seconds);
}

void os_video_player_set_loop(OsVideoPlayer *player, b32 loop) {
    if (!player) return;
    player->loop = loop;
    if (player->native_decoder) {
        video_darwin_set_loop(player->native_decoder, loop);
    }
}

OsVideoState os_video_player_get_state(OsVideoPlayer *player) {
    if (!player) return OS_VIDEO_STATE_ERROR;
    return player->state;
}

f64 os_video_player_get_duration(OsVideoPlayer *player) {
    if (!player) return 0.0;
    return player->duration;
}

f64 os_video_player_get_current_time(OsVideoPlayer *player) {
    if (!player || !player->native_decoder) return 0.0;
    return video_darwin_get_current_time(player->native_decoder);
}

void os_video_player_get_dimensions(OsVideoPlayer *player, u32 *width, u32 *height) {
    if (!player) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = player->width;
    if (height) *height = player->height;
}

void os_video_player_get_output_dimensions(OsVideoPlayer *player, u32 *width, u32 *height) {
    if (!player) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = player->output_width;
    if (height) *height = player->output_height;
}

void os_video_player_set_output_textures(OsVideoPlayer *player, void *texture_a, void *texture_b) {
    if (!player || !texture_a || !texture_b) return;
    player->output_textures[0] = texture_a;
    player->output_textures[1] = texture_b;
    player->write_index = 0;
}

void *os_video_player_get_display_texture(OsVideoPlayer *player) {
    if (!player) return NULL;
    u32 display_index = 1 - player->write_index;
    return player->output_textures[display_index];
}

b32 os_video_player_update(OsVideoPlayer *player, f64 delta_time) {
    if (!player || !player->native_decoder) return false;
    if (player->state != OS_VIDEO_STATE_PLAYING) return false;
    if (!player->output_textures[0] || !player->output_textures[1]) return false;

    b32 frame_ready = video_darwin_update(player->native_decoder, delta_time, true);

    if (video_darwin_is_eof(player->native_decoder)) {
        if (player->loop) {
            video_darwin_seek(player->native_decoder, 0);
        } else {
            player->state = OS_VIDEO_STATE_ENDED;
            return false;
        }
    }

    if (frame_ready) {
        void *dst_texture = player->output_textures[player->write_index];
        if (video_darwin_blit_to_texture(player->native_decoder, dst_texture)) {
            player->write_index = 1 - player->write_index;
            return true;
        }
    }

    return false;
}

b32 os_video_player_has_audio(OsVideoPlayer *player) {
    if (!player) return false;
    return player->has_audio;
}

void os_video_player_get_audio_format(OsVideoPlayer *player, u32 *sample_rate, u32 *channels) {
    if (!player) {
        if (sample_rate) *sample_rate = 0;
        if (channels) *channels = 0;
        return;
    }
    if (sample_rate) *sample_rate = player->audio_sample_rate;
    if (channels) *channels = player->audio_channels;
}

u32 os_video_player_read_audio(OsVideoPlayer *player, u8 *buffer, u32 max_bytes) {
    if (!player || !buffer || !player->native_decoder || !player->has_audio) return 0;
    return video_darwin_read_audio(player->native_decoder, buffer, max_bytes);
}
