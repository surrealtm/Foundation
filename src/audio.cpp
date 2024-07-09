#include "audio.h"
#include "math/maths.h"
#include "os_specific.h"
#include "drflac.h"

#if FOUNDATION_WIN32
# include <mmdeviceapi.h>
# include <audioclient.h>

// The order in which the samples are interpreted by the OS
# define RIGHT_AUDIO_CHANNEL 0
# define LEFT_AUDIO_CHANNEL  1

struct Audio_Win32_State {
    IMMDevice *device;
    IAudioClient *audio_client;
    IAudioRenderClient *render_client;
};

static_assert(sizeof(Audio_Win32_State) <= sizeof(Audio_Mixer::platform_data), "Audio_Win32_State is bigger than expected.");

static
GUID win32_make_guid(u32 data1, u16 data2, u16 data3, u64 data4) {
    GUID result;
    result.Data1 = data1;
    result.Data2 = data2;
    result.Data3 = data3;
    memcpy(&result.Data4[0], &data4, 8);
    return result;
}

static
Error_Code win32_create_audio_client(Audio_Win32_State *win32) {
    HRESULT result;

    result = OleInitialize(null);
    if(result != S_OK) {
        set_custom_error_message("Failed to initialize the Ole library (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    IID device_enumerator_iid = win32_make_guid(0xA95664D2, 0x9614, 0x4F35, 0xE61736B68DDE46A7);
    CLSID device_enumerator_clsid = win32_make_guid(0xBCDE0395, 0xE52F, 0x467C, 0x2E69919257C43D8E);

    IMMDeviceEnumerator *device_enumerator;
    result = CoCreateInstance(device_enumerator_clsid, null, CLSCTX_ALL, device_enumerator_iid, (void **) &device_enumerator);
    if(result != S_OK) {
        set_custom_error_message("Failed to initialize the Com library (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    defer { device_enumerator->Release(); };

    result = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &win32->device);
    if(result != S_OK) {
        set_custom_error_message("Failed to get the default audio endpoint (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    IID audio_client_iid = win32_make_guid(0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB203A768F5C278B1);
    result = win32->device->Activate(audio_client_iid, CLSCTX_ALL, null, (void **) &win32->audio_client);
    if(result != S_OK) {
        set_custom_error_message("Failed to activate the audio client (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    GUID floating_point_subformat_guid = win32_make_guid(0x3, 0x0, 0x10, 0x719B3800AA000080);
    WAVEFORMATEX waveformat;
    waveformat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    waveformat.nChannels       = AUDIO_CHANNELS;
    waveformat.nSamplesPerSec  = AUDIO_SAMPLE_RATE;
    waveformat.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * sizeof(f32);
    waveformat.nBlockAlign     = AUDIO_CHANNELS * sizeof(f32);
    waveformat.wBitsPerSample  = sizeof(f32) * 8;
    waveformat.cbSize          = 0;

    u32 buffer_duration = 10000000 * 2 / AUDIO_UPDATES_PER_SECOND;  // Buffer duration in hundreds of nanoseconds. We take two times the "expected" buffer size here to better handle unexpected lags.
    
    result = win32->audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, // Share this device with other applications
                                             AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, // Convert between our wave format and the one used by the device, with highish quality
                                             buffer_duration,
                                             0, // Periodicity, can only be non-zero in exclusive applications
                                             &waveformat, // The audio format description
                                             null); // Audio session guid
    if(result != S_OK) {
        set_custom_error_message("Failed to initialize the audio client (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;        
    }
    
    return Success;
}

static
Error_Code win32_create_audio_mixer(Audio_Mixer *mixer) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) mixer->platform_data;
    win32->device        = null;
    win32->audio_client  = null;
    win32->render_client = null;
    
    Error_Code error = win32_create_audio_client(win32);
    if(error != Success) return error;

    IID audio_render_client_iid = win32_make_guid(0xF294ACFC, 0x3146, 0x4483, 0xE260C2A7DCADBFA7);

    HRESULT result;
    result = win32->audio_client->GetService(audio_render_client_iid, (void **) &win32->render_client);
    if(result != S_OK) {
        set_custom_error_message("Failed to get the audio render client server (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    result = win32->audio_client->Start();
    if(result != S_OK) {
        set_custom_error_message("Failed to start the audio client (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    return Success;
}

static
void win32_destroy_audio_mixer(Audio_Mixer *mixer) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) mixer->platform_data;

    if(win32->audio_client)  win32->audio_client->Stop();
    if(win32->render_client) win32->render_client->Release();
    if(win32->audio_client)  win32->audio_client->Release();
    if(win32->device)        win32->device->Release();

    CoUninitialize(); // If we have multiple ole objects, this may cause trouble. Therefore there should be a better way to handle this probably, but I do not currently know how.

    win32->audio_client  = null;
    win32->render_client = null;
    win32->device        = null;

    mixer->sources.clear();
}

static
f32 *win32_acquire_audio_buffer(Audio_Mixer *mixer, u32 *frames_to_output) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) mixer->platform_data;
    if(!win32->audio_client || !win32->render_client) {
        *frames_to_output = 0;
        return null;
    }

    f32 *buffer = null;

    HRESULT result;
    
    UINT32 padding; // The amount of frames that are still queued in the buffer from the previous update
    UINT32 size_in_frames; // The maximum size in frames of the endpoint buffer

    result = win32->audio_client->GetCurrentPadding(&padding);

    if(result != S_OK) {
        // GetCurrentPadding failed!
        *frames_to_output = 0;
        return null;
    }

    result = win32->audio_client->GetBufferSize(&size_in_frames);

    if(result != S_OK) {
        // GetCurrentPadding failed!
        *frames_to_output = 0;
        return null;
    }

    if(size_in_frames >= padding + AUDIO_SAMPLES_PER_UPDATE) {
        *frames_to_output = size_in_frames - padding;

        result = win32->render_client->GetBuffer(*frames_to_output, (BYTE **) &buffer);

        if(result != S_OK) {
            // GetBuffer failed!
            *frames_to_output = 0;
            return null;
        }

        u64 buffer_size_in_bytes = (u64) *frames_to_output * AUDIO_CHANNELS * sizeof(f32);
        memset(buffer, 0, buffer_size_in_bytes); // Non-zero values left in the buffer would lead to noise, since we only ever add samples onto this buffer. The buffer might still contain samples from the previous update.
    } else {
        *frames_to_output = 0;    
    }

    return buffer;
}

static
void win32_release_audio_buffer(Audio_Mixer *mixer, u32 frames_to_output) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) mixer->platform_data;
    win32->render_client->ReleaseBuffer(frames_to_output, 0);
}

#endif



/* ------------------------------------------------- Helpers ------------------------------------------------- */

static
u64 get_size_in_bytes_per_frame(Audio_Buffer_Format format, u8 channels) {
    u64 size_in_bytes;

    switch(format) {
    case AUDIO_BUFFER_FORMAT_Pcm16: size_in_bytes = sizeof(s16); break;
    case AUDIO_BUFFER_FORMAT_Float32: size_in_bytes = sizeof(f32); break;
    }
    
    return size_in_bytes * channels;
}

static
f32 mix_audio_samples(f32 lhs, f32 rhs_raw, f32 rhs_volume) {
    return tanhf(lhs + rhs_raw * rhs_volume);
}

static
f32 query_audio_buffer(Audio_Buffer *buffer, u64 frame, u64 channel) {
    f32 result;

    u64 sample = frame * buffer->channels + min(channel, buffer->channels);
    
    switch(buffer->format) {
    case AUDIO_BUFFER_FORMAT_Pcm16: {
        s16 *ptr = (s16 *) buffer->data;
        result = -(f32) ((f32) ptr[sample] / (f32) MIN_S16);
    } break;
        
    case AUDIO_BUFFER_FORMAT_Float32: {
        f32 *ptr = (f32 *) buffer->data;
        result = ptr[sample];
    } break;
    }
    
    return result;
}



/* ----------------------------------------------- Audio Buffer ----------------------------------------------- */
//
// The WAV file header as described in:
// http://soundfile.sapp.org/doc/WaveFormat/
// This struct just gets layed on the file content, to parse and ensure validity of the header.
// This file format describes all values in big endian.
//

#pragma pack(push, 1)
struct Wav_File_Subchunk {
    u8 id[4]; // An ascii string of four characters, identifying the type of chunk
    u32 size; // The number of bytes of data in this chunk, excluding the id and size members.
};

struct Wav_File_Header {
    u8 chunk_id[4]; // Expected to be "RIFF".
    u32 chunk_size;
    u8 format[4]; // Expected to be "WAVE".
    Wav_File_Subchunk subchunk_1; // Expected to be the "fmt " chunk.
    u16 audio_format;
    u16 channel_count;
    u32 sample_rate;
    u32 byte_rate;
    u16 block_align;
    u16 bits_per_sample;
    Wav_File_Subchunk subchunk_2;
};
#pragma pack(pop, 1)

static
Error_Code ensure_valid_audio_buffer_format(Audio_Buffer *buffer) {
    Error_Code result;
    
    if(buffer->sample_rate != AUDIO_SAMPLE_RATE) {
        result = ERROR_AUDIO_Invalid_Sample_Rate;
    } else if(buffer->channels <= 0 || buffer->channels > 2) {
        result = ERROR_AUDIO_Invalid_Channel_Count;
    }
    
    return result;
}

static
u8 *convert_pcm_to_floating_point(s16 *input, u64 input_size_in_bytes, u64 *output_size_in_bytes, Audio_Buffer_Format *format, Allocator *allocator) {
    s64 sample_count = input_size_in_bytes / sizeof(s16);
    *output_size_in_bytes = sample_count * sizeof(f32);

    f32 *output = (f32 *) allocator->allocate(*output_size_in_bytes);

    for(s64 i = 0; i < sample_count; ++i) {
        output[i] = -((f32) input[i] / (f32) MIN_S16);
    }

    *format = AUDIO_BUFFER_FORMAT_Float32;
    
    return (u8 *) output;
}

static
b8 chunk_name_equals(u8 chunk_name[4], const char *expected_name) {
    return strncmp((char *) chunk_name, expected_name, 4) == 0;
}

static
b8 pointer_outside_of_file(string file_content, u8 *pointer, u64 size) {
    return pointer - file_content.data < 0 || (s64) (pointer - file_content.data + size) > file_content.count;
}

Error_Code create_audio_buffer_from_wav_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator) {
    buffer->name = copy_string(allocator, buffer_name);
    buffer->__drflac_cleanup = false;

    Wav_File_Header *header = (Wav_File_Header *) file_content.data;

    if(pointer_outside_of_file(file_content, (u8 *) header, sizeof(Wav_File_Header))) {
        return ERROR_File_Too_Small;
    }
    
    if(!chunk_name_equals(header->chunk_id, "RIFF") || !chunk_name_equals(header->format, "WAVE") || !chunk_name_equals(header->subchunk_1.id, "fmt ")) {
        return ERROR_AUDIO_Invalid_Wav_File;
    }

    Error_Code result = Success;
    
    if(header->audio_format == 0x1) {
        //
        // Default PCM file format. No additional data present, just the data subchunk with the raw samples.
        //
        if(!chunk_name_equals(header->subchunk_2.id, "data")) {
            result = ERROR_AUDIO_Invalid_Wav_File;
            goto _return;
        }

        if(header->bits_per_sample != 16) {
            result = ERROR_AUDIO_Invalid_Bits_Per_Sample;
            goto _return;
        }

        buffer->channels    = (u8) header->channel_count;
        buffer->sample_rate = header->sample_rate;
        buffer->frame_count = header->subchunk_2.size / (header->bits_per_sample / 8) / header->channel_count;
        buffer->data        = convert_pcm_to_floating_point((s16 *) (file_content.data + sizeof(Wav_File_Header)), header->subchunk_2.size, &buffer->size_in_bytes, &buffer->format, allocator);
    } else if(header->audio_format == 0x3) {
        //
        // Floating point file format. The file has an additional "fact" chunk to describe the used floating point
        // format, followed by a PEAK chunk containing more information, and then the actual data sub chunk.
        // I'm currently not sure whether the PEAK chunk is always present or if we also need to handle the
        // case where subchunk_3 is already the data chunk...
        //       
        Wav_File_Subchunk *subchunk_3 = (Wav_File_Subchunk *) (((u8 *) &header->subchunk_2) + header->subchunk_2.size + 8);

        if(pointer_outside_of_file(file_content, (u8 *) subchunk_3, sizeof(Wav_File_Subchunk))) {
            result = ERROR_File_Too_Small;
            goto _return;
        }

        Wav_File_Subchunk *subchunk_4 = (Wav_File_Subchunk *) (((u8 *) subchunk_3) + subchunk_3->size + 8);

        if(pointer_outside_of_file(file_content, (u8 *) subchunk_4, sizeof(Wav_File_Subchunk))) {
            result = ERROR_File_Too_Small;
            goto _return;
        }

        if(!chunk_name_equals(subchunk_3->id, "PEAK") || !chunk_name_equals(subchunk_4->id, "data")) {
            result = ERROR_AUDIO_Invalid_Wav_File;
            goto _return;
        }

        if(header->bits_per_sample != 32) {
            result = ERROR_AUDIO_Invalid_Bits_Per_Sample;
            goto _return;
        }

        u8 *data = (u8 *) allocator->allocate(subchunk_4->size);
        memcpy(data, subchunk_4 + 8, subchunk_4->size);

        buffer->format        = AUDIO_BUFFER_FORMAT_Float32;
        buffer->channels      = (u8) header->channel_count;
        buffer->sample_rate   = header->sample_rate;
        buffer->frame_count   = subchunk_4->size / (header->bits_per_sample / 8) / buffer->channels;
        buffer->data          = data;
        buffer->size_in_bytes = subchunk_4->size;
    } else {
        result = ERROR_AUDIO_Invalid_Wav_File;
        goto _return;
    }
    
    result = ensure_valid_audio_buffer_format(buffer);

 _return:

    return result;
}

Error_Code create_audio_buffer_from_wav_file(Audio_Buffer *buffer, string file_path, Allocator *allocator) {
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        return ERROR_File_Not_Found;
    }

    Error_Code result = create_audio_buffer_from_wav_memory(buffer, file_content, file_path, allocator);

    os_free_file_content(Default_Allocator, &file_content);
    
    return result;
}

Error_Code create_audio_buffer_from_flac_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator) {
    buffer->name = copy_string(allocator, buffer_name);
    buffer->__drflac_cleanup = true;

    buffer->data = (u8 *) drflac_open_memory_and_read_pcm_frames_f32(file_content.data, file_content.count, (unsigned int *) &buffer->channels, (unsigned int *) &buffer->sample_rate, &buffer->frame_count, null);

    if(!buffer->data) {
        return set_custom_error_message("Failed to read FLAC file.");
    }

    buffer->format = AUDIO_BUFFER_FORMAT_Float32;
    buffer->size_in_bytes = buffer->frame_count * buffer->channels * sizeof(f32);
    
    return Success;
}

