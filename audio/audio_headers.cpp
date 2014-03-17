/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#if !defined(FLOOR_NO_OPENAL)

#include "audio_headers.hpp"

LPALGENEFFECTS alGenEffects { (LPALGENEFFECTS)alGetProcAddress("alGenEffects") };
LPALDELETEEFFECTS alDeleteEffects { (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects") };
LPALISEFFECT alIsEffect { (LPALISEFFECT)alGetProcAddress("alIsEffect") };
LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots { (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress("alGenAuxiliaryEffectSlots") };
LPALEFFECTI alEffecti { (LPALEFFECTI)alGetProcAddress("alEffecti") };
LPALEFFECTF alEffectf { (LPALEFFECTF)alGetProcAddress("alEffectf") };
LPALGENFILTERS alGenFilters { (LPALGENFILTERS)alGetProcAddress("alGenFilters") };
LPALISFILTER alIsFilter { (LPALISFILTER)alGetProcAddress("alIsFilter") };
LPALFILTERI alFilteri { (LPALFILTERI)alGetProcAddress("alFilteri") };
LPALFILTERF alFilterf { (LPALFILTERF)alGetProcAddress("alFilterf") };
LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti { (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress("alAuxiliaryEffectSloti") };
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots { (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots") };
LPALDELETEFILTERS alDeleteFilters { (LPALDELETEFILTERS)alGetProcAddress("alDeleteFilters") };

bool floor_audio::check_openal_efx_funcs() {
	const auto fail = [](const string& name) -> bool {
		log_error("failed to get function pointer for \"%s\"!", name);
		return false;
	};
	
	if(alGenEffects == nullptr) return fail("alGenEffects");
	if(alDeleteEffects == nullptr) return fail("alDeleteEffects");
	if(alIsEffect == nullptr) return fail("alIsEffect");
	if(alGenAuxiliaryEffectSlots == nullptr) return fail("alGenAuxiliaryEffectSlots");
	if(alEffecti == nullptr) return fail("alEffecti");
	if(alEffectf == nullptr) return fail("alEffectf");
	if(alGenFilters == nullptr) return fail("alGenFilters");
	if(alIsFilter == nullptr) return fail("alIsFilter");
	if(alFilteri == nullptr) return fail("alFilteri");
	if(alFilterf == nullptr) return fail("alFilterf");
	if(alAuxiliaryEffectSloti == nullptr) return fail("alAuxiliaryEffectSloti");
	if(alDeleteAuxiliaryEffectSlots == nullptr) return fail("alDeleteAuxiliaryEffectSlots");
	if(alDeleteFilters == nullptr) return fail("alDeleteFilters");
	
	return true;
}

#endif
