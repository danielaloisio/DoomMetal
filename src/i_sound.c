// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <SDL.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

// Needed if SNDSERV is defined in doomdef.h
char* sndserver_filename = "sndserver";

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
#define SAMPLERATE		22050
#define SAMPLECOUNT		512
#define NUM_CHANNELS		8

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;

typedef struct
{
    unsigned char* data;
    int length;
    int position;
    int active;
    int loop;
    int volume;
    int separation;
    int pitch;
    int sfx_id;
} channel_t;

static channel_t channels[NUM_CHANNELS];

// Sound effects data
typedef struct
{
    unsigned char* data;
    int length;
    int samplerate;
} sound_t;

static sound_t sounds[NUMSFX];
static int lengths[NUMSFX];

static int music_volume = 15;

static SDL_mutex* audio_mutex = NULL;

//
// Audio callback - called by SDL to fill the audio buffer
//
static void I_AudioCallback(void* userdata, Uint8* stream, int len)
{
    int i, c;
    Sint16* output = (Sint16*)stream;
    int samples = len / 4;

    memset(stream, 0, len);
    
    if (!audio_mutex) return;
    
    SDL_LockMutex(audio_mutex);

    for (c = 0; c < NUM_CHANNELS; c++)
    {
        channel_t* chan = &channels[c];
        
        if (!chan->active || !chan->data)
            continue;
            
        for (i = 0; i < samples && chan->position < chan->length; i++)
        {
            int sample = chan->data[chan->position];

            Sint16 sample_16 = (Sint16)((sample - 128) << 8);

            sample_16 = (sample_16 * chan->volume) / 127;

            int left_vol = 255 - chan->separation;
            int right_vol = chan->separation;

            int left_sample = output[i * 2] + ((sample_16 * left_vol) / 255);
            int right_sample = output[i * 2 + 1] + ((sample_16 * right_vol) / 255);

            if (left_sample > 32767) left_sample = 32767;
            if (left_sample < -32768) left_sample = -32768;
            if (right_sample > 32767) right_sample = 32767;
            if (right_sample < -32768) right_sample = -32768;
            
            output[i * 2] = (Sint16)left_sample;
            output[i * 2 + 1] = (Sint16)right_sample;
            
            chan->position++;
        }

        if (chan->position >= chan->length)
        {
            if (chan->loop)
            {
                chan->position = 0;
            }
            else
            {
                chan->active = 0;
            }
        }
    }
    
    SDL_UnlockMutex(audio_mutex);
}

//
// Load sound effect from WAD
//
static int I_LoadSound(int sfx_id, char* sfxname)
{
    unsigned char* sfx;
    int size;
    char name[20];
    int sfxlump;
    int samplerate;
    int samples;

    sprintf(name, "ds%s", sfxname);

    if (W_CheckNumForName(name) == -1)
        sfxlump = W_GetNumForName("dspistol");
    else
        sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength(sfxlump);
    sfx = (unsigned char*)W_CacheLumpNum(sfxlump, PU_STATIC);

    if (size < 8 || sfx[0] != 0x03 || sfx[1] != 0x00)
    {
        Z_Free(sfx);
        return 0;
    }

    samplerate = sfx[2] | (sfx[3] << 8);
    samples = sfx[4] | (sfx[5] << 8) | (sfx[6] << 16) | (sfx[7] << 24);

    if (samples > size - 8)
        samples = size - 8;

    sounds[sfx_id].data = (unsigned char*)Z_Malloc(samples, PU_STATIC, 0);
    sounds[sfx_id].length = samples;
    sounds[sfx_id].samplerate = samplerate;

    memcpy(sounds[sfx_id].data, sfx + 8, samples);
    
    Z_Free(sfx);
    
    lengths[sfx_id] = samples;
    
    return 1;
}

//
// Find a free channel
//
static int I_FindFreeChannel(void)
{
    int i;
    
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        if (!channels[i].active)
            return i;
    }
    
    // No free channel, stop the oldest one
    return 0;
}