Error_Code create_audio_buffer_from_flac_file(Audio_Buffer *buffer, string file_path, Allocator *allocator) {
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        return ERROR_File_Not_Found;
    }

    Error_Code result = create_audio_buffer_from_flac_memory(buffer, file_content, file_path, allocator);

    os_free_file_content(Default_Allocator, &file_content);
    
    return result;
}

void create_audio_buffer(Audio_Buffer *buffer, Audio_Buffer_Format format, u8 channels, u32 sample_rate, u64 frame_count, string buffer_name, Allocator *allocator) {
    buffer->name             = buffer_name;
    buffer->format           = format;
    buffer->channels         = channels;
    buffer->sample_rate      = sample_rate;
    buffer->frame_count      = 0;
    buffer->size_in_bytes    = frame_count * get_size_in_bytes_per_frame(buffer->format, buffer->channels);
    buffer->__drflac_cleanup = false;
    buffer->data = (u8 *) allocator->allocate(buffer->size_in_bytes);
}

void destroy_audio_buffer(Audio_Buffer *buffer, Allocator *allocator) {
    if(buffer->__drflac_cleanup) {
        drflac_free(buffer->data, null);
    } else {
        allocator->deallocate(buffer->data);
    }

    deallocate_string(allocator, &buffer->name);
    
    buffer->format           = AUDIO_BUFFER_FORMAT_Unknown;
    buffer->channels         = 0;
    buffer->sample_rate      = 0;
    buffer->frame_count      = 0;
    buffer->size_in_bytes    = 0;
    buffer->__drflac_cleanup = false;
    buffer->data             = null;
}



