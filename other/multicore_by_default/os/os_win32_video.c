#if 0
#include "os_video.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "dxguid.lib")

#define MF_100NS_PER_SECOND 10000000LL
#define MAX_CACHED_INPUT_VIEWS 16
#define DECODE_BUFFER_SIZE 4
#define AUDIO_BUFFER_SIZE (48000 * 2 * 2)

typedef struct {
    ID3D11Texture2D *texture;
    UINT subresource_index;
    ID3D11VideoProcessorInputView *view;
} CachedInputView;

typedef struct {
    IMFSample *sample;
    f64 presentation_time;
} VideoDecodedFrame;

typedef struct {
    VideoDecodedFrame frames[DECODE_BUFFER_SIZE];
    volatile LONG read_index;
    volatile LONG write_index;
} VideoFrameBuffer;

typedef struct {
    u8 *buffer;
    u32 capacity;
    volatile LONG read_pos;
    volatile LONG write_pos;
} AudioSampleBuffer;

struct OsVideoPlayer {
    IMFSourceReader *source_reader;
    IMFDXGIDeviceManager *device_manager;
    IMFByteStream *byte_stream;

    ID3D11Device *d3d11_device;
    ID3D11DeviceContext *d3d11_context;

    ID3D11VideoDevice *video_device;
    ID3D11VideoContext *video_context;
    ID3D11VideoProcessorEnumerator *video_processor_enum;
    ID3D11VideoProcessor *video_processor;

    ID3D11VideoProcessorOutputView *output_views[2];
    void *output_textures[2];
    u32 write_index;

    CachedInputView cached_input_views[MAX_CACHED_INPUT_VIEWS];
    u32 cached_input_view_count;

    u32 width;
    u32 height;
    u32 output_width;
    u32 output_height;
    f64 duration;
    f64 current_time;
    f64 frame_duration;
    f64 time_accumulator;

    OsVideoState state;
    b32 loop;
    b32 needs_first_frame;

    VideoFrameBuffer frame_buffer;
    HANDLE decode_thread;
    HANDLE frames_available_event;
    HANDLE slots_available_event;
    volatile b32 stop_decode_thread;
    volatile b32 seek_requested;
    volatile f64 seek_target;
    b32 decode_thread_started;

    AudioSampleBuffer audio_buffer;
    u32 audio_sample_rate;
    u32 audio_channels;
    u32 audio_bits_per_sample;
    b32 has_audio;
    DWORD video_stream_index;
    DWORD audio_stream_index;

    Allocator *allocator;
};

static b32 g_mf_initialized = false;

b32 os_video_init(void) {
    if (g_mf_initialized) {
        return true;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_ERROR("Failed to initialize COM: %", FMT_UINT((u32)hr));
        return false;
    }

    hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to initialize Media Foundation: %", FMT_UINT((u32)hr));
        return false;
    }

    g_mf_initialized = true;
    return true;
}

void os_video_shutdown(void) {
    if (g_mf_initialized) {
        MFShutdown();
        CoUninitialize();
        g_mf_initialized = false;
    }
}

internal void video_frame_buffer_init(VideoFrameBuffer *buf) {
    memset(buf, 0, sizeof(VideoFrameBuffer));
}

internal void video_frame_buffer_destroy(VideoFrameBuffer *buf) {
    UNUSED(buf);
}

internal u32 video_frame_buffer_count(VideoFrameBuffer *buf) {
    LONG w = buf->write_index;
    LONG r = buf->read_index;
    MemoryBarrier();
    return (u32)((w - r + DECODE_BUFFER_SIZE) % DECODE_BUFFER_SIZE);
}

internal u32 video_frame_buffer_free_slots(VideoFrameBuffer *buf) {
    return DECODE_BUFFER_SIZE - 1 - video_frame_buffer_count(buf);
}

internal b32 video_frame_buffer_push(VideoFrameBuffer *buf, IMFSample *sample, f64 pts) {
    LONG w = buf->write_index;
    LONG next_w = (w + 1) % DECODE_BUFFER_SIZE;
    MemoryBarrier();
    if (next_w == buf->read_index) {
        return false;
    }
    VideoDecodedFrame *frame = &buf->frames[w];
    frame->sample = sample;
    frame->presentation_time = pts;
    MemoryBarrier();
    buf->write_index = next_w;
    return true;
}

