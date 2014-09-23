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

#ifndef __FLOOR_AUDIO_CONTROLLER_HPP__
#define __FLOOR_AUDIO_CONTROLLER_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENAL)

#include <floor/audio/audio_headers.hpp>
#include <floor/audio/audio_source.hpp>

class audio_controller {
public:
	//
	static void init();
	static void destroy();
	
	//! call this to update the listener position and orientation
	static void update(const float3& position,
					   const float3& forward_vec,
					   const float3& up_vec);
	
	//
	static void acquire_context();
	static bool try_acquire_context();
	static void release_context();
	
	//! creates an audio_source from an already loaded audio_store object
	static weak_ptr<audio_source> add_source(const string& store_identifier,
											 const audio_source::SOURCE_TYPE type = audio_source::SOURCE_TYPE::AUDIO_3D,
											 const string source_identifier = "");
	//! creates an audio_source from an already loaded audio_store object
	static weak_ptr<audio_source> add_source(weak_ptr<audio_store::audio_data> data,
											 const audio_source::SOURCE_TYPE type = audio_source::SOURCE_TYPE::AUDIO_3D,
											 const string source_identifier = "");
	
	//! loads an audio file via the audio_store and creates an audio_source from it
	static weak_ptr<audio_source> add_source(const string& filename,
											 const string& store_identifier,
											 const audio_source::SOURCE_TYPE type = audio_source::SOURCE_TYPE::AUDIO_3D,
											 const string source_identifier = "",
											 const vector<AUDIO_EFFECT> effects = vector<AUDIO_EFFECT> {});
	
	//! removes an audio_source from the audio_controller and deletes its data
	static bool remove_source(weak_ptr<audio_source> source);
	//! removes an audio_source from the audio_controller and deletes its data
	static bool remove_source(const string& source_identifier);
	
	//! applies the global volume set in floor::set_music_volume()
	//! to all audio sources managed by the audio controller
	static void update_music_volumes();
	//! applies the global volume set in floor::set_sound_volume()
	//! to all audio sources managed by the audio controller
	static void update_sound_volumes();
	
	//! the available efx slots
	static const vector<ALuint>& get_effect_slots();
	
protected:
	// static class
	audio_controller() = delete;
	~audio_controller() = delete;
	audio_controller& operator=(const audio_controller&) = delete;
	
	//
	static ALCdevice* device;
	static ALCcontext* context;
	
	//
	static recursive_mutex ctx_lock;
	static atomic<unsigned int> ctx_active_locks;
	static void handle_acquire();
	
	//
	static unordered_map<string, shared_ptr<audio_source>> sources;
	static weak_ptr<audio_source> internal_add_source(const string& store_identifier,
													  weak_ptr<audio_store::audio_data> data,
													  const audio_source::SOURCE_TYPE type,
													  const string source_identifier);
	
	//
	static vector<ALuint> effect_slots;
	
};

#endif

#endif