/* ----------------------------------------------- Audio Source ----------------------------------------------- */

static inline
f32 normalize_angle(f32 angle) {
    angle = fmodf(angle, FTAU);
    if(angle < 0) angle += FTAU;
    return angle;
}

static inline
f32 calculate_source_gain(Audio_Mixer *mixer, Audio_Source *source) {
    if(source->volume_type != AUDIO_VOLUME_Master) {
        return mixer->volumes[AUDIO_VOLUME_Master] * mixer->volumes[source->volume_type] * source->volume;
    } else {
        return mixer->volumes[AUDIO_VOLUME_Master] * source->volume;
    }

}

static
Audio_Arc *find_arc_for_panning(Audio_Listener *listener, f32 panning_angle) {
    for(Audio_Arc &arc : listener->arcs) {
        f32 delta = normalize_angle(panning_angle - arc.left_angle);
        if(delta <= arc.covered_angle) return &arc;
    }

    return null;
}

static inline
f32 calculate_blending_percentage_for_arc(Audio_Arc *arc, f32 panning_angle) {
    return normalize_angle(panning_angle - arc->left_angle) / arc->covered_angle;
}

static
void calculate_source_channel_pans(Audio_Listener *listener, Audio_Source *source, f32 *channels) {
    f32 dx = source->x - listener->x, dy = source->y - listener->y, dz = source->z - listener->z;

    f32 distance = sqrtf(dx * dx + dy * dy + dz * dz);
    f32 distance_attenuation = clamp(1.f / (distance - source->falloff_start), 0.f, 1.f);

    f32 px = dx - listener->ux * dx, py = dy - listener->uy * dy, pz = dz - listener->uz * dz; // Project the delta vector onto the horizontal listener plane.

    f32 u = px * listener->fx + py * listener->fy + pz * listener->fz;
    f32 v = px * listener->rx + py * listener->ry + pz * listener->rz;

    f32 denom = 1.f / sqrtf(u * u + v * v); // Normalize the horizontal direction vector
    u *= denom;
    v *= denom;

    f32 panning_angle = atan2f(v, u); // Put the sound angle in the range [0;2PI]

    Audio_Arc *arc = find_arc_for_panning(listener, panning_angle);
    if(!arc) return; // This should never happen

    f32 beta = calculate_blending_percentage_for_arc(arc, panning_angle);
    channels[arc->left_channel]  *= sinf(FPI / 2.f * beta);
    channels[arc->right_channel] *= cosf(FPI / 2.f * beta);
}

