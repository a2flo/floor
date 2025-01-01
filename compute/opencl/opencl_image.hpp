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

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_image.hpp>
#include <floor/core/aligned_ptr.hpp>

class opencl_device;
class opencl_image final : public compute_image {
public:
	opencl_image(const compute_queue& cqueue,
				 const uint4 image_dim,
				 const COMPUTE_IMAGE_TYPE image_type,
				 std::span<uint8_t> host_data_ = {},
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
	
	~opencl_image() override;
	
	bool zero(const compute_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the opencl specific image object/pointer
	const cl_mem& get_cl_image() const {
		return image;
	}
	
protected:
	cl_mem image { nullptr };
	cl_mem_flags cl_flags { 0 };
	
	// mip-level origin index for use with cl*Image functions
	const uint32_t mip_origin_idx;
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
	//! if cqueue isn't nullptr, returns its cl_command_queue, otherwise returns the devices default queue
	cl_command_queue queue_or_default_queue(const compute_queue* cqueue) const;
	
	struct opencl_mapping {
		aligned_ptr<uint8_t> ptr;
		const COMPUTE_MEMORY_MAP_FLAG flags;
		vector<void*> mapped_ptrs;
		vector<size_t> level_sizes;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, opencl_mapping> mappings;
	
};

#endif
