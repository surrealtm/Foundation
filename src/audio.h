#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"
#include "memutils.h"

// @Incomplete: Support streaming in large audio buffers from disk.
// @Cleanup: Handle switching audio engines during play, e.g. when connecting headphones on laptop. Might only
// happen if connecting them while muted, and then unmuting?

#define AUDIO_PLATFORM_STATE_SIZE 32

#define AUDIO_SAMPLE_RATE             48000
#define AUDIO_CHANNELS                    2
#define AUDIO_UPDATES_PER_SECOND         15 // @Incomplete: Are other values here also okay?
#define AUDIO_SAMPLES_PER_UPDATE     (AUDIO_SAMPLE_RATE / AUDIO_UPDATES_PER_SECOND)
#define AUDIO_NANOSECONDS_PER_UPDATE (1000000000 / AUDIO_UPDATES_PER_SECOND)

enum Audio_Volume_Type {
    AUDIO_VOLUME_Master       = 0x0,
    AUDIO_VOLUME_Music        = 0x1,
    AUDIO_VOLUME_Sound_Effect = 0x2,
    AUDIO_VOLUME_Ambient      = 0x3,
    AUDIO_VOLUME_COUNT        = 0x4,
};

enum Audio_Source_State {
    AUDIO_SOURCE_Paused    = 0x0,
    AUDIO_SOURCE_Playing   = 0x1,
    AUDIO_SOURCE_Completed = 0x2,
};

enum Audio_Buffer_Format {
    AUDIO_BUFFER_FORMAT_Pcm16,
    AUDIO_BUFFER_FORMAT_Float32,
};

struct Audio_Buffer {
    string name; // Usually the file path, but can be set manually.

    Audio_Buffer_Format format;
    u8 *data;
    
    u8 channels;
    u32 sample_rate;
    u64 frame_count;
    u64 size_in_bytes;

    b8 __drflac_cleanup; // Depending on how an audio buffer was loaded, we may require different cleanups.
};

struct Audio_Source {
    Audio_Source_State state;
    Audio_Volume_Type volume_type;
    b8 remove_on_completion; // Externally managed audio sources will not be automatically destroyed by the audio system.

    f32 volume;
    b8 loop;

    Audio_Buffer *playing_buffer;
    u64 frame_offset_in_buffer;
};

struct Audio_Player {
    u8 platform_data[AUDIO_PLATFORM_STATE_SIZE];

    Linked_List<Audio_Source> sources;
    f32 volumes[AUDIO_VOLUME_COUNT];
};

Error_Code create_audio_player(Audio_Player *player);
void destroy_audio_player(Audio_Player *player);
void update_audio_player(Audio_Player *player);

Error_Code create_audio_buffer_from_wav_memory(Audio_Buffer *buffer, string file_content, string buffer_name);
Error_Code create_audio_buffer_from_wav_file(Audio_Buffer *buffer, string file_path);
Error_Code create_audio_buffer_from_flac_memory(Audio_Buffer *buffer, string file_content, string buffer_name);
Error_Code create_audio_buffer_from_flac_file(Audio_Buffer *buffer, string file_path);
void destroy_audio_buffer(Audio_Buffer *buffer);

Audio_Source *acquire_audio_source(Audio_Player *player);
void release_audio_source(Audio_Player *player, Audio_Source *source);

void play_audio_buffer(Audio_Player *player, Audio_Buffer *buffer, Audio_Volume_Type type);
void play_audio_buffer(Audio_Source *source, Audio_Buffer *buffer);