internal b32 video_frame_buffer_pop(VideoFrameBuffer *buf, IMFSample **sample, f64 *pts) {
    LONG r = buf->read_index;
    MemoryBarrier();
    if (r == buf->write_index) {
        return false;
    }
    VideoDecodedFrame *frame = &buf->frames[r];
    *sample = (IMFSample *)InterlockedExchangePointer((void **)&frame->sample, NULL);
    if (!*sample) {
        return false;
    }
    *pts = frame->presentation_time;
    MemoryBarrier();
    buf->read_index = (r + 1) % DECODE_BUFFER_SIZE;
    return true;
}

internal b32 video_frame_buffer_peek(VideoFrameBuffer *buf, f64 *pts) {
    LONG r = buf->read_index;
    MemoryBarrier();
    if (r == buf->write_index) {
        return false;
    }
    *pts = buf->frames[r].presentation_time;
    return true;
}

internal void video_frame_buffer_flush(VideoFrameBuffer *buf) {
    LONG r = buf->read_index;
    LONG w = buf->write_index;
    while (r != w) {
        IMFSample *sample = (IMFSample *)InterlockedExchangePointer((void **)&buf->frames[r].sample, NULL);
        if (sample) {
            sample->lpVtbl->Release(sample);
        }
        r = (r + 1) % DECODE_BUFFER_SIZE;
    }
    buf->read_index = 0;
    buf->write_index = 0;
}

internal void audio_sample_buffer_init(AudioSampleBuffer *buf, Allocator *allocator) {
    buf->buffer = ALLOC_ARRAY(allocator, u8, AUDIO_BUFFER_SIZE);
    buf->capacity = AUDIO_BUFFER_SIZE;
    buf->read_pos = 0;
    buf->write_pos = 0;
}

internal u32 audio_sample_buffer_available(AudioSampleBuffer *buf) {
    LONG w = buf->write_pos;
    LONG r = buf->read_pos;
    MemoryBarrier();
    return (u32)((w - r + buf->capacity) % buf->capacity);
}

internal u32 audio_sample_buffer_free_space(AudioSampleBuffer *buf) {
    return buf->capacity - 1 - audio_sample_buffer_available(buf);
}

internal u32 audio_sample_buffer_write(AudioSampleBuffer *buf, u8 *data, u32 size) {
    u32 free_space = audio_sample_buffer_free_space(buf);
    if (size > free_space) {
        size = free_space;
    }
    if (size == 0) return 0;

    LONG w = buf->write_pos;
    u32 first_chunk = buf->capacity - (u32)w;
    if (first_chunk > size) first_chunk = size;

    memcpy(buf->buffer + w, data, first_chunk);
    if (size > first_chunk) {
        memcpy(buf->buffer, data + first_chunk, size - first_chunk);
    }

    MemoryBarrier();
    buf->write_pos = (w + size) % buf->capacity;
    return size;
}

internal u32 audio_sample_buffer_read(AudioSampleBuffer *buf, u8 *data, u32 size) {
    u32 available = audio_sample_buffer_available(buf);
    if (size > available) {
        size = available;
    }
    if (size == 0) return 0;

    LONG r = buf->read_pos;
    u32 first_chunk = buf->capacity - (u32)r;
    if (first_chunk > size) first_chunk = size;

    memcpy(data, buf->buffer + r, first_chunk);
    if (size > first_chunk) {
        memcpy(data + first_chunk, buf->buffer, size - first_chunk);
    }

    MemoryBarrier();
    buf->read_pos = (r + size) % buf->capacity;
    return size;
}

internal void audio_sample_buffer_flush(AudioSampleBuffer *buf) {
    buf->read_pos = 0;
    buf->write_pos = 0;
}

internal void decode_audio_sample(OsVideoPlayer *player, IMFSample *sample) {
    IMFMediaBuffer *buffer = NULL;
    HRESULT hr = sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer);
    if (FAILED(hr)) {
        return;
    }

    BYTE *audio_data = NULL;
    DWORD audio_data_len = 0;
    hr = buffer->lpVtbl->Lock(buffer, &audio_data, NULL, &audio_data_len);
    if (SUCCEEDED(hr)) {
        audio_sample_buffer_write(&player->audio_buffer, audio_data, audio_data_len);
        buffer->lpVtbl->Unlock(buffer);
    }

    buffer->lpVtbl->Release(buffer);
}

