// src/backends/backend_coreaudio.cpp
#include "../include/arxsound_backend.hpp"
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cmath>

namespace arxsound {

// ============================================================================
// CoreAudio internal structures
// ============================================================================

struct coreaudio_context_state {
    AudioDeviceID default_output_device;
    AudioDeviceID default_input_device;
    std::mutex mutex;
};

struct coreaudio_device_state {
    // Audio Unit
    AudioUnit audio_unit = nullptr;
    
    // Audio format
    AudioStreamBasicDescription asbd;
    
    // Device info
    AS_device_type type{AS_device_type::PLAYBACK};
    AudioDeviceID device_id = kAudioObjectUnknown;
    
    // Callbacks
    AS_device_data_proc data_callback = nullptr;
    void* user_data = nullptr;
    
    // State
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    
    // Volume
    float master_volume = 1.0f;
    
    // I/O buffer
    std::vector<float> io_buffer;
};

// ============================================================================
// CoreAudio render callback
// ============================================================================

static OSStatus coreaudio_render_callback(
    void* user_data,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data
) {
    (void)in_time_stamp;
    (void)in_bus_number;
    
    auto* state = static_cast<coreaudio_device_state*>(user_data);
    if (!state || !state->data_callback) {
        // Fill with silence
        for (UInt32 i = 0; i < io_data->mNumberBuffers; ++i) {
            memset(io_data->mBuffers[i].mData, 0, io_data->mBuffers[i].mDataByteSize);
        }
        return noErr;
    }
    
    // Call user callback
    // Note: CoreAudio provides interleaved float32 data
    void* output_buffer = io_data->mBuffers[0].mData;
    state->data_callback(
        nullptr,  // device
        output_buffer,
        nullptr,  // input (for playback)
        in_number_frames
    );
    
    // Apply master volume
    if (state->master_volume != 1.0f) {
        float* f32_data = static_cast<float*>(output_buffer);
        UInt32 total_samples = in_number_frames * state->asbd.mChannelsPerFrame;
        for (UInt32 i = 0; i < total_samples; ++i) {
            f32_data[i] *= state->master_volume;
        }
    }
    
    return noErr;
}

static OSStatus coreaudio_capture_callback(
    void* user_data,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data
) {
    (void)io_action_flags;
    (void)in_time_stamp;
    (void)in_bus_number;
    (void)io_data;
    
    auto* state = static_cast<coreaudio_device_state*>(user_data);
    if (!state || !state->data_callback) {
        return noErr;
    }
    
    // For capture, we need to render into a buffer first
    AudioBufferList buffer_list;
    buffer_list.mNumberBuffers = 1;
    buffer_list.mBuffers[0].mNumberChannels = state->asbd.mChannelsPerFrame;
    buffer_list.mBuffers[0].mDataByteSize = in_number_frames * 
                                            state->asbd.mChannelsPerFrame * 
                                            sizeof(float);
    
    // Allocate temporary buffer
    std::vector<float> temp_buffer(in_number_frames * state->asbd.mChannelsPerFrame);
    buffer_list.mBuffers[0].mData = temp_buffer.data();
    
    OSStatus status = AudioUnitRender(
        state->audio_unit,
        io_action_flags,
        in_time_stamp,
        1,  // Input bus
        in_number_frames,
        &buffer_list
    );
    
    if (status == noErr) {
        state->data_callback(
            nullptr,  // device
            nullptr,  // output
            temp_buffer.data(),
            in_number_frames
        );
    }
    
    return status;
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result coreaudio_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)context;
    (void)config;
    (void)callbacks;
    
