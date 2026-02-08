
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
#include <SDL_mixer.h>

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
#define SAMPLECOUNT		512
#define NUM_CHANNELS		8
// It is 2 for 16bit, and 2 for two channels.
#define BUFMUL                  4
#define MIXBUFFERSIZE		(SAMPLECOUNT*BUFMUL)

#define SAMPLERATE		22050	// Hz
#define SAMPLESIZE		2   	// 16bit

// The actual lengths of all sound effects.
int 		lengths[NUMSFX];

Mix_Chunk*  sound_chunks[NUMSFX];

Mix_Music*  current_music = NULL;




//
// This function loads the sound data from the WAD lump,
//  for single sound
//
//
// This function loads the sound data from the WAD lump,
//  for single sound
//
Mix_Chunk*
getsfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    int                 size;
    char                name[20];
    int                 sfxlump;
    Mix_Chunk*          chunk;
    unsigned char*      wavbuf;
    int                 wavsize;
    int                 samplerate;
    int                 samples;

    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength( sfxlump );
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    if (size < 8 || sfx[0] != 0x03 || sfx[1] != 0x00) {
        Z_Free(sfx);
        return NULL;
    }

    samplerate = sfx[2] | (sfx[3] << 8);
    samples = sfx[4] | (sfx[5] << 8) | (sfx[6] << 16) | (sfx[7] << 24);

    if (samples > size - 8)
        samples = size - 8;

    wavsize = 44 + samples;
    wavbuf = (unsigned char*)Z_Malloc(wavsize, PU_STATIC, 0);

    memcpy(wavbuf, "RIFF", 4);
    int32_t chunksize = wavsize - 8;
    memcpy(wavbuf + 4, &chunksize, 4);
    memcpy(wavbuf + 8, "WAVE", 4);

    memcpy(wavbuf + 12, "fmt ", 4);
    int32_t fmtsize = 16;
    memcpy(wavbuf + 16, &fmtsize, 4);
    int16_t format = 1; // PCM
    memcpy(wavbuf + 20, &format, 2);
    int16_t channels = 1; // Mono
    memcpy(wavbuf + 22, &channels, 2);
    int32_t srate = samplerate;
    memcpy(wavbuf + 24, &srate, 4);
    int32_t byterate = srate * 1 * 1; // rate * channels * bps/8
    memcpy(wavbuf + 28, &byterate, 4);
    int16_t blockalign = 1;
    memcpy(wavbuf + 32, &blockalign, 2);
    int16_t bps = 8;
    memcpy(wavbuf + 34, &bps, 2);

    memcpy(wavbuf + 36, "data", 4);
    int32_t datasize = samples;
    memcpy(wavbuf + 40, &datasize, 4);

    memcpy(wavbuf + 44, sfx + 8, samples);

    SDL_RWops* rw = SDL_RWFromMem(wavbuf, wavsize);
    chunk = Mix_LoadWAV_RW(rw, 1); // 1 = close rwops automatically

    Z_Free(sfx);
    Z_Free(wavbuf);

    if (!chunk) {
        fprintf(stderr, "Mix_LoadWAV_RW failed for %s: %s\n", sfxname, Mix_GetError());
        return NULL;
    }

    *len = samples;
    return chunk;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
}

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_SfxVolume = volume;

  Mix_Volume(-1, (volume * MIX_MAX_VOLUME) / 15);
}

// MUSIC API
void I_SetMusicVolume(int volume)
{
  // Internal state variable.
  snd_MusicVolume = volume;

  Mix_VolumeMusic((volume * MIX_MAX_VOLUME) / 15);
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
    Mix_Chunk* chunk;
    int channel;

    priority = 0;
    pitch = 0;
    
    // Debug.
    //fprintf( stderr, "starting sound %d", id );

    if (id < 1 || id >= NUMSFX) {
        fprintf(stderr, "I_StartSound: invalid sound id %d\n", id);
        return -1;
    }
    
    chunk = sound_chunks[id];
    if (!chunk) {
        fprintf(stderr, "I_StartSound: sound %d not loaded\n", id);
        return -1;
    }
    
    // Set volume for this chunk (0-128 scale)
    Mix_VolumeChunk(chunk, (vol * MIX_MAX_VOLUME) / 127);
    
    // Play the sound on any available channel
    channel = Mix_PlayChannel(-1, chunk, 0);
    
    if (channel == -1) {
        fprintf(stderr, "I_StartSound: Mix_PlayChannel failed: %s\n", Mix_GetError());
        return -1;
    }
    
    // Set panning (separation)
    // sep ranges from 0 (left) to 255 (right), with 128 being center
    // SDL_mixer uses 0-254 for SetPanning, where 127 is center
    int left = 254 - sep;
    int right = sep;
    if (left < 0) left = 0;
    if (left > 254) left = 254;
    if (right < 0) right = 0;
    if (right > 254) right = 254;
    
    Mix_SetPanning(channel, left, right);
    
    return channel;
}