internal DWORD WINAPI decode_thread_proc(LPVOID param) {
    OsVideoPlayer *player = (OsVideoPlayer *)param;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_ERROR("Decode thread: Failed to initialize COM");
        return 1;
    }

    b32 video_eof = false;
    b32 audio_eof = false;

    while (!player->stop_decode_thread) {
        if (player->seek_requested) {
            video_frame_buffer_flush(&player->frame_buffer);
            if (player->has_audio) {
                audio_sample_buffer_flush(&player->audio_buffer);
            }
            video_eof = false;
            audio_eof = false;
            player->seek_requested = false;
            SetEvent(player->slots_available_event);
        }

        b32 video_buffer_full = video_frame_buffer_free_slots(&player->frame_buffer) == 0;
        b32 audio_buffer_full = !player->has_audio || audio_sample_buffer_free_space(&player->audio_buffer) < 8192;

        if (video_buffer_full && audio_buffer_full) {
            WaitForSingleObject(player->slots_available_event, 16);
            continue;
        }

        DWORD stream_to_read = (DWORD)MF_SOURCE_READER_ANY_STREAM;
        if (video_buffer_full || video_eof) {
            stream_to_read = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
        } else if (audio_buffer_full || audio_eof || !player->has_audio) {
            stream_to_read = (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM;
        }

        DWORD stream_index = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample *sample = NULL;

        hr = player->source_reader->lpVtbl->ReadSample(
            player->source_reader,
            stream_to_read,
            0,
            &stream_index,
            &flags,
            &timestamp,
            &sample
        );

        if (FAILED(hr)) {
            Sleep(1);
            continue;
        }

        b32 is_video = (stream_index == player->video_stream_index);
        b32 is_audio = (stream_index == player->audio_stream_index);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (is_video) video_eof = true;
            if (is_audio) audio_eof = true;

            if (video_eof && (audio_eof || !player->has_audio)) {
                if (player->loop) {
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    var.vt = VT_I8;
                    var.hVal.QuadPart = 0;
                    player->source_reader->lpVtbl->SetCurrentPosition(player->source_reader, &GUID_NULL, &var);
                    PropVariantClear(&var);
                    video_eof = false;
                    audio_eof = false;
                    continue;
                } else {
                    player->state = OS_VIDEO_STATE_ENDED;
                    break;
                }
            }
            continue;
        }

        if (sample) {
            if (is_video) {
                f64 pts = (f64)timestamp / (f64)MF_100NS_PER_SECOND;
                if (video_frame_buffer_push(&player->frame_buffer, sample, pts)) {
                    SetEvent(player->frames_available_event);
                } else {
                    sample->lpVtbl->Release(sample);
                }
            } else if (is_audio && player->has_audio) {
                decode_audio_sample(player, sample);
                sample->lpVtbl->Release(sample);
            } else {
                sample->lpVtbl->Release(sample);
            }
        }
    }

    CoUninitialize();
    return 0;
}

internal void stop_decode_thread(OsVideoPlayer *player) {
    if (!player->decode_thread_started) return;

    player->stop_decode_thread = true;
    SetEvent(player->slots_available_event);
    WaitForSingleObject(player->decode_thread, 2000);
    CloseHandle(player->decode_thread);
    player->decode_thread = NULL;
    player->decode_thread_started = false;
}

internal void start_decode_thread(OsVideoPlayer *player) {
    if (player->decode_thread_started) return;

    player->stop_decode_thread = false;
    player->decode_thread = CreateThread(NULL, 0, decode_thread_proc, player, 0, NULL);
    if (player->decode_thread) {
        player->decode_thread_started = true;
    }
}

internal IMFByteStream *create_byte_stream_from_file(const char *file_path) {
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, file_path, -1, NULL, 0);
    if (wide_len <= 0) {
        LOG_ERROR("Failed to get wide string length for file path");
        return NULL;
    }

    wchar_t wide_path[MAX_PATH];
    if (wide_len > MAX_PATH) {
        LOG_ERROR("File path too long");
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, file_path, -1, wide_path, wide_len);

    IMFByteStream *byte_stream = NULL;
    HRESULT hr = MFCreateFile(
        MF_ACCESSMODE_READ,
        MF_OPENMODE_FAIL_IF_NOT_EXIST,
        MF_FILEFLAGS_NONE,
        wide_path,
        &byte_stream
    );

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create byte stream from file: %", FMT_UINT((u32)hr));
        return NULL;
    }

    return byte_stream;
}