    // CoreAudio doesn't require special context initialization
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_context_uninit(AS_context* context) {
    (void)context;
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyOwnerWorld
    };
    
    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size
    );
    
    if (status != noErr) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    UInt32 device_count = data_size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> device_ids(device_count);
    
    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size,
        device_ids.data()
    );
    
    if (status != noErr) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    // Get default device
    AudioDeviceID default_device = kAudioObjectUnknown;
    property_address.mSelector = (type == AS_device_type::CAPTURE)
        ? kAudioHardwarePropertyDefaultInputDevice
        : kAudioHardwarePropertyDefaultOutputDevice;
    
    data_size = sizeof(AudioDeviceID);
    AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size,
        &default_device
    );
    
    // Enumerate each device
    for (UInt32 i = 0; i < device_count; ++i) {
        AudioDeviceID device_id = device_ids[i];
        
        // Check if device has input or output
        property_address.mSelector = kAudioDevicePropertyStreamConfiguration;
        property_address.mScope = (type == AS_device_type::CAPTURE)
            ? kAudioDevicePropertyScopeInput
            : kAudioDevicePropertyScopeOutput;
        
        data_size = 0;
        status = AudioObjectGetPropertyDataSize(
            device_id,
            &property_address,
            0,
            nullptr,
            &data_size
        );
        
        if (status != noErr || data_size == 0) {
            continue;
        }
        
        // Get device name
        CFStringRef name_ref = nullptr;
        property_address.mSelector = kAudioObjectPropertyName;
        property_address.mScope = kAudioObjectPropertyScopeGlobal;
        data_size = sizeof(CFStringRef);
        
        AudioObjectGetPropertyData(
            device_id,
            &property_address,
            0,
            nullptr,
            &data_size,
            &name_ref
        );
        
        AS_device_info info{};
        ZeroMemory(&info, sizeof(info));
        
        if (name_ref) {
            CFIndex length = CFStringGetLength(name_ref);
            CFStringGetCString(name_ref, info.name, sizeof(info.name), kCFStringEncodingUTF8);
            CFRelease(name_ref);
        } else {
            snprintf(info.name, sizeof(info.name), "CoreAudio Device %u", i);
        }
        
        // Store device ID
        info.id.coreaudio[0] = static_cast<char>(device_id & 0xFF);
        info.id.coreaudio[1] = static_cast<char>((device_id >> 8) & 0xFF);
        info.id.coreaudio[2] = static_cast<char>((device_id >> 16) & 0xFF);
        info.id.coreaudio[3] = static_cast<char>((device_id >> 24) & 0xFF);
        
        info.is_default = (device_id == default_device) ? AS_TRUE : AS_FALSE;
        info.min_sample_rate = 8000;
        info.max_sample_rate = 192000;
        info.min_channels = 1;
        info.max_channels = 8;
        info.supports_shared = AS_TRUE;
        info.supports_exclusive = AS_FALSE;
        
        // Native formats
        info.native_format_count = 0;
        auto add_format = [&](AS_format fmt, uint32_t ch, uint32_t sr) {
            if (info.native_format_count < AS_device_info::MAX_NATIVE_FORMATS) {
                info.native_formats[info.native_format_count++] = {fmt, ch, sr};
            }
        };
        add_format(AS_format::F32, 2, 48000);
        add_format(AS_format::F32, 2, 44100);
        
        callback(context, type, &info, user_data);
    }
    
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    if (!info) return AS_result::INVALID_ARGS;
    
    return coreaudio_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result coreaudio_on_device_init(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    const AS_device_config* config,
    AS_device* device
) {
    if (!context || !config || !device) {
        return AS_result::INVALID_ARGS;
    }
    
    auto* state = new(std::nothrow) coreaudio_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    state->data_callback = config->data_callback;
    state->user_data = config->user_data;
    
    // Create Audio Component
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    // Create Audio Unit
    OSStatus status = AudioComponentInstanceNew(component, &state->audio_unit);
    if (status != noErr || !state->audio_unit) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    // Set audio format
    AS_format format = (type == AS_device_type::CAPTURE)
        ? config->capture.format
        : config->playback.format;
    if (format == AS_format::UNKNOWN) {
        format = AS_format::F32;
    }
    
    uint32_t channels = (type == AS_device_type::CAPTURE)
        ? config->capture.channels
        : config->playback.channels;
    if (channels == 0) channels = 2;
    
    uint32_t sample_rate = config->sample_rate;
    if (sample_rate == 0) sample_rate = 48000;
    
    state->asbd.mSampleRate = static_cast<Float64>(sample_rate);
    state->asbd.mFormatID = kAudioFormatLinearPCM;
    state->asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    state->asbd.mBytesPerPacket = sizeof(float) * channels;
    state->asbd.mFramesPerPacket = 1;
    state->asbd.mBytesPerFrame = sizeof(float) * channels;
    state->asbd.mChannelsPerFrame = channels;
    state->asbd.mBitsPerChannel = sizeof(float) * 8;
    
    // Set format on Audio Unit
    status = AudioUnitSetProperty(
        state->audio_unit,
        kAudioUnitProperty_StreamFormat,
        (type == AS_device_type::CAPTURE) ? kAudioUnitScope_Input : kAudioUnitScope_Output,
        0,
        &state->asbd,
        sizeof(state->asbd)
    );
    
    if (status != noErr) {
        AudioComponentInstanceDispose(state->audio_unit);
        delete state;
        return AS_result::FORMAT_NOT_SUPPORTED;
    }
    
    // Set render callback
    AURenderCallbackStruct callback_struct;
    callback_struct.inputProc = (type == AS_device_type::CAPTURE) 
        ? coreaudio_capture_callback 
        : coreaudio_render_callback;
    callback_struct.inputProcRefCon = state;
    
    status = AudioUnitSetProperty(
        state->audio_unit,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Global,
        0,
        &callback_struct,
        sizeof(callback_struct)
    );
    
    if (status != noErr) {
        AudioComponentInstanceDispose(state->audio_unit);
        delete state;
        return AS_result::ERROR;
    }
    
    // Initialize Audio Unit
    status = AudioUnitInitialize(state->audio_unit);
    if (status != noErr) {
        AudioComponentInstanceDispose(state->audio_unit);
        delete state;
        return AS_result::ERROR;
    }
    
    // Update device runtime info
    device->p_impl->actual_sample_rate = sample_rate;
    device->p_impl->actual_playback_format = format;
    device->p_impl->actual_playback_channels = channels;
    device->p_impl->actual_capture_format = format;
    device->p_impl->actual_capture_channels = channels;
    
    // Store state
    device->p_impl->backend_data = static_cast<void*>(state);
    
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<coreaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    // Stop if running
    if (state->running.load()) {
        coreaudio_on_device_stop(device);
    }
    
    // Dispose Audio Unit
    if (state->audio_unit) {
        AudioUnitUninitialize(state->audio_unit);
        AudioComponentInstanceDispose(state->audio_unit);
    }
    
    delete state;
    device->p_impl->backend_data = nullptr;
    
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<coreaudio_device_state*>(device->p_impl->backend_data);
    if (!state || !state->audio_unit) {
        return AS_result::INVALID_OPERATION;
    }
    
    if (state->running.load()) {
        return AS_result::SUCCESS;
    }
    
    // Start Audio Unit
    OSStatus status = AudioOutputUnitStart(state->audio_unit);
    if (status != noErr) {
        return AS_result::ERROR;
    }
    
    state->running.store(true);
    state->started.store(true);
    
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<coreaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    if (!state->running.load()) {
        return AS_result::SUCCESS;
    }
    
    // Stop Audio Unit
    if (state->audio_unit) {
        AudioOutputUnitStop(state->audio_unit);
    }
    
    state->running.store(false);
    state->started.store(false);
    
    return AS_result::SUCCESS;
}

static AS_result coreaudio_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;  // CoreAudio is callback-based
}

static AS_result coreaudio_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;  // CoreAudio is callback-based
}

// ============================================================================
// Backend registration
// ============================================================================

static AS_bool32 coreaudio_is_available() {
    // Check if we can find an output component
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    return (component != nullptr) ? AS_TRUE : AS_FALSE;
}

static AS_backend_callbacks g_coreaudio_callbacks = {
    coreaudio_on_context_init,
    coreaudio_on_context_uninit,
    coreaudio_on_context_enumerate_devices,
    coreaudio_on_context_get_device_info,
    coreaudio_on_device_init,
    coreaudio_on_device_uninit,
    coreaudio_on_device_start,
    coreaudio_on_device_stop,
    coreaudio_on_device_read,
    coreaudio_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_coreaudio_info = {
    AS_backend::COREAUDIO,
    "CoreAudio",
    coreaudio_is_available,
    g_coreaudio_callbacks
};

AS_result AS_register_coreaudio_backend() {
    return AS_register_backend(&g_coreaudio_info);
}

// Auto-register on load
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_coreaudio_backend() {
    AS_register_coreaudio_backend();
}
#endif

} // namespace arxsound