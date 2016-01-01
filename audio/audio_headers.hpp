/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <floor/core/essentials.hpp>

#ifndef __FLOOR_AUDIO_HEADERS_HPP__
#define __FLOOR_AUDIO_HEADERS_HPP__

#if !defined(FLOOR_NO_OPENAL)

#if defined(__APPLE__)
#include <OpenALSoft/al.h>
#include <OpenALSoft/alc.h>
#include <OpenALSoft/efx.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#endif

extern LPALGENEFFECTS alGenEffects;
extern LPALDELETEEFFECTS alDeleteEffects;
extern LPALISEFFECT alIsEffect;
extern LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
extern LPALEFFECTI alEffecti;
extern LPALEFFECTF alEffectf;
extern LPALGENFILTERS alGenFilters;
extern LPALISFILTER alIsFilter;
extern LPALFILTERI alFilteri;
extern LPALFILTERF alFilterf;
extern LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
extern LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
extern LPALDELETEFILTERS alDeleteFilters;

namespace floor_audio {
	bool check_openal_efx_funcs();
};

//
#define AL_ERROR_TO_STR(code) (\
(code == 0xA001 ? "AL_INVALID_NAME" : \
(code == 0xA002 ? "AL_INVALID_ENUM" : \
(code == 0xA003 ? "AL_INVALID_VALUE" : \
(code == 0xA004 ? "AL_INVALID_OPERATION" : \
(code == 0xA005 ? "AL_OUT_OF_MEMORY" : "<unknown al error>"))))))

#define AL_IS_ERROR() ([]() -> bool { \
	int al_error = alGetError(); \
	if(al_error != AL_NO_ERROR) { log_error("OpenAL Error in line #%u: %X: %s", __LINE__, al_error, AL_ERROR_TO_STR(al_error)); } \
	return (al_error != AL_NO_ERROR); \
}())

#define AL_CLEAR_ERROR() ([]() { \
	int al_error = alGetError(); \
	if(al_error != AL_NO_ERROR) { log_error("(CLEAR) OpenAL Error in line #%u: %X: %s", __LINE__, al_error, AL_ERROR_TO_STR(al_error)); } \
}())

#define AL(_AL_CALL) {\
	AL_CLEAR_ERROR(); \
	_AL_CALL; \
	AL_CLEAR_ERROR(); \
}

#endif

#endif