internal b32 setup_dxgi_device_manager(OsVideoPlayer *player, UINT *reset_token) {
    ID3D10Multithread *multithread = NULL;
    HRESULT hr = player->d3d11_device->lpVtbl->QueryInterface(
        player->d3d11_device,
        &IID_ID3D10Multithread,
        (void **)&multithread
    );
    if (SUCCEEDED(hr) && multithread) {
        multithread->lpVtbl->SetMultithreadProtected(multithread, TRUE);
        multithread->lpVtbl->Release(multithread);
    }

    hr = MFCreateDXGIDeviceManager(reset_token, &player->device_manager);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DXGI device manager: %", FMT_UINT((u32)hr));
        return false;
    }

    hr = player->device_manager->lpVtbl->ResetDevice(
        player->device_manager,
        (IUnknown *)player->d3d11_device,
        *reset_token
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to reset DXGI device: %", FMT_UINT((u32)hr));
        return false;
    }

    return true;
}

internal b32 create_source_reader(OsVideoPlayer *player, IMFByteStream *byte_stream, UINT reset_token) {
    UNUSED(reset_token);

    IMFAttributes *attributes = NULL;
    HRESULT hr = MFCreateAttributes(&attributes, 2);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create attributes: %", FMT_UINT((u32)hr));
        return false;
    }

    hr = attributes->lpVtbl->SetUnknown(
        attributes,
        &MF_SOURCE_READER_D3D_MANAGER,
        (IUnknown *)player->device_manager
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set D3D manager: %", FMT_UINT((u32)hr));
        attributes->lpVtbl->Release(attributes);
        return false;
    }

    hr = attributes->lpVtbl->SetUINT32(
        attributes,
        &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,
        TRUE
    );

    hr = MFCreateSourceReaderFromByteStream(byte_stream, attributes, &player->source_reader);
    attributes->lpVtbl->Release(attributes);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create source reader from byte stream: %", FMT_UINT((u32)hr));
        return false;
    }

    return true;
}

internal b32 configure_video_output(OsVideoPlayer *player) {
    DWORD actual_index = 0;
    DWORD flags = 0;
    player->source_reader->lpVtbl->GetStreamSelection(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        (BOOL *)&flags
    );

    for (DWORD i = 0; i < 16; i++) {
        IMFMediaType *type = NULL;
        HRESULT hr = player->source_reader->lpVtbl->GetNativeMediaType(player->source_reader, i, 0, &type);
        if (FAILED(hr)) continue;
        GUID major_type;
        hr = type->lpVtbl->GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type);
        type->lpVtbl->Release(type);
        if (SUCCEEDED(hr) && IsEqualGUID(&major_type, &MFMediaType_Video)) {
            actual_index = i;
            break;
        }
    }
    player->video_stream_index = actual_index;

    IMFMediaType *native_type = NULL;
    HRESULT hr = player->source_reader->lpVtbl->GetNativeMediaType(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &native_type
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get native media type: %", FMT_UINT((u32)hr));
        return false;
    }

    UINT64 frame_size = 0;
    hr = native_type->lpVtbl->GetUINT64(native_type, &MF_MT_FRAME_SIZE, &frame_size);
    if (SUCCEEDED(hr)) {
        player->width = (UINT32)(frame_size >> 32);
        player->height = (UINT32)(frame_size & 0xFFFFFFFF);
    }

    UINT64 frame_rate = 0;
    hr = native_type->lpVtbl->GetUINT64(native_type, &MF_MT_FRAME_RATE, &frame_rate);
    if (SUCCEEDED(hr)) {
        UINT32 fps_num = (UINT32)(frame_rate >> 32);
        UINT32 fps_den = (UINT32)(frame_rate & 0xFFFFFFFF);
        if (fps_num > 0) {
            player->frame_duration = (f64)fps_den / (f64)fps_num;
        } else {
            player->frame_duration = 1.0 / 30.0;
        }
    } else {
        player->frame_duration = 1.0 / 30.0;
    }

    native_type->lpVtbl->Release(native_type);

    IMFMediaType *output_type = NULL;
    hr = MFCreateMediaType(&output_type);
    if (FAILED(hr)) {
        return false;
    }

    hr = output_type->lpVtbl->SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    if (FAILED(hr)) {
        output_type->lpVtbl->Release(output_type);
        return false;
    }

    hr = output_type->lpVtbl->SetGUID(output_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
    if (FAILED(hr)) {
        output_type->lpVtbl->Release(output_type);
        return false;
    }

    hr = player->source_reader->lpVtbl->SetCurrentMediaType(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        NULL,
        output_type
    );
    output_type->lpVtbl->Release(output_type);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to set output media type: %", FMT_UINT((u32)hr));
        return false;
    }

    return true;
}

