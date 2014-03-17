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

#include "audio/audio_controller.hpp"
#include "audio/audio_store.hpp"

ALCdevice* audio_controller::device { nullptr };
ALCcontext* audio_controller::context { nullptr };
recursive_mutex audio_controller::ctx_lock {};
atomic<unsigned int> audio_controller::ctx_active_locks { 0u };

void audio_controller::init() {
	// open device (TODO: add config option for device name)
	device = alcOpenDevice(nullptr);
	if(device == nullptr) {
		log_error("failed to open default openal device!");
		return;
	}
	
	// create context
	static const ALCint attrlist[] {
		0
	};
	
	context = alcCreateContext(device, attrlist);
	alcMakeContextCurrent(context);
	
	log_debug("openal initialized");
	
	// check extensions
	if(!alcIsExtensionPresent(device, "ALC_EXT_EFX")) {
		log_msg("openal efx is not supported on this device");
	}
	
	// init store
	audio_store::init();
}

void audio_controller::destroy() {
	//
	audio_store::destroy();
	
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

#endif