Audio_Source *acquire_audio_source(Audio_Mixer *mixer, Audio_Volume_Type type, b8 spatialized) {
    Audio_Source *source           = mixer->sources.push();
    source->state                  = AUDIO_SOURCE_Paused;
    source->volume_type            = type;
    source->remove_on_completion   = false;
    source->playing_buffer         = null;
    source->frame_offset_in_buffer = 0;
    source->volume                 = 1.f;
    source->loop                   = false;
    source->spatialized            = spatialized;
    source->x                      = 0;
    source->y                      = 0;
    source->z                      = 0;
    source->falloff_start          = 10.f;
    return source;
}

void release_audio_source(Audio_Mixer *mixer, Audio_Source *source) {
    mixer->sources.remove_value_pointer(source);
}

void stop_audio_source(Audio_Source *source) {
    source->state = AUDIO_SOURCE_Completed;
    source->playing_buffer = null;
    source->frame_offset_in_buffer = 0;
}

void pause_audio_source(Audio_Source *source) {
    source->state = AUDIO_SOURCE_Paused;
}

void resume_audio_source(Audio_Source *source) {
    if(source->playing_buffer != null) source->state = AUDIO_SOURCE_Playing;
}

void set_audio_source_looping(Audio_Source *source, b8 looping) {
    source->loop = looping;
}

