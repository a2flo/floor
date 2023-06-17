/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/audio/audio_store.hpp>
#include <floor/audio/audio_controller.hpp>
#include <floor/threading/task.hpp>
#include <floor/floor/floor.hpp>

unordered_map<string, shared_ptr<audio_store::audio_data>> audio_store::store;

void audio_store::init() {
}

void audio_store::destroy() {
	for(const auto& data : store) {
		if(alIsBuffer(data.second->buffer)) {
			alDeleteBuffers(1, &data.second->buffer);
		}
	}
	store.clear();
}

weak_ptr<audio_store::audio_data> audio_store::add_file(const string& filename,
														const string& identifier,
														const vector<AUDIO_EFFECT> effects) {
	Uint8* audio_buffer = nullptr;
	Uint32 audio_len = 0;
	SDL_AudioSpec audio_spec;
	if(!SDL_LoadWAV(filename.c_str(), &audio_spec, &audio_buffer, &audio_len)) {
		log_error("couldn't load audio file $ \"$\": $",
				  identifier, filename, SDL_GetError());
		return weak_ptr<audio_store::audio_data> {};
	}
	
	log_debug("\"$\": rate $, channels $, encoding $X",
			  identifier, audio_spec.freq, (size_t)audio_spec.channels, audio_spec.format);
	
	ALenum format;
	switch(audio_spec.format) {
		case AUDIO_U8:
		case AUDIO_S8:
			format = (audio_spec.channels == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8);
			break;
		case AUDIO_U16:
		case AUDIO_S16:
		case AUDIO_U16MSB:
		case AUDIO_S16MSB:
			format = (audio_spec.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16);
			break;
		default:
			// S32/F32
			log_error("couldn't load audio file $ \"$\": S32 and F32 formats are currently unsupported!",
					  identifier, filename);
			SDL_FreeWAV(audio_buffer);
			return weak_ptr<audio_store::audio_data> {};
	}
	
	//
	audio_controller::acquire_context();
	AL_CLEAR_ERROR(); // clear error code
	ALuint buffer_data = 0;
	AL(alGenBuffers(1, &buffer_data))
	AL(alBufferData(buffer_data, format, audio_buffer, (ALsizei)audio_len, audio_spec.freq))
	
	auto iter = store.emplace(identifier, make_shared<audio_data>(audio_data {
		filename,
		buffer_data,
		format,
		audio_spec.freq,
		default_velocity,
		default_volume,
		default_reference_distance,
		default_rolloff_factor,
		default_max_distance,
		effects
	}));
	audio_controller::release_context();
	
	SDL_FreeWAV(audio_buffer);
	
	floor::get_event()->add_event(EVENT_TYPE::AUDIO_STORE_LOAD,
								  make_shared<audio_store_load_event>(SDL_GetTicks64(), identifier));
	
	return iter.first->second;
}

weak_ptr<audio_store::audio_data> audio_store::add_raw(const uint8_t* raw_data,
													   const ALsizei& raw_data_len,
													   const ALenum& format,
													   const ALsizei& frequency,
													   const string& identifier,
													   const vector<AUDIO_EFFECT> effects) {
	audio_controller::acquire_context();
	AL_CLEAR_ERROR(); // clear error code
	ALuint buffer_data = 0;
	AL(alGenBuffers(1, &buffer_data))
	AL(alBufferData(buffer_data, format, raw_data, raw_data_len, frequency))
	
	auto iter = store.emplace(identifier, make_shared<audio_data>(audio_data {
		"RAW:"+identifier,
		buffer_data,
		format,
		frequency,
		default_velocity,
		default_volume,
		default_reference_distance,
		default_rolloff_factor,
		default_max_distance,
		effects
	}));
	audio_controller::release_context();
	
	floor::get_event()->add_event(EVENT_TYPE::AUDIO_STORE_LOAD,
								  make_shared<audio_store_load_event>(SDL_GetTicks64(), identifier));
	
	return iter.first->second;
}

bool audio_store::has_audio_data(const string& identifier) {
	return (store.count(identifier) > 0);
}

weak_ptr<audio_store::audio_data> audio_store::get_audio_data(const string& identifier) {
	const auto iter = store.find(identifier);
	if(iter == store.end()) return weak_ptr<audio_store::audio_data> {};
	return iter->second;
}

const unordered_map<string, shared_ptr<audio_store::audio_data>>& audio_store::get_store() {
	return store;
}

#endif
