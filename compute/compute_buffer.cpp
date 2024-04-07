/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/core/logger.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/metal/metal_buffer.hpp>
#endif
#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#endif

compute_buffer::compute_buffer(const compute_queue& cqueue,
							   const size_t& size_,
							   std::span<uint8_t> host_data_,
							   const COMPUTE_MEMORY_FLAG flags_,
							   compute_buffer* shared_buffer_) :
compute_memory(cqueue, host_data_, flags_), size(align_size(size_)), shared_buffer(shared_buffer_) {
	if (size == 0) {
		throw std::runtime_error("can't allocate a buffer of size 0!");
	}
	
	if (host_data.data() != nullptr && host_data.size_bytes() != size) {
		throw std::runtime_error("host data size " + std::to_string(host_data.size_bytes()) + " does not match buffer size " + std::to_string(size));
	}
	
	if (size_ != size) {
		log_error("buffer size must always be a multiple of $! - using size of $ instead of $ now",
				  min_multiple(), size, size_);
	}
	
	if (shared_buffer != nullptr) {
		if (!has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags) && !has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
			log_warn("provided a shared buffer, but no sharing flag is set");
		}
	}
	
	// if there is host data, it must have at least the same size as the buffer
	if (host_data.data() != nullptr && host_data.size_bytes() < size) {
		throw std::runtime_error("image host data size " + std::to_string(host_data.size_bytes()) +
								 " is smaller than the expected buffer size " + std::to_string(size));
	}
}

shared_ptr<compute_buffer> compute_buffer::clone(const compute_queue& cqueue, const bool copy_contents,
												 const COMPUTE_MEMORY_FLAG flags_override) {
	if (dev.context == nullptr) {
		log_error("invalid buffer/device state");
		return {};
	}
	
	auto clone_flags = (flags_override != COMPUTE_MEMORY_FLAG::NONE ? flags_override : flags);
	shared_ptr<compute_buffer> ret;
	if (host_data.data() != nullptr) {
		// never copy host data on the newly created buffer
		clone_flags |= COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY;
		assert(size == host_data.size_bytes());
		ret = dev.context->create_buffer(cqueue, host_data, clone_flags);
	} else {
		ret = dev.context->create_buffer(cqueue, size, clone_flags);
	}
	if (ret == nullptr) {
		return {};
	}
	
	if (copy_contents) {
		ret->copy(cqueue, *this);
	}
	
	return ret;
}

const metal_buffer* compute_buffer::get_underlying_metal_buffer_safe() const {
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		const metal_buffer* ret = get_shared_metal_buffer();
		if (ret) {
			if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Metal use
				release_metal_buffer(nullptr, nullptr);
			} else if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING_SYNC_SHARED>(flags)) {
				sync_metal_buffer(nullptr, nullptr);
			}
		} else {
			ret = (const metal_buffer*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_METAL)
		if (auto test_cast_mtl_buffer = dynamic_cast<const metal_buffer*>(ret); !test_cast_mtl_buffer) {
			throw runtime_error("specified buffer is neither a Metal buffer nor a shared Metal buffer");
		}
#endif
		return ret;
	}
	return (const metal_buffer*)this;
}

const vulkan_buffer* compute_buffer::get_underlying_vulkan_buffer_safe() const {
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		const vulkan_buffer* ret = get_shared_vulkan_buffer();
		if (ret) {
			if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Vulkan use
				release_vulkan_buffer(nullptr, nullptr);
			} else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING_SYNC_SHARED>(flags)) {
				sync_vulkan_buffer(nullptr, nullptr);
			}
		} else {
			ret = (const vulkan_buffer*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_VULKAN)
		if (auto test_cast_vk_buffer = dynamic_cast<const vulkan_buffer*>(ret); !test_cast_vk_buffer) {
			throw runtime_error("specified buffer is neither a Vulkan buffer nor a shared Vulkan buffer");
		}
#endif
		return ret;
	}
	return (const vulkan_buffer*)this;
}
