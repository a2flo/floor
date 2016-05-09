/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
#include <floor/compute/device/cuda_sampler.hpp>

class cuda_device;
class cuda_image final : public compute_image {
public:
	cuda_image(const cuda_device* device,
			   const uint4 image_dim,
			   const COMPUTE_IMAGE_TYPE image_type,
			   void* host_ptr = nullptr,
			   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
			   const uint32_t opengl_type = 0,
			   const uint32_t external_gl_object_ = 0,
			   const opengl_image_info* gl_image_info = nullptr);
	
	~cuda_image() override;
	
	bool acquire_opengl_object(shared_ptr<compute_queue> cqueue) override;
	bool release_opengl_object(shared_ptr<compute_queue> cqueue) override;
	
	void zero(shared_ptr<compute_queue> cqueue) override;
	
	void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags =
											(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
											 COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the cuda specific image pointer (array or mip-mapped array)
	const void* get_cuda_image() const { return image; }
	
	//! returns the cuda surface object
	const cu_surf_object& get_cuda_surface() const { return surface; }
	
	//! returns the cuda texture object
	const auto& get_cuda_textures() const {
		return textures;
	}
	
	//! internal function - initialized once by cuda_compute
	static void init_internal();
	
protected:
	// generic image pointer (identical to either image_array or image_mipmap_array)
	void* image { nullptr };
	cu_array image_array { nullptr };
	cu_mip_mapped_array image_mipmap_array { nullptr };
	cu_graphics_resource rsrc { nullptr };
	uint32_t fixed_depth { 1 }; // max(1, #layers * #cube-faces)
	
	// contains the cu_array for each mip-level
	vector<cu_array> image_mipmap_arrays;
	
	// only need one surface object (only needs to point to image)
	cu_surf_object surface { 0ull };
	// the way cuda reads/samples images must be specified in the host api, which will basically
	// create a combined texture+sampler object -> need to create these for all possible types
	array<cu_tex_only_object, cuda_sampler::max_sampler_count> textures;
	
	struct cuda_mapping {
		const size3 origin;
		const size3 region;
		const COMPUTE_MEMORY_MAP_FLAG flags;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, cuda_mapping> mappings;
	
	// separate create image function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue);
	
	//
	uint32_t depth_compat_tex { 0u };
	uint32_t depth_compat_format { 0u };
	uint32_t depth_copy_fbo { 0u };
	
};

#endif

#endif
