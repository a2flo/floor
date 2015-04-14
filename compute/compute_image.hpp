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

#ifndef __FLOOR_COMPUTE_IMAGE_HPP__
#define __FLOOR_COMPUTE_IMAGE_HPP__

#include <floor/compute/compute_memory.hpp>
#include <floor/compute/device/image_types.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_image : public compute_memory {
public:
	//! TODO: create image descriptor for advanced use?
	//! TODO: ...
	compute_image(const void* device,
				  const uint4 image_dim_,
				  const COMPUTE_IMAGE_TYPE image_type_,
				  void* host_ptr_ = nullptr,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0) :
	compute_memory(device, host_ptr_, flags_, opengl_type_),
	image_dim(image_dim_), image_type(image_type_),
	image_data_size(image_data_size_from_types(image_dim, image_type)) {
		// TODO: make sure format is supported, fail early if not
		if(!image_format_valid(image_type)) {
			log_error("invalid image format: %X", image_type);
			return;
		}
		// TODO: if opengl_type is 0 and opengl sharing is enabled, try guessing it, otherwise fail
	}
	
	virtual ~compute_image() = 0;
	
	// TODO: read, write, copy, fill, zero/clear
	// TODO: map with dim size and dim coords/offset
	
	//! maps device memory into host accessible memory,
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags =
													(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
													 COMPUTE_MEMORY_MAP_FLAG::BLOCK)) = 0;
	
	//! maps device memory into host accessible memory,
	//! returning the mapped pointer as a vector<> of "data_type"
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type>
	vector<data_type>* map(shared_ptr<compute_queue> cqueue,
						   const COMPUTE_MEMORY_MAP_FLAG flags_ =
						   (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
							COMPUTE_MEMORY_MAP_FLAG::BLOCK)) {
		return (vector<data_type>*)map(cqueue, flags_);
	}
	
	//! maps device memory into host accessible memory,
	//! returning the mapped pointer as an array<> of "data_type" with "n" elements
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type, size_t n>
	array<data_type, n>* map(shared_ptr<compute_queue> cqueue,
							 const COMPUTE_MEMORY_MAP_FLAG flags_ =
							 (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
							  COMPUTE_MEMORY_MAP_FLAG::BLOCK)) {
		return (array<data_type, n>*)map(cqueue, flags_);
	}
	
	//! unmaps a previously mapped memory pointer
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) = 0;
	
protected:
	const uint4 image_dim;
	const COMPUTE_IMAGE_TYPE image_type;
	const size_t image_data_size;

	// internal function to create/delete an opengl image if compute/opengl sharing is used
	bool create_gl_image(const bool copy_host_data);
	void delete_gl_image();
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
