#include "audio.h"
#include "os_specific.h"

#if FOUNDATION_WIN32
# include <mmdeviceapi.h>
# include <audioclient.h>

struct Audio_Win32_State {
    IMMDevice *device;
    IAudioClient *audio_client;
    IAudioRenderClient *render_client;
};

static_assert(sizeof(Audio_Win32_State) <= sizeof(Audio_Player::platform_data), "Audio_Win32_State is bigger than expected.");

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

    IID device_enumerator_iid = win32_make_guid(0xA95664D2, 0x9614, 0x4F35, 0xA746DE8DB63617E6);
    CLSID device_enumerator_clsid = win32_make_guid(0xBCDE0395, 0xE52F, 0x467C, 0x8E3DC4579291692E);

    IMMDeviceEnumerator *device_enumerator;
    result = CoCreateInstance(device_enumerator_iid, null, CLSCTX_ALL, device_enumerator_iid, (void **) &device_enumerator);
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

    IID audio_client_iid = win32_make_guid(0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB178C2F568A703B2);
    result = win32->device->Activate(audio_client_iid, CLSCTX_ALL, null, (void **) &win32->audio_client);
    if(result != S_OK) {
        set_custom_error_message("Failed to activate the audio client (%s).", win32_hresult_to_string(result));
        return ERROR_Custom_Error_Message;
    }

    GUID floating_point_subformat_guid = win32_make_guid(0x3, 0x0, 0x10, 0x8000AA0389B71);
    WAVEFORMATEX waveformat;
    waveformat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    waveformat.nChannels       = AUDIO_CHANNELS;
    waveformat.nSamplesPerSec  = AUDIO_SAMPLE_RATE;
    waveformat.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * sizeof(f32);
    waveformat.nBlockAlign     = AUDIO_CHANNELS * sizeof(f32);
    waveformat.wBitsPerSample  = sizeof(f32) * 8;
    waveformat.cbSize          = 0;

    result = win32->audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, // Share this device with other applications
                                             AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, // Convert between our wave format and the one used by the device, with highish quality
                                             AUDIO_NANOSECONDS_PER_UPDATE, // Buffer duration in hundreds of nanoseconds
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
Error_Code win32_create_audio_player(Audio_Player *player) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) player->platform_data;
    
    Error_Code error = win32_create_audio_client(win32);
    if(error != Success) return error;

    IID audio_render_client_iid = win32_make_guid(0xF294ACFC, 0x3146, 0x4483, 0xA7BFADDCA7C260E2);

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
void win32_destroy_audio_player(Audio_Player *player) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) player->platform_data;

    if(win32->audio_client)  win32->audio_client->Stop();
    if(win32->render_client) win32->render_client->Release();
    if(win32->audio_client)  win32->audio_client->Release();
    if(win32->device)        win32->device->Release();

    win32->audio_client  = null;
    win32->render_client = null;
    win32->device        = null;

    player->sources.clear();
}

static
f32 *win32_acquire_audio_buffer(Audio_Player *player, u32 *frames_to_output) {
    Audio_Win32_State *win32 = (Audio_Win32_State *) player->platform_data;

    f32 *buffer = null;

    HRESULT result;
    
    UINT32 padding; // The amount of frames that are still queued in the buffer from the previous update

    result = win32->audio_client->GetCurrentPadding(&padding);

    if(result != S_OK) {
        // GetCurrentPadding failed!
        *frames_to_output = 0;
        return null;
    }

    if(AUDIO_SAMPLES_PER_UPDATE > padding) {
        *frames_to_output = AUDIO_SAMPLES_PER_UPDATE - padding;

        result = win32->render_client->GetBuffer(*frames_to_output, (BYTE **) &buffer);

        if(result != S_OK) {
            // GetBuffer failed!
            *frames_to_output = 0;
            return null;
        }

        s64 buffer_size_in_bytes = *frames_to_output * AUDIO_CHANNELS * sizeof(f32);
        memset(buffer, 0, buffer_size_in_bytes); // Non-zero values left in the buffer would lead to noise, since we only ever add samples onto this buffer. The buffer might still contain samples from the previous update.
    } else {
        *frames_to_output = 0;    
    }

    return buffer;
}

#endif


/* ------------------------------------------------- Helpers ------------------------------------------------- */

