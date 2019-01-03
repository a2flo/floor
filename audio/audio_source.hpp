/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_AUDIO_SOURCE_HPP__
#define __FLOOR_AUDIO_SOURCE_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENAL)

#include <floor/audio/audio_store.hpp>

class audio_source {
public:
	enum class SOURCE_TYPE {
		AUDIO_3D,
		AUDIO_BACKGROUND,
	};
	
	//! NOTE: don't create these directly, but rather use the functions provided
	//! by the audio_controller to create sources (this way, the audio_controller
	//! will do the memory and openal management + handle the global volume control)
	audio_source(const string& identifier,
				 const SOURCE_TYPE& type,
				 weak_ptr<audio_store::audio_data> data) noexcept;
	~audio_source() noexcept;
	
	// play state
	void play();
	void pause();
	void stop();
	void rewind();
	void loop(const bool state = true);
	
	bool is_playing() const;
	bool is_paused() const;
	bool is_looping() const;
	
	// these will query openal for the current state of the source object
	// and update the play state flags accordingly
	bool query_playing();
	bool query_paused();
	bool query_looping();
	bool query_initial();
	
	// misc
	const string& get_identifier() const;
	const SOURCE_TYPE& get_type() const;
	
	void set_volume(const float& volume);
	void update_volume() const;
	const float& get_volume() const;
	
	// 3d/effect functions
	void set_position(const float3& position);
	const float3& get_position() const;
	void set_velocity(const float3& velocity);
	const float3& get_velocity() const;
	void set_reference_distance(const float& reference_distance);
	const float& get_reference_distance() const;
	void set_rolloff_factor(const float& rolloff_factor);
	const float& get_rolloff_factor() const;
	void set_max_distance(const float& max_distance);
	const float& get_max_distance() const;
	
protected:
	const string identifier;
	const SOURCE_TYPE type;
	weak_ptr<audio_store::audio_data> data;
	
	ALuint source { 0u };
	
	// misc state
	float volume { 1.0f };
	bool playing { false };
	bool paused { true };
	bool looping { false };
	ALint update_source_state();
	
	float3 position;
	float3 velocity;
	float reference_distance { 0.0f };
	float rolloff_factor { 0.0f };
	float max_distance { 0.0f };
	
	// (efx) effects and filters
	vector<ALuint> effects;
	vector<ALuint> filters;
	
};

#endif

#endif
