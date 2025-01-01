/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/compute/compute_memory.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/core/logger.hpp>

static constexpr COMPUTE_MEMORY_FLAG handle_memory_flags(COMPUTE_MEMORY_FLAG flags) {
	// Vulkan sharing handling
	if(has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		// clear out USE_HOST_MEMORY flag if it is set
		if(has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags)) {
			flags &= ~COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY;
		}
	}
	
	// Metal sharing handling
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		// clear out USE_HOST_MEMORY flag if it is set
		if (has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags)) {
			flags &= ~COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY;
		}
	}
	
	// handle read/write flags
	if((flags & COMPUTE_MEMORY_FLAG::READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		// neither read nor write is set -> set read/write
		flags |= COMPUTE_MEMORY_FLAG::READ_WRITE;
	}
	
	// handle host read/write flags
	if((flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE &&
	   has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags)) {
		// can't be using host memory and declaring that the host doesn't access the memory
		log_error("USE_HOST_MEMORY specified, but host read/write flags set to NONE!");
		flags |= COMPUTE_MEMORY_FLAG::HOST_READ_WRITE;
	}
	
	// handle SHARING_SYNC and related flags
	if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
		if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags) ||
			has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
			// automatically set SHARING_RENDER_READ_WRITE/SHARING_COMPUTE_READ_WRITE if no r/w is defined
			if (!has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ>(flags) &&
				!has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_WRITE>(flags)) {
				flags |= COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE;
			}
			if (!has_flag<COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ>(flags) &&
				!has_flag<COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_WRITE>(flags)) {
				flags |= COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE;
			}
			
			// check for incompatible r/w flags
			if ((flags & COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE) == COMPUTE_MEMORY_FLAG::SHARING_RENDER_WRITE &&
				(flags & COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE) == COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_WRITE) {
				log_warn("both the render and compute backend are set to write-only (SHARING_RENDER_WRITE/SHARING_COMPUTE_WRITE)");
				flags |= COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE;
				flags |= COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE;
			}
			if ((flags & COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE) == COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ &&
				(flags & COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE) == COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ) {
				log_warn("both the render and compute backend are set to read-only (SHARING_RENDER_READ/SHARING_COMPUTE_READ)");
				flags |= COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE;
				flags |= COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE;
			}
		} else {
			log_warn("SHARING_SYNC is set, but neither Vulkan nor Metal sharing is enabled");
			
			// clear all
			flags &= ~COMPUTE_MEMORY_FLAG::SHARING_SYNC;
			flags &= ~COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE;
			flags &= ~COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE;
		}
	}
	
	return flags;
}

compute_memory::compute_memory(const compute_queue& cqueue,
							   std::span<uint8_t> host_data_,
							   const COMPUTE_MEMORY_FLAG flags_) :
dev(cqueue.get_device()), host_data(host_data_), flags(handle_memory_flags(flags_)) {
	if((flags_ & COMPUTE_MEMORY_FLAG::READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		log_error("memory must be read-only, write-only or read-write!");
	}
	if(has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags_) &&
	   has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags_)) {
		log_error("USE_HOST_MEMORY and VULKAN_SHARING are mutually exclusive!");
	}
	if(has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags_) &&
	   has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags_)) {
		log_error("USE_HOST_MEMORY and METAL_SHARING are mutually exclusive!");
	}
}

compute_memory::~compute_memory() {
	dev.context->remove_from_resource_registry(this);
}

void compute_memory::_lock() const {
	lock.lock();
}

void compute_memory::_unlock() const {
	lock.unlock();
}

const compute_queue* compute_memory::get_default_queue_for_memory(const compute_memory& mem) {
	const auto& mem_dev = mem.get_device();
	return mem_dev.context->get_device_default_queue(mem_dev);
}

void compute_memory::set_debug_label(const string& label) {
	dev.context->update_resource_registry(this, debug_label, label);
	debug_label = label;
}

const string& compute_memory::get_debug_label() const {
	return debug_label;
}

COMPUTE_MEMORY_FLAG compute_memory::make_host_shared_memory_flags(const COMPUTE_MEMORY_FLAG& flags,
																  const compute_device& shared_dev,
																  const bool copy_host_data) {
	auto shared_flags = flags;
	
	// we don't actually want to have sharing flags on the shared memory itself
	shared_flags &= ~COMPUTE_MEMORY_FLAG::SHARING_SYNC;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ_WRITE;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ_WRITE;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::VULKAN_SHARING_SYNC_SHARED;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::METAL_SHARING_SYNC_SHARED;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::VULKAN_SHARING;
	shared_flags &= ~COMPUTE_MEMORY_FLAG::METAL_SHARING;
	
	// determine necessary r/w flags
	if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
		if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_WRITE>(flags) &&
			!has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ>(flags)) {
			// -> Host-Compute will never to write/update data in the render backend
			shared_flags |= COMPUTE_MEMORY_FLAG::HOST_READ;
		} else if (!has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_WRITE>(flags) &&
				   has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ>(flags)) {
			// -> Host-Compute will never to read data in the render backend
			shared_flags |= COMPUTE_MEMORY_FLAG::HOST_WRITE;
		} else {
			shared_flags |= COMPUTE_MEMORY_FLAG::HOST_READ_WRITE;
		}
	} else {
		shared_flags |= COMPUTE_MEMORY_FLAG::HOST_READ_WRITE;
	}
	
	if (!copy_host_data) {
		shared_flags |= COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY;
	}
	
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		// use host-coherent memory if preferred
		if (((const vulkan_device&)shared_dev).prefer_host_coherent_mem) {
			shared_flags |= COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT;
		}
	}
	
	return shared_flags;
}
