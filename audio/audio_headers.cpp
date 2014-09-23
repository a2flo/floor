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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENAL)

#include <floor/audio/audio_headers.hpp>
#include <floor/core/logger.hpp>

LPALGENEFFECTS alGenEffects;
LPALDELETEEFFECTS alDeleteEffects;
LPALISEFFECT alIsEffect;
LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
LPALEFFECTI alEffecti;
LPALEFFECTF alEffectf;
LPALGENFILTERS alGenFilters;
LPALISFILTER alIsFilter;
LPALFILTERI alFilteri;
LPALFILTERF alFilterf;
LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
LPALDELETEFILTERS alDeleteFilters;

bool floor_audio::check_openal_efx_funcs() {
	const auto fail = [](const string& name) -> bool {
		log_error("failed to get function pointer for \"%s\"!", name);
		return false;
	};
	
	alGenEffects = { (LPALGENEFFECTS)alGetProcAddress("alGenEffects") };
	alDeleteEffects = { (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects") };
	alIsEffect = { (LPALISEFFECT)alGetProcAddress("alIsEffect") };
	alGenAuxiliaryEffectSlots = { (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress("alGenAuxiliaryEffectSlots") };
	alEffecti = { (LPALEFFECTI)alGetProcAddress("alEffecti") };
	alEffectf = { (LPALEFFECTF)alGetProcAddress("alEffectf") };
	alGenFilters = { (LPALGENFILTERS)alGetProcAddress("alGenFilters") };
	alIsFilter = { (LPALISFILTER)alGetProcAddress("alIsFilter") };
	alFilteri = { (LPALFILTERI)alGetProcAddress("alFilteri") };
	alFilterf = { (LPALFILTERF)alGetProcAddress("alFilterf") };
	alAuxiliaryEffectSloti = { (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress("alAuxiliaryEffectSloti") };
	alDeleteAuxiliaryEffectSlots = { (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots") };
	alDeleteFilters = { (LPALDELETEFILTERS)alGetProcAddress("alDeleteFilters") };
	
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
