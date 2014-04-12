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

#ifndef __FLOOR_AUDIO_STORE_HPP__
#define __FLOOR_AUDIO_STORE_HPP__

#if !defined(FLOOR_NO_OPENAL)

#include "core/cpp_headers.hpp"
#include "core/vector3.hpp"
#include "audio/audio_headers.hpp"

//! these are the only effects supported by all openal implementations
enum class AUDIO_EFFECT : unsigned int {
	REVERB,
	ECHO
};

class audio_store {
public:
	static void init();
	static void destroy();
	
	static constexpr float default_volume { 1.0f };
	static constexpr float3 default_velocity { 0.0f };
	static constexpr float default_reference_distance { 5.0f };
	static constexpr float default_rolloff_factor { 3.5f };
	static constexpr float default_max_distance { 1000.0f };
	
	struct audio_data {
		const string filename;
		const ALuint buffer;
		const ALenum format;
		const ALsizei freq;
		const float3 velocity;
		const float volume;
		const float reference_distance;
		const float rolloff_factor;
		const float max_distance;
		const vector<AUDIO_EFFECT> effects;
	};
	
	static weak_ptr<audio_data> load_file(const string& filename,
										  const string& identifier,
										  const vector<AUDIO_EFFECT> effects = vector<AUDIO_EFFECT> {});
	
	static bool has_audio_data(const string& identifier);
	static weak_ptr<audio_data> get_audio_data(const string& identifier);
	
	static const unordered_map<string, shared_ptr<audio_data>>& get_store();
	
protected:
	// static class
	audio_store() = delete;
	~audio_store() = delete;
	audio_store& operator=(const audio_store&) = delete;
	
	// <identifier, data>
	static unordered_map<string, shared_ptr<audio_data>> store;
	
};

#endif

#endif
