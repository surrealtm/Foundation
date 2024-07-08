#pragma once

#include "foundation.h"
#include "string_type.h"
#include "error.h"
#include "memutils.h"

// @Incomplete: Support streaming in large audio buffers from disk.
// @Cleanup: Handle switching audio engines during play, e.g. when connecting headphones on laptop. Might only
// happen if connecting them while muted, and then unmuting?

#define AUDIO_PLATFORM_STATE_SIZE 32

#ifndef AUDIO_SAMPLE_RATE
# define AUDIO_SAMPLE_RATE        48000
#endif

#ifndef AUDIO_CHANNELS
# define AUDIO_CHANNELS               2
#endif

#ifndef AUDIO_UPDATES_PER_SECOND
# define AUDIO_UPDATES_PER_SECOND    10
# define AUDIO_SAMPLES_PER_UPDATE (AUDIO_SAMPLE_RATE / AUDIO_UPDATES_PER_SECOND)
#endif

enum Audio_Volume_Type {
    AUDIO_VOLUME_Master,
    AUDIO_VOLUME_Interface,
    AUDIO_VOLUME_Music,
    AUDIO_VOLUME_Sound_Effect,
    AUDIO_VOLUME_Ambient,
    AUDIO_VOLUME_COUNT,
};

enum Audio_Source_State {
    AUDIO_SOURCE_Paused    = 0x0,
    AUDIO_SOURCE_Playing   = 0x1,
    AUDIO_SOURCE_Completed = 0x2,
};

enum Audio_Buffer_Format {
    AUDIO_BUFFER_FORMAT_Unknown,
    AUDIO_BUFFER_FORMAT_Pcm16,
    AUDIO_BUFFER_FORMAT_Float32,
};

struct Audio_Buffer {
    string name; // Usually the file path, but can be set manually.

    u8 channels;
    u32 sample_rate;
    Audio_Buffer_Format format;

    u8 *data;
    u64 size_in_bytes;
    
    u64 frame_count; // Valid number of frames in the data buffer

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

typedef f32 *(*Audio_Stream_Callback)(void *user_pointer, u64 requested_frames);

struct Audio_Stream {
    void *user_pointer;
    Audio_Stream_Callback callback;
    Audio_Buffer buffer;
    Audio_Source *source;
    u64 frames_played_last_update;
};

struct Audio_Mixer {
    u8 platform_data[AUDIO_PLATFORM_STATE_SIZE];

    Allocator *allocator;

    Linked_List<Audio_Source> sources;
    Linked_List<Audio_Stream> streams;
    
    f32 volumes[AUDIO_VOLUME_COUNT];
};

Error_Code create_audio_mixer(Audio_Mixer *mixer);
void destroy_audio_mixer(Audio_Mixer *mixer);
void update_audio_mixer(Audio_Mixer *mixer);

// This can be useful to "catch up" the audio system after a long time without any updates,
// for example after loading something from disk or program startup. In that case, the audio
// mixer may have to "catch up" a large number of audio frames, which may not be available
// at the time for seamless play (e.g. when streaming into an audio buffer which is not large
// enough for the time gap). Instead of playing the too-small available part of the audio
// stream, skip this update entirely by just playing silence, and then resume normal playback.
// This avoids bad audio artifacts at the cost of a little latency.
void update_audio_mixer_with_silence(Audio_Mixer *mixer);

Error_Code create_audio_buffer_from_wav_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator);
Error_Code create_audio_buffer_from_wav_file(Audio_Buffer *buffer, string file_path, Allocator *allocator);
Error_Code create_audio_buffer_from_flac_memory(Audio_Buffer *buffer, string file_content, string buffer_name, Allocator *allocator);
Error_Code create_audio_buffer_from_flac_file(Audio_Buffer *buffer, string file_path, Allocator *allocator);
void create_audio_buffer(Audio_Buffer *buffer, Audio_Buffer_Format format, u8 channels, u32 sample_rate, u64 frame_count, string buffer_name, Allocator *allocator);
void destroy_audio_buffer(Audio_Buffer *buffer, Allocator *allocator);

Audio_Source *acquire_audio_source(Audio_Mixer *mixer, Audio_Volume_Type type);
void release_audio_source(Audio_Mixer *mixer, Audio_Source *source);
void pause_audio_source(Audio_Source *source);
void resume_audio_source(Audio_Source *source);
void set_audio_source_options(Audio_Source *source, b8 looping);

Audio_Stream *create_audio_stream(Audio_Mixer *mixer, void *user_pointer, Audio_Stream_Callback callback, Audio_Volume_Type type, string buffer_name, Allocator *allocator);
void destroy_audio_stream(Audio_Mixer *mixer, Audio_Stream *stream, Allocator *allocator);
void pause_audio_stream(Audio_Stream *stream);
void resume_audio_stream(Audio_Stream *stream);

void play_audio_buffer(Audio_Mixer *mixer, Audio_Buffer *buffer, Audio_Volume_Type type);
void play_audio_buffer(Audio_Source *source, Audio_Buffer *buffer);
