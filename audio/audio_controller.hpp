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

#if !defined(FLOOR_NO_OPENAL)

#include "audio/audio_headers.hpp"

class audio_controller {
public:
	static void init();
	static void destroy();
	
	static void update(const float3& position,
					   const float3& forward_vec,
					   const float3& up_vec);
	
	static void acquire_context();
	static bool try_acquire_context();
	static void release_context();
	
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
	
};

#endif

#endif