void play_audio_buffer(Audio_Mixer *mixer, Audio_Buffer *buffer, Audio_Volume_Type type, b8 spatialized) {
    Audio_Source *source = acquire_audio_source(mixer, type, spatialized);
    source->remove_on_completion = true;
    play_audio_buffer(source, buffer);
}

void play_audio_buffer(Audio_Source *source, Audio_Buffer *buffer) {
    source->playing_buffer = buffer;
    source->frame_offset_in_buffer = 0;
    source->state = AUDIO_SOURCE_Playing;
}

void set_audio_source_transformation(Audio_Source *source, f32 x, f32 y, f32 z, f32 falloff_start) {
    source->x = x;
    source->y = y;
    source->z = z;
    source->falloff_start = falloff_start;
}



/* ----------------------------------------------- Audio Stream ----------------------------------------------- */

static
void update_audio_stream(Audio_Stream *stream, u32 frames_to_output) {
    u64 frame_size_in_bytes = get_size_in_bytes_per_frame(stream->buffer.format, stream->buffer.channels);

    // Move all the frames in the buffer that have not yet been played to the front, so that the source
    // will play them next.
    u64 still_valid_frames = stream->buffer.frame_count - stream->source->frame_offset_in_buffer;
    memmove(&stream->buffer.data[0], &stream->buffer.data[still_valid_frames], still_valid_frames * frame_size_in_bytes);

    // Calculate the number of frames to generate this update to fill the audio buffer.
    u64 requested_frames   = frames_to_output - still_valid_frames;
    u64 storable_frames    = stream->buffer.size_in_bytes / frame_size_in_bytes;
    u64 frames_to_generate = min(requested_frames, storable_frames);

    // Generate the new frames and copy them into the audio buffer
    f32 *frames = stream->callback(stream->user_pointer, frames_to_generate);
    memcpy(&stream->buffer.data[still_valid_frames], frames, frames_to_generate * frame_size_in_bytes);
    stream->buffer.frame_count = still_valid_frames + frames_to_generate;

    // Reset the audio source to continue playing from this buffer.
    stream->frames_played_last_update      = stream->source->frame_offset_in_buffer;
    stream->source->frame_offset_in_buffer = 0;
    stream->source->playing_buffer         = &stream->buffer;
    stream->source->state                  = AUDIO_SOURCE_Playing;
}

