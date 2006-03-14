/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org

    This file hacked^H^H^H^H^H^Hwritten by Ryan C. Gordon
        (icculus@icculus.org)
*/
#include "SDL_config.h"

/* Output audio to nowhere... */

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "SDL_dummyaudio.h"

/* The tag name used by DUMMY audio */
#define DUMMYAUD_DRIVER_NAME         "dummy"

/* environment variables and defaults. */
#define DUMMYENVR_WRITEDELAY      "SDL_DUMMYAUDIODELAY"
#define DUMMYDEFAULT_WRITEDELAY   150

/* Audio driver functions */
static int DUMMYAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void DUMMYAUD_WaitAudio(_THIS);
static void DUMMYAUD_PlayAudio(_THIS);
static Uint8 *DUMMYAUD_GetAudioBuf(_THIS);
static void DUMMYAUD_CloseAudio(_THIS);

/* Audio driver bootstrap functions */
static int DUMMYAUD_Available(void)
{
	const char *envr = SDL_getenv("SDL_AUDIODRIVER");
	if (envr && (SDL_strcmp(envr, DUMMYAUD_DRIVER_NAME) == 0)) {
		return(1);
	}
	return(0);
}

static void DUMMYAUD_DeleteDevice(SDL_AudioDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_AudioDevice *DUMMYAUD_CreateDevice(int devindex)
{
	SDL_AudioDevice *this;
	const char *envr;

	/* Initialize all variables that we clean on shutdown */
	this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		SDL_memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			SDL_free(this);
		}
		return(0);
	}
	SDL_memset(this->hidden, 0, (sizeof *this->hidden));

	envr = SDL_getenv(DUMMYENVR_WRITEDELAY);
	this->hidden->write_delay = (envr) ? SDL_atoi(envr) : DUMMYDEFAULT_WRITEDELAY;

	/* Set the function pointers */
	this->OpenAudio = DUMMYAUD_OpenAudio;
	this->WaitAudio = DUMMYAUD_WaitAudio;
	this->PlayAudio = DUMMYAUD_PlayAudio;
	this->GetAudioBuf = DUMMYAUD_GetAudioBuf;
	this->CloseAudio = DUMMYAUD_CloseAudio;

	this->free = DUMMYAUD_DeleteDevice;

	return this;
}

AudioBootStrap DUMMYAUD_bootstrap = {
	DUMMYAUD_DRIVER_NAME, "SDL dummy audio driver",
	DUMMYAUD_Available, DUMMYAUD_CreateDevice
};

/* This function waits until it is possible to write a full sound buffer */
static void DUMMYAUD_WaitAudio(_THIS)
{
	SDL_Delay(this->hidden->write_delay);
}

static void DUMMYAUD_PlayAudio(_THIS)
{
    /* no-op...this is a null driver. */
}

static Uint8 *DUMMYAUD_GetAudioBuf(_THIS)
{
	return(this->hidden->mixbuf);
}

static void DUMMYAUD_CloseAudio(_THIS)
{
	if ( this->hidden->mixbuf != NULL ) {
		SDL_FreeAudioMem(this->hidden->mixbuf);
		this->hidden->mixbuf = NULL;
	}
}

static int DUMMYAUD_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	/* Allocate mixing buffer */
	this->hidden->mixlen = spec->size;
	this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(this->hidden->mixlen);
	if ( this->hidden->mixbuf == NULL ) {
		return(-1);
	}
	SDL_memset(this->hidden->mixbuf, spec->silence, spec->size);

	/* We're ready to rock and roll. :-) */
	return(0);
}

