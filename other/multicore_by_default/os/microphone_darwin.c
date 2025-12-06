#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

#pragma pop_macro("global")
#pragma pop_macro("internal")

#define MIC_BUFFER_SIZE (48000 * 4)

@interface MicrophoneCaptureNative : NSObject
@property (nonatomic, strong) AVAudioEngine *audioEngine;
@property (nonatomic, strong) AVAudioInputNode *inputNode;
@property (nonatomic, assign) BOOL isRecording;
@property (nonatomic, assign) int16_t *ringBuffer;
@property (nonatomic, assign) uint32_t bufferSize;
@property (nonatomic, assign) uint32_t writePos;
@property (nonatomic, assign) uint32_t readPos;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, assign) uint32_t sampleRate;
@end

@implementation MicrophoneCaptureNative

- (instancetype)init {
    self = [super init];
    if (self) {
        self.audioEngine = nil;
        self.inputNode = nil;
        self.isRecording = NO;
        self.bufferSize = MIC_BUFFER_SIZE;
        self.ringBuffer = (int16_t*)malloc(sizeof(int16_t) * self.bufferSize);
        self.writePos = 0;
        self.readPos = 0;
        self.queue = dispatch_queue_create("microphone_queue", DISPATCH_QUEUE_SERIAL);
        self.sampleRate = 48000;
    }
    return self;
}

- (void)dealloc {
    if (self.isRecording) {
        [self stopRecording];
    }
    if (self.ringBuffer) {
        free(self.ringBuffer);
        self.ringBuffer = NULL;
    }
    [super dealloc];
}

- (BOOL)requestPermission {
#if TARGET_OS_IOS
    __block BOOL granted = NO;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    [audioSession requestRecordPermission:^(BOOL allowed) {
        granted = allowed;
        dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    return granted;
#else
    if (@available(macOS 10.14, *)) {
        __block BOOL granted = NO;
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);

        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL allowed) {
            granted = allowed;
            dispatch_semaphore_signal(sema);
        }];

        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        return granted;
    }
    return YES;
#endif
}

- (BOOL)startRecording {
    if (self.isRecording) {
        return YES;
    }

    if (![self requestPermission]) {
        return NO;
    }

#if TARGET_OS_IOS
    NSError *error = nil;
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord
                  withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothA2DP
                        error:&error];
    if (error) {
        return NO;
    }

    [audioSession setActive:YES error:&error];
    if (error) {
        return NO;
    }
#endif

    if (!self.audioEngine) {
        self.audioEngine = [[AVAudioEngine alloc] init];
        self.inputNode = [self.audioEngine inputNode];
        AVAudioFormat *initFormat = [self.inputNode outputFormatForBus:0];
        self.sampleRate = (uint32_t)initFormat.sampleRate;
    }

    AVAudioFormat *inputFormat = [self.inputNode outputFormatForBus:0];

    AVAudioFormat *recordingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                                      sampleRate:inputFormat.sampleRate
                                                                        channels:1
                                                                     interleaved:YES];

    AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:inputFormat toFormat:recordingFormat];
    if (!converter) {
        return NO;
    }

    __unsafe_unretained MicrophoneCaptureNative *weakSelf = self;
    [self.inputNode installTapOnBus:0
                         bufferSize:1024
                             format:inputFormat
                              block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
        (void)when;
        MicrophoneCaptureNative *strongSelf = weakSelf;
        if (!strongSelf) return;

        AVAudioPCMBuffer *convertedBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:recordingFormat
                                                                          frameCapacity:buffer.frameLength];

        NSError *conversionError = nil;
        AVAudioConverterInputBlock inputBlock = ^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
            (void)inNumberOfPackets;
            *outStatus = AVAudioConverterInputStatus_HaveData;
            return buffer;
        };

        [converter convertToBuffer:convertedBuffer error:&conversionError withInputFromBlock:inputBlock];

        if (conversionError) {
            return;
        }

        int16_t *samples = (int16_t *)convertedBuffer.int16ChannelData[0];
        uint32_t sampleCount = convertedBuffer.frameLength;

        dispatch_sync(strongSelf.queue, ^{
            for (uint32_t i = 0; i < sampleCount; i++) {
                strongSelf.ringBuffer[strongSelf.writePos] = samples[i];
                strongSelf.writePos = (strongSelf.writePos + 1) % strongSelf.bufferSize;

                if (strongSelf.writePos == strongSelf.readPos) {
                    strongSelf.readPos = (strongSelf.readPos + 1) % strongSelf.bufferSize;
                }
            }
        });
    }];

    NSError *engineError = nil;
    [self.audioEngine startAndReturnError:&engineError];

    if (engineError) {
        [self.inputNode removeTapOnBus:0];
        return NO;
    }

    self.isRecording = YES;
    return YES;
}