Audio_Stream *create_audio_stream(Audio_Mixer *mixer, void *user_pointer, Audio_Stream_Callback callback, Audio_Volume_Type type, b8 spatialized, string buffer_name, Allocator *allocator) {
    Audio_Stream *stream = mixer->streams.push();
    stream->user_pointer = user_pointer;
    stream->callback = callback;
    stream->source = acquire_audio_source(mixer, type, spatialized);
    create_audio_buffer(&stream->buffer, AUDIO_BUFFER_FORMAT_Float32, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE, AUDIO_SAMPLES_PER_UPDATE * 4, buffer_name, allocator);
    return stream;
}

void destroy_audio_stream(Audio_Mixer *mixer, Audio_Stream *stream, Allocator *allocator) {
    release_audio_source(mixer, stream->source);
    destroy_audio_buffer(&stream->buffer, allocator);
    mixer->streams.remove_value_pointer(stream);
}

void pause_audio_stream(Audio_Stream *stream) {
    stream->source->state = AUDIO_SOURCE_Paused;
}

void resume_audio_stream(Audio_Stream *stream) {
    stream->source->state = AUDIO_SOURCE_Playing;
}



/* ----------------------------------------------- Audio Mixer ----------------------------------------------- */

static
void setup_arc(Audio_Listener *listener, s8 arc_index, s8 left_channel, s8 right_channel, f32 left_angle, f32 right_angle) {
    assert(arc_index >= 0 && arc_index < ARRAY_COUNT(listener->arcs));
    listener->arcs[arc_index].left_channel  = left_channel;
    listener->arcs[arc_index].right_channel = right_channel;
    listener->arcs[arc_index].left_angle    = degrees_to_radians(left_angle);
    listener->arcs[arc_index].right_angle   = degrees_to_radians(right_angle);
    listener->arcs[arc_index].covered_angle = normalize_angle(degrees_to_radians(right_angle - left_angle));
}

Error_Code create_audio_mixer(Audio_Mixer *mixer) {
    // Set up the base volumes.
    for(s64 i = 0; i < AUDIO_VOLUME_COUNT; ++i)
        mixer->volumes[i] = 1.f;

    // Set up the listener.
    mixer->listener.x  = 0;
    mixer->listener.y  = 0;
    mixer->listener.z  = 0;
    mixer->listener.fx = 0;
    mixer->listener.fy = 0;
    mixer->listener.fz = -1;
    mixer->listener.ux = 0;
    mixer->listener.uy = 1;
    mixer->listener.uz = 0;
    mixer->listener.rx = 1;
    mixer->listener.ry = 0;
    mixer->listener.rz = 0;

#if AUDIO_CHANNELS == 2
    // One arc covers the front-side of the listening circle, the other arc covers the back-side of the circle.
    const b8 headphones = true;
    const f32 angle = headphones ? 90 : 45;    
    setup_arc(&mixer->listener, 0, LEFT_AUDIO_CHANNEL, RIGHT_AUDIO_CHANNEL, -angle, +angle);
    setup_arc(&mixer->listener, 1, RIGHT_AUDIO_CHANNEL, LEFT_AUDIO_CHANNEL, +angle, -angle);
#endif
    
#if FOUNDATION_WIN32
    return win32_create_audio_mixer(mixer);
#endif
    
    return Success;
}