static
f32 calculate_audio_source_volume(Audio_Player *player, Audio_Source *source) {
    if(source->volume_type != AUDIO_VOLUME_Master) {
        return player->volumes[AUDIO_VOLUME_Master] * player->volumes[source->volume_type] * source->volume;
    } else {
        return player->volumes[AUDIO_VOLUME_Master] * source->volume;
    }

}

static
f32 mix_audio_samples(f32 lhs, f32 rhs_raw, f32 rhs_volume) {
    return tanhf(lhs + rhs_raw * rhs_volume);
}

static
f32 query_audio_buffer(Audio_Buffer *buffer, u64 sample) {
    f32 result;

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



/* ----------------------------------------------- Audio Player ----------------------------------------------- */

Error_Code create_audio_player(Audio_Player *player) {
#if FOUNDATION_WIN32
    return win32_create_audio_player(player);
#endif

    return Success;
}

void destroy_audio_player(Audio_Player *player) {
#if FOUNDATION_WIN32
    win32_destroy_audio_player(player);
#endif
}

void update_audio_player(Audio_Player *player) {
    u32 frames_to_output;
    f32 *output;

#if FOUNDATION_WIN32
    output = win32_acquire_audio_buffer(player, &frames_to_output);
#endif

    for(auto it = player->sources.begin(); it != player->sources.end(); ) {
        Audio_Source &source = it.pointer->data;
        
        if(source.state == AUDIO_SOURCE_Playing) {
            assert(source.playing_buffer != null, "Playing audio source does not have a buffer attached.");

            u32 source_frames_to_output = min(frames_to_output, (u32) (source.playing_buffer->frame_count - source.frame_offset_in_buffer));
            f32 source_volume = calculate_audio_source_volume(player, &source);

            for(u32 i = 0; i < source_frames_to_output; ++i) {
                for(u8 j = 0; j < AUDIO_CHANNELS; ++j) {
                    u64 source_buffer_offset = (source.frame_offset_in_buffer + i) * source.playing_buffer->channels + j;
                    u64 output_offset = i * AUDIO_CHANNELS + j;
                    f32 output_sample = output[output_offset];
                    f32 source_sample = query_audio_buffer(source.playing_buffer, source_buffer_offset);
                    output[output_offset] = mix_audio_samples(output_sample, source_sample, source_volume);
                }                
            }

            source.frame_offset_in_buffer += source_frames_to_output;
            if(source.frame_offset_in_buffer == source.playing_buffer->frame_count) source.state = AUDIO_SOURCE_Completed;
        }

        if(source.state == AUDIO_SOURCE_Completed && !source.remove_on_completion) {
            //
            // Remove this source from the linked list.
            //
            auto node_to_remove = it.pointer;
            ++it;
            player->sources.remove_node(node_to_remove);            
        } else {        
            if(source.state == AUDIO_SOURCE_Completed && source.loop) {
                //
                // Restart the current audio source.
                // @Incomplete: Should we not maybe handle this in the above loop while iterating over the
                // samples? This way, there's a gap whenever the source finishes in an update, because it
                // has to wait until the next update to actually start playing again...
                //
                source.frame_offset_in_buffer = 0;
                source.state = AUDIO_SOURCE_Playing;
            }

            //
            // Go to the next audio source.
            //
            ++it;
        }
    }
}



/* ----------------------------------------------- Audio Buffer ----------------------------------------------- */

Error_Code create_audio_buffer_from_wav_memory(Audio_Buffer *buffer, string file_content, string buffer_name) {
    // @Incomplete
    return Success;
}

Error_Code create_audio_buffer_from_wav_file(Audio_Buffer *buffer, string file_path) {
    // @Incomplete
    return Success;
}

Error_Code create_audio_buffer_from_flac_memory(Audio_Buffer *buffer, string file_content, string buffer_name) {
    // @Incomplete
    return Success;
}

Error_Code create_audio_buffer_from_flac_file(Audio_Buffer *buffer, string file_path) {
    // @Incomplete
    return Success;
}

void destroy_audio_buffer(Audio_Buffer *buffer) {
    // @Incomplete

}



/* ----------------------------------------------- Audio Source ----------------------------------------------- */

Audio_Source *acquire_audio_source(Audio_Player *player) {
    // @Incomplete
    return null;
}

void release_audio_source(Audio_Player *player, Audio_Source *source) {
    // @Incomplete

}

void play_audio_buffer(Audio_Player *player, Audio_Buffer *buffer, Audio_Volume_Type type) {
    // @Incomplete

}

void play_audio_buffer(Audio_Source *source, Audio_Buffer *buffer) {
    // @Incomplete

}
