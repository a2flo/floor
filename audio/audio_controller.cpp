/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/audio/audio_controller.hpp>
#include <floor/audio/audio_store.hpp>
#include <floor/floor/floor.hpp>

ALCdevice* audio_controller::device { nullptr };
ALCcontext* audio_controller::context { nullptr };
recursive_mutex audio_controller::ctx_lock {};
atomic<unsigned int> audio_controller::ctx_active_locks { 0u };
unordered_map<string, shared_ptr<audio_source>> audio_controller::sources;
vector<ALuint> audio_controller::effect_slots;

void audio_controller::init() {
	// open device
	const string& device_name { floor::get_audio_device_name() };
	device = alcOpenDevice(device_name.empty() ? nullptr : device_name.c_str());
	if(device == nullptr) {
		log_error("failed to open default openal device!");
		return;
	}
	
	// check extensions
	ALint aux_sends = 0;
	if(!alcIsExtensionPresent(device, "ALC_EXT_EFX")) {
		log_msg("openal efx is not supported on this device");
	}
	else {
		// check how many aux send slots we actually got + create efx slots
		alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &aux_sends);
	}
	log_debug("openal: got %u auxillary send slots", aux_sends);
	
	// create context
	aux_sends = std::max(aux_sends, 2); // request 2 aux sends max
	const ALCint attrlist[] {
		ALC_MAX_AUXILIARY_SENDS, aux_sends,
		0
	};
	
	context = alcCreateContext(device, attrlist);
	alcMakeContextCurrent(context);
	
	// create efx slots
	if(aux_sends > 0) {
		AL_CLEAR_ERROR();
		effect_slots.resize((size_t)aux_sends, 0);
		AL(alGenAuxiliaryEffectSlots(aux_sends, &effect_slots[0]));
		AL_IS_ERROR();
	}
	
	// use an exponential distance model for now
	AL(alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED));
	
	// init store
	audio_store::init();
	
	// done
	log_debug("openal initialized");
}

void audio_controller::destroy() {
	//
	sources.clear();
	
	//
	audio_store::destroy();
	
	//
	AL_CLEAR_ERROR();
	AL(alDeleteAuxiliaryEffectSlots((ALsizei)effect_slots.size(), &effect_slots[0]));
	effect_slots.clear();
	
	//
	if(context != nullptr) {
		alcDestroyContext(context);
		context = nullptr;
	}
	
	if(device != nullptr) {
		alcCloseDevice(device);
		device = nullptr;
	}
}

void audio_controller::update(const float3& position,
							  const float3& forward_vec,
							  const float3& up_vec) {
	acquire_context();
	AL(alListener3f(AL_POSITION, position.x, position.y, position.z));
	const ALfloat orientation[] {
		forward_vec.x, forward_vec.y, forward_vec.z,
		up_vec.x, up_vec.y, up_vec.z
	};
	AL(alListenerfv(AL_ORIENTATION, &orientation[0]));
	release_context();
}

bool audio_controller::try_acquire_context() {
	// note: the context lock is recursive, so one thread can lock it multiple times.
	if(!ctx_lock.try_lock()) return false;
	handle_acquire();
	return true;
}

void audio_controller::acquire_context() {
	// note: the context lock is recursive, so one thread can lock it multiple times.
	ctx_lock.lock();
	handle_acquire();
}

void audio_controller::handle_acquire() {
	// note: not a race, since there can only be one active al thread
	const unsigned int cur_active_locks = ctx_active_locks++;
	if(cur_active_locks == 0 && alcMakeContextCurrent(context) != 0) {
		if(AL_IS_ERROR()) {
			log_error("couldn't make openal context current!");
		}
	}
}

void audio_controller::release_context() {
	const unsigned int cur_active_locks = --ctx_active_locks;
	if(cur_active_locks == 0) {
		if(AL_IS_ERROR()) {
			log_error("couldn't release current openal context!");
		}
	}
	ctx_lock.unlock();
}

weak_ptr<audio_source> audio_controller::add_source(const string& store_identifier,
													const audio_source::SOURCE_TYPE type,
													const string source_identifier) {
	return internal_add_source(store_identifier,
							   audio_store::get_audio_data(store_identifier),
							   type, source_identifier);
}

weak_ptr<audio_source> audio_controller::add_source(weak_ptr<audio_store::audio_data> data,
													const audio_source::SOURCE_TYPE type,
													const string source_identifier) {
	return internal_add_source("nullptr", data, type, source_identifier);
}

weak_ptr<audio_source> audio_controller::add_source(const string& filename,
													const string& store_identifier,
													const audio_source::SOURCE_TYPE type,
													const string source_identifier,
													const vector<AUDIO_EFFECT> effects) {
	return internal_add_source(store_identifier,
							   audio_store::add_file(filename, store_identifier, effects),
							   type, source_identifier);
}

weak_ptr<audio_source> audio_controller::internal_add_source(const string& store_identifier,
															 weak_ptr<audio_store::audio_data> data,
															 const audio_source::SOURCE_TYPE type,
															 const string source_identifier) {
	auto data_ptr = data.lock();
	if(!data_ptr) {
		log_error("there is no such file/identifier \"%s\" in the audio store", store_identifier);
		return weak_ptr<audio_source> {};
	}
	
	string identifier = store_identifier;
	if(!source_identifier.empty()) {
		identifier += "." + source_identifier;
	}
	
	if(source_identifier.empty() || sources.count(identifier) > 0) {
		identifier += "." + ull2string(SDL_GetPerformanceCounter());
	}
	
	if(sources.count(identifier) > 0) {
		identifier += "." + uint2string(core::rand(numeric_limits<unsigned int>::max()));
	}
	
	const auto iter = sources.emplace(identifier, make_shared<audio_source>(identifier, type, data_ptr));
	return iter.first->second;
}

bool audio_controller::remove_source(weak_ptr<audio_source> source) {
	auto src_ptr = source.lock();
	if(!src_ptr) {
		log_error("source doesn't exist!");
		return false;
	}
	
	const auto iter = sources.find(src_ptr->get_identifier());
	if(iter == sources.end()) {
		log_error("source \"%s\" doesn't exist in the audio controller!",
				  src_ptr->get_identifier());
		return false;
	}
	
	src_ptr = nullptr;
	sources.erase(iter);
	return true;
}

bool audio_controller::remove_source(const string& source_identifier) {
	const auto iter = sources.find(source_identifier);
	if(iter == sources.end()) {
		log_error("source \"%s\" doesn't exist in the audio controller!",
				  source_identifier);
		return false;
	}
	sources.erase(iter);
	return true;
}

const vector<ALuint>& audio_controller::get_effect_slots() {
	return effect_slots;
}

void audio_controller::update_music_volumes() {
	acquire_context();
	for(auto& source : sources) {
		if(source.second->get_type() == audio_source::SOURCE_TYPE::AUDIO_BACKGROUND) {
			source.second->update_volume();
		}
	}
	release_context();
}

void audio_controller::update_sound_volumes() {
	acquire_context();
	for(auto& source : sources) {
		if(source.second->get_type() == audio_source::SOURCE_TYPE::AUDIO_3D) {
			source.second->update_volume();
			//
		}
	}
	release_context();
}

#endif