void destroy_audio_mixer(Audio_Mixer *mixer) {
#if FOUNDATION_WIN32
    win32_destroy_audio_mixer(mixer);
#endif
}

void update_audio_mixer(Audio_Mixer *mixer) {
    //
    // Get the OS's internal sound buffer into which we will update.
    //
    u32 frames_to_output;
    f32 *output;

#if FOUNDATION_WIN32
    output = win32_acquire_audio_buffer(mixer, &frames_to_output);
#endif

    //
    // Update all audio streams.
    //
    for(Audio_Stream &stream : mixer->streams) {
        if(stream.source->state != AUDIO_SOURCE_Paused) update_audio_stream(&stream, frames_to_output);
    }
    
    //
    // Fill the platform's sound buffer for this update.
    //
    for(auto it = mixer->sources.begin(); it != mixer->sources.end(); ) {
        Audio_Source &source = it.pointer->data;
        
        if(source.state == AUDIO_SOURCE_Playing) {
            assert(source.playing_buffer != null, "Playing audio source does not have a buffer attached.");

            u32 source_frames_to_output;
            if(source.loop) {
                source_frames_to_output = frames_to_output;
            } else {
                source_frames_to_output = min(frames_to_output, (u32) (source.playing_buffer->frame_count - source.frame_offset_in_buffer));
            }

            f32 gain = calculate_source_gain(mixer, &source);
            f32 pan[AUDIO_CHANNELS];
            for(s64 i = 0; i < AUDIO_CHANNELS; ++i) pan[i] = gain;

            if(source.spatialized && ARRAY_COUNT(mixer->listener.arcs) > 1) {
                calculate_source_channel_pans(&mixer->listener, &source, pan);
            }
           
            for(u32 i = 0; i < source_frames_to_output; ++i) {
                for(u8 j = 0; j < AUDIO_CHANNELS; ++j) {
                    u64 output_offset = (u64) i * AUDIO_CHANNELS + j;
                    f32 output_sample = output[output_offset];
                    f32 source_sample = query_audio_buffer(source.playing_buffer, (source.frame_offset_in_buffer + i) % source.playing_buffer->frame_count, j);
                    output[output_offset] = mix_audio_samples(output_sample, source_sample, pan[j]);
                }                
            }

            if(source.loop) {
                source.frame_offset_in_buffer = (source.frame_offset_in_buffer + source_frames_to_output) % source.playing_buffer->frame_count;
            } else {
                source.frame_offset_in_buffer += source_frames_to_output;
                if(source.frame_offset_in_buffer == source.playing_buffer->frame_count) source.state = AUDIO_SOURCE_Completed;
            }
        }

        if(source.state == AUDIO_SOURCE_Completed && source.remove_on_completion) {
            //
            // Remove this source from the linked list.
            //
            auto node_to_remove = it.pointer;
            ++it;
            mixer->sources.remove_node(node_to_remove);            
        } else {        
            //
            // Go to the next audio source.
            //
            ++it;
        }
    }

#if FOUNDATION_WIN32
    win32_release_audio_buffer(mixer, frames_to_output);
#endif
}

void update_audio_mixer_with_silence(Audio_Mixer *mixer) {
    u32 frames_to_output;

#if FOUNDATION_WIN32
    win32_acquire_audio_buffer(mixer, &frames_to_output);
    win32_release_audio_buffer(mixer, frames_to_output);
#endif
}

void update_audio_listener(Audio_Mixer *mixer, f32 x, f32 y, f32 z, f32 fx, f32 fy, f32 fz, f32 ux, f32 uy, f32 uz, f32 rx, f32 ry, f32 rz) {
    mixer->listener.x  = x;
    mixer->listener.y  = y;
    mixer->listener.z  = z;
    mixer->listener.fx = fx;
    mixer->listener.fy = fy;
    mixer->listener.fz = fz;
    mixer->listener.ux = ux;
    mixer->listener.uy = uy;
    mixer->listener.uz = uz;
    mixer->listener.rx = rx;
    mixer->listener.ry = ry;
    mixer->listener.rz = rz;
}
