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

#ifndef __FLOOR_CUDA_IMAGE_HPP__
#define __FLOOR_CUDA_IMAGE_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_image.hpp>

class cuda_device;
class FLOOR_API cuda_image final : public compute_image {
public:
	cuda_image(const cuda_device* device,
			   const uint4 image_dim,
			   const COMPUTE_IMAGE_TYPE image_type,
			   void* host_ptr = nullptr,
			   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
												   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
			   const uint32_t opengl_type = 0);
	
	~cuda_image() override;
	
	bool acquire_opengl_object(shared_ptr<compute_queue> cqueue) override;
	bool release_opengl_object(shared_ptr<compute_queue> cqueue) override;
	
	void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags =
											(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
											 COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the cuda specific image pointer (cuda array)
	const CUarray& get_cuda_image() const { return image; }
	
	//! returns the cuda surface object
	const CUsurfObject& get_cuda_surface() const { return surface; }
	
	//! returns the cuda texture object
	const CUtexObject& get_cuda_texture() const { return texture; }
	
protected:
	CUarray image { nullptr };
	CUsurfObject surface { 0ull };
	CUtexObject texture { 0ull };
	CUgraphicsResource rsrc { nullptr };
	
	struct cuda_mapping {
		const size3 origin;
		const size3 region;
		const COMPUTE_MEMORY_MAP_FLAG flags;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, cuda_mapping> mappings;
	
	// separate create image function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue);
	
};

#endif

#endif