//
// Initialize sound
//
void I_InitSound(void)
{
    SDL_AudioSpec desired;
    int i;
    
    fprintf(stderr, "I_InitSound: ");

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        return;
    }

    SDL_zero(desired);
    desired.freq = SAMPLERATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = SAMPLECOUNT;
    desired.callback = I_AudioCallback;
    desired.userdata = NULL;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &audio_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    
    if (audio_device == 0)
    {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    
    fprintf(stderr, "configured audio device (freq=%d, channels=%d, samples=%d)\n",
            audio_spec.freq, audio_spec.channels, audio_spec.samples);

    audio_mutex = SDL_CreateMutex();
    if (!audio_mutex)
    {
        fprintf(stderr, "Failed to create audio mutex\n");
        return;
    }

    for (i = 0; i < NUM_CHANNELS; i++)
    {
        memset(&channels[i], 0, sizeof(channel_t));
    }

    for (i = 0; i < NUMSFX; i++)
    {
        memset(&sounds[i], 0, sizeof(sound_t));
    }

    fprintf(stderr, "I_InitSound: ");
    
    for (i = 1; i < NUMSFX; i++)
    {
        if (!S_sfx[i].link)
        {
            I_LoadSound(i, S_sfx[i].name);
        }
        else
        {
            int link_id = S_sfx[i].link - S_sfx;
            sounds[i] = sounds[link_id];
            lengths[i] = lengths[link_id];
        }
    }
    
    fprintf(stderr, "pre-cached all sound data\n");

    SDL_PauseAudioDevice(audio_device, 0);
    
    fprintf(stderr, "I_InitSound: sound module ready\n");
}

//
// Shutdown sound
//
void I_ShutdownSound(void)
{
    int i;
    
    if (audio_device)
    {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    
    if (audio_mutex)
    {
        SDL_DestroyMutex(audio_mutex);
        audio_mutex = NULL;
    }
    
    // Free sound data
    for (i = 0; i < NUMSFX; i++)
    {
        if (sounds[i].data && (!S_sfx[i].link || i == S_sfx[i].link - S_sfx))
        {
            Z_Free(sounds[i].data);
            sounds[i].data = NULL;
        }
    }
    
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

//
// Start a sound effect
//
int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    int channel;
    
    (void)priority;
    (void)pitch;
    
    if (id < 1 || id >= NUMSFX)
    {
        fprintf(stderr, "I_StartSound: invalid sound id %d\n", id);
        return -1;
    }
    
    if (!sounds[id].data)
    {
        fprintf(stderr, "I_StartSound: sound %d not loaded\n", id);
        return -1;
    }
    
    if (!audio_mutex)
        return -1;
    
    SDL_LockMutex(audio_mutex);

    channel = I_FindFreeChannel();

    channels[channel].data = sounds[id].data;
    channels[channel].length = sounds[id].length;
    channels[channel].position = 0;
    channels[channel].active = 1;
    channels[channel].loop = 0;
    channels[channel].volume = vol;
    channels[channel].separation = sep;
    channels[channel].pitch = pitch;
    channels[channel].sfx_id = id;
    
    SDL_UnlockMutex(audio_mutex);
    
    return channel;
}

//
// Stop a sound
//
void I_StopSound(int handle)
{
    if (handle < 0 || handle >= NUM_CHANNELS)
        return;
    
    if (!audio_mutex)
        return;
    
    SDL_LockMutex(audio_mutex);
    channels[handle].active = 0;
    SDL_UnlockMutex(audio_mutex);
}

//
// Check if sound is playing
//
int I_SoundIsPlaying(int handle)
{
    int result = 0;
    
    if (handle < 0 || handle >= NUM_CHANNELS)
        return 0;
    
    if (!audio_mutex)
        return 0;
    
    SDL_LockMutex(audio_mutex);
    result = channels[handle].active;
    SDL_UnlockMutex(audio_mutex);
    
    return result;
}

//
// Update sound parameters
//
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    (void)pitch;
    
    if (handle < 0 || handle >= NUM_CHANNELS)
        return;
    
    if (!audio_mutex)
        return;
    
    SDL_LockMutex(audio_mutex);
    
    if (channels[handle].active)
    {
        channels[handle].volume = vol;
        channels[handle].separation = sep;
    }
    
    SDL_UnlockMutex(audio_mutex);
}

//
// Set sound volume
//
void I_SetSfxVolume(int volume)
{
    snd_SfxVolume = volume;
}


void I_SetChannels(void)
{
}

void I_UpdateSound(void)
{
}

void I_SubmitSound(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// MUSIC API.
//
void I_InitMusic(void)
{
    fprintf(stderr, "I_InitMusic: music not implemented (SDL2 only)\n");
}

void I_ShutdownMusic(void)
{
}

void I_SetMusicVolume(int volume)
{
    music_volume = volume;
}

void I_PlaySong(int handle, int looping)
{
    (void)handle;
    (void)looping;
}

void I_PauseSong(int handle)
{
    (void)handle;
}

void I_ResumeSong(int handle)
{
    (void)handle;
}

void I_StopSong(int handle)
{
    (void)handle;
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

int I_RegisterSong(void* data)
{
    (void)data;
    return 0;
}

int I_QrySongPlaying(int handle)
{
    (void)handle;
    return 0;
}