void I_StopSound (int handle)
{
  // Stop the channel
  if (handle >= 0) {
      Mix_HaltChannel(handle);
  }
}


int I_SoundIsPlaying(int handle)
{
    // Check if the channel is still playing
    if (handle < 0) {
        return 0;
    }
    
    return Mix_Playing(handle);
}




//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
// This function currently supports only 16bit.
//
void I_UpdateSound( void )
{

}


// 
// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime. 
// It is called during Timer interrupt with SNDINTR.
// Mixing now done synchronous, and
//  only output be done asynchronous?
//
void
I_SubmitSound(void)
{
}



void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // Update sound parameters for a playing sound
  if (handle < 0 || !Mix_Playing(handle)) {
      return;
  }

  Mix_Volume(handle, (vol * MIX_MAX_VOLUME) / 127);

  int left = 254 - sep;
  int right = sep;
  if (left < 0) left = 0;
  if (left > 254) left = 254;
  if (right < 0) right = 0;
  if (right > 254) right = 254;
  
  Mix_SetPanning(handle, left, right);

  (void)pitch;
}




void I_ShutdownSound(void)
{    
  int i;

  for (i = 0; i < NUMSFX; i++) {
      if (sound_chunks[i]) {
          Mix_FreeChunk(sound_chunks[i]);
          sound_chunks[i] = NULL;
      }
  }

  Mix_CloseAudio();
  Mix_Quit();
}






void
I_InitSound()
{ 
  int i;
  
  // Secure and configure sound device first.
  fprintf( stderr, "I_InitSound: ");

  if (Mix_OpenAudio(SAMPLERATE, AUDIO_S16SYS, 2, 512) < 0)
  {
      fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
      return;
  }

  Mix_AllocateChannels(NUM_CHANNELS);
  
  fprintf(stderr, "configured SDL_mixer\n");

  // Initialize external data (all sounds) at start, keep static.
  fprintf( stderr, "I_InitSound: ");

  for (i = 0; i < NUMSFX; i++) {
      sound_chunks[i] = NULL;
  }
  
  for (i=1 ; i<NUMSFX ; i++)
  { 
    // Alias? Example is the chaingun sound linked to pistol.
    if (!S_sfx[i].link)
    {
      // Load data from WAD file.
      sound_chunks[i] = getsfx( S_sfx[i].name, &lengths[i] );
      S_sfx[i].data = sound_chunks[i];
    }
    else
    {
      // Previously loaded already?
      sound_chunks[i] = sound_chunks[S_sfx[i].link-S_sfx];
      S_sfx[i].data = sound_chunks[i];
      lengths[i] = lengths[S_sfx[i].link-S_sfx];
    }
  }

  fprintf( stderr, " pre-cached all sound data\n");
  
  // Finished initialization.
  fprintf(stderr, "I_InitSound: sound module ready\n");
    
}




//
// MUSIC API.
//
void I_InitMusic(void)		
{
    fprintf(stderr, "I_InitMusic: music module ready\n");
}

void I_ShutdownMusic(void)	
{ 
    if (current_music) {
        Mix_FreeMusic(current_music);
        current_music = NULL;
    }
}

void I_PlaySong(int handle, int looping)
{
    Mix_Music* music = (Mix_Music*)handle;
    
    if (music) {
        Mix_PlayMusic(music, looping ? -1 : 0);
    }
}

void I_PauseSong (int handle)
{
    (void)handle;
    Mix_PauseMusic();
}

void I_ResumeSong (int handle)
{
    (void)handle;
    Mix_ResumeMusic();
}

void I_StopSong(int handle)
{
    (void)handle;
    Mix_HaltMusic();
}

void I_UnRegisterSong(int handle)
{
    Mix_Music* music = (Mix_Music*)handle;
    
    if (music) {
        Mix_FreeMusic(music);
        if (music == current_music) {
            current_music = NULL;
        }
    }
}

int I_RegisterSong(void* data)
{
    SDL_RWops* rw;
    Mix_Music* music;
    int size = 0;
    
    if (!data) {
        return 0;
    }

    for (int i=0; i<NUMMUSIC; i++) {
        if (S_music[i].data == data) {
            size = W_LumpLength(S_music[i].lumpnum);
            break;
        }
    }

    if (size == 0) {
        size = 128 * 1024;
    }

    if (current_music) {
        Mix_FreeMusic(current_music);
        current_music = NULL;
    }

    rw = SDL_RWFromMem(data, size);
    
    if (!rw) {
        fprintf(stderr, "I_RegisterSong: SDL_RWFromMem failed\n");
        return 0;
    }
    
    music = Mix_LoadMUS_RW(rw, 1);
    
    if (!music) {
        fprintf(stderr, "I_RegisterSong: Mix_LoadMUS_RW failed: %s\n", Mix_GetError());
        return 0;
    }
    
    current_music = music;

    return (int)music;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
    (void)handle;
    return Mix_PlayingMusic();
}