internal b32 configure_audio_output(OsVideoPlayer *player) {
    for (DWORD i = 0; i < 16; i++) {
        IMFMediaType *type = NULL;
        HRESULT hr = player->source_reader->lpVtbl->GetNativeMediaType(player->source_reader, i, 0, &type);
        if (FAILED(hr)) continue;
        GUID major_type;
        hr = type->lpVtbl->GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type);
        type->lpVtbl->Release(type);
        if (SUCCEEDED(hr) && IsEqualGUID(&major_type, &MFMediaType_Audio)) {
            player->audio_stream_index = i;
            break;
        }
    }

    IMFMediaType *native_type = NULL;
    HRESULT hr = player->source_reader->lpVtbl->GetNativeMediaType(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        0,
        &native_type
    );
    if (FAILED(hr)) {
        player->has_audio = false;
        return true;
    }

    UINT32 sample_rate = 0;
    hr = native_type->lpVtbl->GetUINT32(native_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate);
    if (SUCCEEDED(hr)) {
        player->audio_sample_rate = sample_rate;
    } else {
        player->audio_sample_rate = 48000;
    }

    UINT32 channels = 0;
    hr = native_type->lpVtbl->GetUINT32(native_type, &MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (SUCCEEDED(hr)) {
        player->audio_channels = channels;
    } else {
        player->audio_channels = 2;
    }

    native_type->lpVtbl->Release(native_type);

    IMFMediaType *output_type = NULL;
    hr = MFCreateMediaType(&output_type);
    if (FAILED(hr)) {
        player->has_audio = false;
        return true;
    }

    output_type->lpVtbl->SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    output_type->lpVtbl->SetGUID(output_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    output_type->lpVtbl->SetUINT32(output_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    output_type->lpVtbl->SetUINT32(output_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, player->audio_sample_rate);
    output_type->lpVtbl->SetUINT32(output_type, &MF_MT_AUDIO_NUM_CHANNELS, player->audio_channels);
    output_type->lpVtbl->SetUINT32(output_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, player->audio_channels * 2);
    output_type->lpVtbl->SetUINT32(output_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
        player->audio_sample_rate * player->audio_channels * 2);

    hr = player->source_reader->lpVtbl->SetCurrentMediaType(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        NULL,
        output_type
    );
    output_type->lpVtbl->Release(output_type);

    if (FAILED(hr)) {
        LOG_WARN("Failed to set audio output type: %", FMT_UINT((u32)hr));
        player->has_audio = false;
        return true;
    }

    player->audio_bits_per_sample = 16;
    player->has_audio = true;

    LOG_INFO("Audio stream: % Hz, % channels",
        FMT_UINT(player->audio_sample_rate),
        FMT_UINT(player->audio_channels));

    return true;
}

internal b32 get_video_duration(OsVideoPlayer *player) {
    PROPVARIANT var;
    PropVariantInit(&var);

    HRESULT hr = player->source_reader->lpVtbl->GetPresentationAttribute(
        player->source_reader,
        (DWORD)MF_SOURCE_READER_MEDIASOURCE,
        &MF_PD_DURATION,
        &var
    );

    if (SUCCEEDED(hr) && var.vt == VT_UI8) {
        player->duration = (f64)var.uhVal.QuadPart / (f64)MF_100NS_PER_SECOND;
    } else {
        player->duration = 0.0;
    }

    PropVariantClear(&var);
    return true;
}

internal b32 create_video_processor(OsVideoPlayer *player) {
    HRESULT hr = player->d3d11_device->lpVtbl->QueryInterface(
        player->d3d11_device,
        &IID_ID3D11VideoDevice,
        (void **)&player->video_device
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get ID3D11VideoDevice: %", FMT_UINT((u32)hr));
        return false;
    }

    hr = player->d3d11_context->lpVtbl->QueryInterface(
        player->d3d11_context,
        &IID_ID3D11VideoContext,
        (void **)&player->video_context
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get ID3D11VideoContext: %", FMT_UINT((u32)hr));
        return false;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {0};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputWidth = player->width;
    content_desc.InputHeight = player->height;
    content_desc.OutputWidth = player->output_width;
    content_desc.OutputHeight = player->output_height;
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = player->video_device->lpVtbl->CreateVideoProcessorEnumerator(
        player->video_device,
        &content_desc,
        &player->video_processor_enum
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create video processor enumerator: %", FMT_UINT((u32)hr));
        return false;
    }

    hr = player->video_device->lpVtbl->CreateVideoProcessor(
        player->video_device,
        player->video_processor_enum,
        0,
        &player->video_processor
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create video processor: %", FMT_UINT((u32)hr));
        return false;
    }

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE input_color_space = {0};
    input_color_space.YCbCr_Matrix = 1;
    input_color_space.Nominal_Range = 1;

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_color_space = {0};
    output_color_space.Usage = 0;
    output_color_space.RGB_Range = 0;

    player->video_context->lpVtbl->VideoProcessorSetStreamColorSpace(
        player->video_context,
        player->video_processor,
        0,
        &input_color_space
    );

    player->video_context->lpVtbl->VideoProcessorSetOutputColorSpace(
        player->video_context,
        player->video_processor,
        &output_color_space
    );

    return true;
}

OsVideoPlayer *os_video_player_create(OsVideoPlayerDesc *desc, Allocator *allocator) {
    if (!desc || !desc->file_path) {
        LOG_ERROR("Invalid video player descriptor");
        return NULL;
    }

    if (!desc->device || !desc->device_context) {
        LOG_ERROR("D3D11 device and context are required");
        return NULL;
    }

    if (!g_mf_initialized) {
        LOG_ERROR("Video system not initialized - call os_video_init first");
        return NULL;
    }

    OsVideoPlayer *player = ALLOC(allocator, OsVideoPlayer);
    if (!player) {
        return NULL;
    }

    memset(player, 0, sizeof(OsVideoPlayer));
    player->allocator = allocator;
    player->d3d11_device = (ID3D11Device *)desc->device;
    player->d3d11_context = (ID3D11DeviceContext *)desc->device_context;
    player->loop = desc->loop;
    player->state = OS_VIDEO_STATE_IDLE;

    player->byte_stream = create_byte_stream_from_file(desc->file_path);
    if (!player->byte_stream) {
        LOG_ERROR("Failed to create byte stream from file");
        os_video_player_destroy(player);
        return NULL;
    }

    UINT reset_token = 0;
    if (!setup_dxgi_device_manager(player, &reset_token)) {
        os_video_player_destroy(player);
        return NULL;
    }

    if (!create_source_reader(player, player->byte_stream, reset_token)) {
        os_video_player_destroy(player);
        return NULL;
    }

    if (!configure_video_output(player)) {
        os_video_player_destroy(player);
        return NULL;
    }

    configure_audio_output(player);

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

    if (!get_video_duration(player)) {
        os_video_player_destroy(player);
        return NULL;
    }

    if (!create_video_processor(player)) {
        os_video_player_destroy(player);
        return NULL;
    }

    video_frame_buffer_init(&player->frame_buffer);
    if (player->has_audio) {
        audio_sample_buffer_init(&player->audio_buffer, allocator);
    }
    player->frames_available_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    player->slots_available_event = CreateEvent(NULL, FALSE, TRUE, NULL);

    LOG_INFO("Video loaded: %x% -> %x% @ % fps, duration: % seconds",
        FMT_UINT(player->width),
        FMT_UINT(player->height),
        FMT_UINT(player->output_width),
        FMT_UINT(player->output_height),
        FMT_FLOAT(1.0 / player->frame_duration),
        FMT_FLOAT(player->duration)
    );

    return player;
}

internal void clear_input_view_cache(OsVideoPlayer *player);

void os_video_player_destroy(OsVideoPlayer *player) {
    if (!player) {
        return;
    }

    stop_decode_thread(player);
    video_frame_buffer_flush(&player->frame_buffer);
    video_frame_buffer_destroy(&player->frame_buffer);

    if (player->frames_available_event) {
        CloseHandle(player->frames_available_event);
    }
    if (player->slots_available_event) {
        CloseHandle(player->slots_available_event);
    }

    clear_input_view_cache(player);

    for (u32 i = 0; i < 2; i++) {
        if (player->output_views[i]) {
            player->output_views[i]->lpVtbl->Release(player->output_views[i]);
        }
    }

    if (player->video_processor) {
        player->video_processor->lpVtbl->Release(player->video_processor);
    }

    if (player->video_processor_enum) {
        player->video_processor_enum->lpVtbl->Release(player->video_processor_enum);
    }

    if (player->video_context) {
        player->video_context->lpVtbl->Release(player->video_context);
    }

    if (player->video_device) {
        player->video_device->lpVtbl->Release(player->video_device);
    }

    if (player->source_reader) {
        player->source_reader->lpVtbl->Release(player->source_reader);
    }

    if (player->device_manager) {
        player->device_manager->lpVtbl->Release(player->device_manager);
    }

    if (player->byte_stream) {
        player->byte_stream->lpVtbl->Release(player->byte_stream);
    }
}

#define PREFILL_FRAME_COUNT 2
#define PREFILL_TIMEOUT_MS 50
#define PREFILL_MAX_ATTEMPTS 20

void os_video_player_play(OsVideoPlayer *player) {
    if (!player) return;
    if (player->state != OS_VIDEO_STATE_PLAYING) {
        player->needs_first_frame = true;
        player->time_accumulator = 0;
        start_decode_thread(player);

        u32 attempts = 0;
        while (video_frame_buffer_count(&player->frame_buffer) < PREFILL_FRAME_COUNT &&
               attempts < PREFILL_MAX_ATTEMPTS) {
            WaitForSingleObject(player->frames_available_event, PREFILL_TIMEOUT_MS);
            if (player->state == OS_VIDEO_STATE_ENDED || player->state == OS_VIDEO_STATE_ERROR) {
                break;
            }
            attempts++;
        }
    }
    player->state = OS_VIDEO_STATE_PLAYING;
}

void os_video_player_pause(OsVideoPlayer *player) {
    if (!player) return;
    stop_decode_thread(player);
    player->state = OS_VIDEO_STATE_PAUSED;
}

void os_video_player_stop(OsVideoPlayer *player) {
    if (!player) return;
    stop_decode_thread(player);
    video_frame_buffer_flush(&player->frame_buffer);
    player->state = OS_VIDEO_STATE_IDLE;
    player->current_time = 0;
    os_video_player_seek(player, 0);
}

void os_video_player_seek(OsVideoPlayer *player, f64 time_seconds) {
    if (!player || !player->source_reader) return;

    clear_input_view_cache(player);

    player->seek_requested = true;

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = (LONGLONG)(time_seconds * MF_100NS_PER_SECOND);

    player->source_reader->lpVtbl->SetCurrentPosition(player->source_reader, &GUID_NULL, &var);
    player->current_time = time_seconds;
    player->time_accumulator = 0;
    player->needs_first_frame = true;

    PropVariantClear(&var);
}

void os_video_player_set_loop(OsVideoPlayer *player, b32 loop) {
    if (!player) return;
    player->loop = loop;
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
    if (!player) return 0.0;
    return player->current_time;
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

    for (u32 i = 0; i < 2; i++) {
        if (player->output_views[i]) {
            player->output_views[i]->lpVtbl->Release(player->output_views[i]);
            player->output_views[i] = NULL;
        }
    }

    player->output_textures[0] = texture_a;
    player->output_textures[1] = texture_b;
    player->write_index = 0;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {0};
    output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    output_view_desc.Texture2D.MipSlice = 0;

    for (u32 i = 0; i < 2; i++) {
        HRESULT hr = player->video_device->lpVtbl->CreateVideoProcessorOutputView(
            player->video_device,
            (ID3D11Resource *)player->output_textures[i],
            player->video_processor_enum,
            &output_view_desc,
            &player->output_views[i]
        );
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create output view %: %", FMT_UINT(i), FMT_UINT((u32)hr));
        }
    }
}

void *os_video_player_get_display_texture(OsVideoPlayer *player) {
    if (!player) return NULL;
    u32 display_index = 1 - player->write_index;
    return player->output_textures[display_index];
}

internal void clear_input_view_cache(OsVideoPlayer *player) {
    for (u32 i = 0; i < player->cached_input_view_count; i++) {
        if (player->cached_input_views[i].view) {
            player->cached_input_views[i].view->lpVtbl->Release(player->cached_input_views[i].view);
        }
    }
    player->cached_input_view_count = 0;
}

internal ID3D11VideoProcessorInputView *get_or_create_input_view(
    OsVideoPlayer *player,
    ID3D11Texture2D *texture,
    UINT subresource_index
) {
    for (u32 i = 0; i < player->cached_input_view_count; i++) {
        CachedInputView *cached = &player->cached_input_views[i];
        if (cached->texture == texture && cached->subresource_index == subresource_index) {
            return cached->view;
        }
    }

    if (player->cached_input_view_count >= MAX_CACHED_INPUT_VIEWS) {
        clear_input_view_cache(player);
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
    input_view_desc.FourCC = 0;
    input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_view_desc.Texture2D.MipSlice = 0;
    input_view_desc.Texture2D.ArraySlice = subresource_index;

    ID3D11VideoProcessorInputView *input_view = NULL;
    HRESULT hr = player->video_device->lpVtbl->CreateVideoProcessorInputView(
        player->video_device,
        (ID3D11Resource *)texture,
        player->video_processor_enum,
        &input_view_desc,
        &input_view
    );

    if (FAILED(hr) || !input_view) {
        return NULL;
    }

    CachedInputView *slot = &player->cached_input_views[player->cached_input_view_count++];
    slot->texture = texture;
    slot->subresource_index = subresource_index;
    slot->view = input_view;

    return input_view;
}

internal b32 blit_sample_to_output(OsVideoPlayer *player, IMFSample *sample) {
    if (!player || !sample) return false;
    if (!player->output_views[player->write_index]) return false;

    IMFMediaBuffer *buffer = NULL;
    HRESULT hr = sample->lpVtbl->GetBufferByIndex(sample, 0, &buffer);
    if (FAILED(hr)) {
        return false;
    }

    IMFDXGIBuffer *dxgi_buffer = NULL;
    hr = buffer->lpVtbl->QueryInterface(buffer, &IID_IMFDXGIBuffer, (void **)&dxgi_buffer);

    b32 blit_success = false;

    if (SUCCEEDED(hr) && dxgi_buffer) {
        ID3D11Texture2D *src_texture = NULL;
        UINT subresource_index = 0;

        hr = dxgi_buffer->lpVtbl->GetResource(dxgi_buffer, &IID_ID3D11Texture2D, (void **)&src_texture);
        if (SUCCEEDED(hr) && src_texture) {
            dxgi_buffer->lpVtbl->GetSubresourceIndex(dxgi_buffer, &subresource_index);

            ID3D11VideoProcessorInputView *input_view = get_or_create_input_view(
                player,
                src_texture,
                subresource_index
            );

            if (input_view) {
                D3D11_VIDEO_PROCESSOR_STREAM stream = {0};
                stream.Enable = TRUE;
                stream.pInputSurface = input_view;

                hr = player->video_context->lpVtbl->VideoProcessorBlt(
                    player->video_context,
                    player->video_processor,
                    player->output_views[player->write_index],
                    0,
                    1,
                    &stream
                );

                if (SUCCEEDED(hr)) {
                    player->write_index = 1 - player->write_index;
                    blit_success = true;
                }
            }

            src_texture->lpVtbl->Release(src_texture);
        }
        dxgi_buffer->lpVtbl->Release(dxgi_buffer);
    }

    buffer->lpVtbl->Release(buffer);

    return blit_success;
}

#define MAX_FRAME_SKIP 4

b32 os_video_player_update(OsVideoPlayer *player, f64 delta_time) {
    if (!player) return false;
    if (player->state != OS_VIDEO_STATE_PLAYING) return false;
    if (!player->output_textures[0] || !player->output_textures[1]) return false;

    player->time_accumulator += delta_time;

    b32 need_frame = player->needs_first_frame ||
                     player->time_accumulator >= player->frame_duration;

    if (!need_frame) {
        return false;
    }

    IMFSample *sample = NULL;
    f64 pts = 0;

    if (!video_frame_buffer_pop(&player->frame_buffer, &sample, &pts)) {
        return false;
    }

    SetEvent(player->slots_available_event);

    f64 target_time = player->current_time + player->time_accumulator;
    u32 skipped = 0;

    while (skipped < MAX_FRAME_SKIP) {
        f64 next_pts = 0;
        if (!video_frame_buffer_peek(&player->frame_buffer, &next_pts)) {
            break;
        }
        if (next_pts > target_time) {
            break;
        }
        IMFSample *next_sample = NULL;
        f64 next_sample_pts = 0;
        if (!video_frame_buffer_pop(&player->frame_buffer, &next_sample, &next_sample_pts)) {
            break;
        }
        sample->lpVtbl->Release(sample);
        sample = next_sample;
        pts = next_sample_pts;
        SetEvent(player->slots_available_event);
        skipped++;
    }

    b32 blit_success = blit_sample_to_output(player, sample);
    sample->lpVtbl->Release(sample);

    if (blit_success) {
        player->current_time = pts;
        if (player->needs_first_frame) {
            player->needs_first_frame = false;
            player->time_accumulator = 0;
        } else {
            player->time_accumulator -= player->frame_duration * (1 + skipped);
            if (player->time_accumulator < 0) {
                player->time_accumulator = 0;
            }
        }
    }

    return blit_success;
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
    if (!player || !buffer || !player->has_audio) return 0;
    u32 bytes_read = audio_sample_buffer_read(&player->audio_buffer, buffer, max_bytes);
    if (bytes_read > 0) {
        SetEvent(player->slots_available_event);
    }
    return bytes_read;
}
#endif
