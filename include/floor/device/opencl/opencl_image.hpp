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

#pragma once

#include <floor/device/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/device/device_image.hpp>
#include <floor/core/aligned_ptr.hpp>

namespace fl {

class opencl_device;
class opencl_image final : public device_image {
public:
	opencl_image(const device_queue& cqueue,
				 const uint4 image_dim,
				 const IMAGE_TYPE image_type,
				 std::span<uint8_t> host_data_ = {},
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
				 const uint32_t mip_level_limit = 0u);
	
	~opencl_image() override;
	
	bool zero(const device_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the OpenCL specific image object/pointer
	const cl_mem& get_cl_image() const {
		return image;
	}
	
protected:
	cl_mem image { nullptr };
	cl_mem_flags cl_flags { 0 };
	
	// mip-level origin index for use with cl*Image functions
	const uint32_t mip_origin_idx;
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
	//! if cqueue isn't nullptr, returns its cl_command_queue, otherwise returns the devices default queue
	cl_command_queue queue_or_default_queue(const device_queue* cqueue) const;
	
	struct opencl_mapping {
		aligned_ptr<uint8_t> ptr;
		const MEMORY_MAP_FLAG flags;
		std::vector<void*> mapped_ptrs;
		std::vector<size_t> level_sizes;
	};
	// stores all mapped pointers and the mapped buffer
	std::unordered_map<void*, opencl_mapping> mappings;
	
};

} // namespace fl

#endif