- (void)stopRecording {
    if (!self.isRecording) {
        return;
    }

    [self.inputNode removeTapOnBus:0];
    [self.audioEngine stop];

    self.isRecording = NO;
}

- (uint32_t)getAvailableSamples {
    __block uint32_t available = 0;
    dispatch_sync(self.queue, ^{
        if (self.writePos >= self.readPos) {
            available = self.writePos - self.readPos;
        } else {
            available = self.bufferSize - self.readPos + self.writePos;
        }
    });
    return available;
}

- (uint32_t)readSamples:(int16_t *)buffer maxSamples:(uint32_t)maxSamples {
    __block uint32_t samplesRead = 0;
    dispatch_sync(self.queue, ^{
        uint32_t available = 0;
        if (self.writePos >= self.readPos) {
            available = self.writePos - self.readPos;
        } else {
            available = self.bufferSize - self.readPos + self.writePos;
        }

        uint32_t toRead = maxSamples < available ? maxSamples : available;

        for (uint32_t i = 0; i < toRead; i++) {
            buffer[i] = self.ringBuffer[self.readPos];
            self.readPos = (self.readPos + 1) % self.bufferSize;
            samplesRead++;
        }
    });
    return samplesRead;
}

@end

static MicrophoneCaptureNative *g_micCapture = nil;

static void* microphone_darwin_init(void) {
    if (g_micCapture) {
        return (void*)CFBridgingRetain(g_micCapture);
    }

    g_micCapture = [[MicrophoneCaptureNative alloc] init];
    return (void*)CFBridgingRetain(g_micCapture);
}

static int microphone_darwin_start_recording(void *micPtr) {
    MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
    if (!mic) return 0;

    return [mic startRecording] ? 1 : 0;
}

static void microphone_darwin_stop_recording(void *micPtr) {
    MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
    if (!mic) return;

    [mic stopRecording];
}

static uint32_t microphone_darwin_get_available_samples(void *micPtr) {
    MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
    if (!mic) return 0;

    return [mic getAvailableSamples];
}

static uint32_t microphone_darwin_read_samples(void *micPtr, int16_t *buffer, uint32_t maxSamples) {
    MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
    if (!mic) return 0;

    return [mic readSamples:buffer maxSamples:maxSamples];
}

static uint32_t microphone_darwin_get_sample_rate(void *micPtr) {
    MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
    if (!mic) return 48000;

    return mic.sampleRate;
}

static void microphone_darwin_free(void *micPtr) {
    if (micPtr) {
        MicrophoneCaptureNative *mic = (__bridge MicrophoneCaptureNative*)micPtr;
        [mic stopRecording];
        CFRelease(micPtr);
    }
}

static void *g_mic_handle = NULL;

static void* get_mic_handle(void) {
    if (!g_mic_handle) {
        g_mic_handle = microphone_darwin_init();
    }
    return g_mic_handle;
}

u32 os_mic_get_available_samples(void) {
    void *handle = get_mic_handle();
    if (!handle) return 0;
    return microphone_darwin_get_available_samples(handle);
}

u32 os_mic_read_samples(i16 *buffer, u32 max_samples) {
    void *handle = get_mic_handle();
    if (!handle) return 0;
    return microphone_darwin_read_samples(handle, buffer, max_samples);
}

void os_mic_start_recording(void) {
    void *handle = get_mic_handle();
    if (!handle) return;
    microphone_darwin_start_recording(handle);
}

void os_mic_stop_recording(void) {
    void *handle = get_mic_handle();
    if (!handle) return;
    microphone_darwin_stop_recording(handle);
}

u32 os_mic_get_sample_rate(void) {
    void *handle = get_mic_handle();
    if (!handle) return 48000;
    return microphone_darwin_get_sample_rate(handle);
}
