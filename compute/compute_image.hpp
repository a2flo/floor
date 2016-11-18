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

#ifndef __FLOOR_COMPUTE_IMAGE_HPP__
#define __FLOOR_COMPUTE_IMAGE_HPP__

#include <floor/compute/compute_memory.hpp>
#include <floor/compute/device/image_types.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_context;
class compute_program;
class compute_kernel;
class compute_image : public compute_memory {
public:
	struct opengl_image_info;
	
	//! this sets the r/w flags in a COMPUTE_MEMORY_FLAG enum according to the ones in an COMPUTE_IMAGE_TYPE enum
	static constexpr COMPUTE_MEMORY_FLAG infer_rw_flags(const COMPUTE_IMAGE_TYPE& image_type, COMPUTE_MEMORY_FLAG flags) {
		// clear existing r/w flags
		flags &= ~COMPUTE_MEMORY_FLAG::READ_WRITE;
		// set r/w flags from specified image type
		if(has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type)) flags |= COMPUTE_MEMORY_FLAG::READ;
		if(has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type)) flags |= COMPUTE_MEMORY_FLAG::WRITE;
		return flags;
	}
	
	//! automatically sets/infers image_type flags when certain conditions are met
	static constexpr COMPUTE_IMAGE_TYPE infer_image_flags(COMPUTE_IMAGE_TYPE image_type) {
		return image_type;
	}
	
	//! handles misc image type modifications (infer no-sampler flag, strip mip-mapped flag if mip-level count <= 1)
	static constexpr COMPUTE_IMAGE_TYPE handle_image_type(const uint4& image_dim_, COMPUTE_IMAGE_TYPE image_type_) {
		COMPUTE_IMAGE_TYPE ret = infer_image_flags(image_type_);
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type_)) {
			if(image_mip_level_count(image_dim_, image_type_) <= 1) {
				ret &= ~COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED;
			}
		}
		return ret;
	}
	
	// TODO: anisotropic value (must be present at init, or need to recreate image later)
	compute_image(const compute_device* device,
				  const uint4 image_dim_,
				  const COMPUTE_IMAGE_TYPE image_type_,
				  void* host_ptr_ = nullptr,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0,
				  const uint32_t external_gl_object_ = 0,
				  const opengl_image_info* gl_image_info = nullptr) :
	compute_memory(device, host_ptr_, infer_rw_flags(image_type_, flags_), opengl_type_, external_gl_object_),
	image_dim(image_dim_), image_type(handle_image_type(image_dim_, image_type_)),
	is_mip_mapped(has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type)),
	generate_mip_maps(is_mip_mapped &&
					  has_flag<COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS>(flags_)),
	image_data_size(image_data_size_from_types(image_dim, image_type, 1, generate_mip_maps)),
	mip_level_count(is_mip_mapped ? (uint32_t)image_mip_level_count(image_dim, image_type) : 1u),
	layer_count(image_layer_count(image_dim, image_type)),
	gl_internal_format(gl_image_info != nullptr ? gl_image_info->gl_internal_format : 0),
	gl_format(gl_image_info != nullptr ? gl_image_info->gl_format : 0),
	gl_type(gl_image_info != nullptr ? gl_image_info->gl_type : 0),
	image_data_size_mip_maps(image_data_size_from_types(image_dim, image_type, 1, false)) {
		// can't be both mip-mapped and a multi-sampled image
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type) &&
		   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
			log_error("image can't be both mip-mapped and a multi-sampled image!");
			return;
		}
		// writing to compressed formats is not supported anywhere
		if(image_compressed(image_type) && has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type)) {
			log_error("image can not be compressed and writable!");
			return;
		}
		// TODO: make sure format is supported, fail early if not
		if(!image_format_valid(image_type)) {
			log_error("invalid image format: %X", image_type);
			return;
		}
		// can't generate compressed mip-levels right now
		if(image_compressed(image_type) && generate_mip_maps) {
			log_error("generating mip-maps for compressed image data is not supported!");
			return;
		}
		// TODO: if opengl_type is 0 and opengl sharing is enabled, try guessing it, otherwise fail
	}
	
	virtual ~compute_image() = 0;
	
	// TODO: read, write, copy, fill
	// TODO: map with dim size and dim coords/offset
	
	//! maps device memory into host accessible memory,
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags =
													(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
													 COMPUTE_MEMORY_MAP_FLAG::BLOCK)) = 0;
	
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
	
	//! return struct of get_opengl_image_info
	struct opengl_image_info {
		uint4 image_dim;
		COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
		int32_t gl_internal_format { 0 };
		uint32_t gl_format { 0 };
		uint32_t gl_type { 0 };
		bool valid { false };
	};
	//! helper function to retrieve information from a pre-existing opengl image
	static opengl_image_info get_opengl_image_info(const uint32_t& opengl_image,
												   const uint32_t& opengl_target,
												   const COMPUTE_MEMORY_FLAG& flags);
	
	//! returns the image dim variable with which this image has been created
	const uint4& get_image_dim() const {
		return image_dim;
	}
	
	//! returns the amount of image layers with which this image has been created
	//! NOTE: this count includes cube map sides (layers)
	const uint32_t& get_layer_count() const {
		return layer_count;
	}
	
	//! returns the compute image type of this image
	const COMPUTE_IMAGE_TYPE& get_image_type() const {
		return image_type;
	}
	
	//! returns the data size necessary to store this image in memory
	const size_t& get_image_data_size() const {
		return image_data_size;
	}
	
	//! returns true if automatic mip-map chain generation is enabled
	bool get_generate_mip_maps() const {
		return generate_mip_maps;
	}
	
	//! for debugging purposes: dump COMPUTE_IMAGE_TYPE information into a human-readable string
	static string image_type_to_string(const COMPUTE_IMAGE_TYPE& type);
	
protected:
	const uint4 image_dim;
	const COMPUTE_IMAGE_TYPE image_type;
	const bool is_mip_mapped;
	const bool generate_mip_maps;
	const size_t image_data_size;
	const uint32_t mip_level_count;
	const uint32_t layer_count;
	
#if !defined(FLOOR_IOS)
	// internal function to create/delete an opengl image if compute/opengl sharing is used
	bool create_gl_image(const bool copy_host_data);
	void delete_gl_image();
	
	// helper functions to init/update opengl image data (handles all types)
	void init_gl_image_data(const void* data);
	void update_gl_image_data(const void* data);
#endif
	
	// track gl format types (these are set after a gl texture has been created/wrapped)
	int32_t gl_internal_format { 0 };
	uint32_t gl_format { 0u };
	uint32_t gl_type { 0u };
	
	// for use with 3-channel image "emulation" through a corresponding 4-channel image
	void set_shim_type_info();
	COMPUTE_IMAGE_TYPE shim_image_type;
	size_t shim_image_data_size { 0 };
	
	// when automatically generating mip-maps, we also need to store all mip-maps
	// manually (thus != image_data_size), otherwise this is equal to image_data_size
	const size_t image_data_size_mip_maps;
	size_t shim_image_data_size_mip_maps { 0 };
	
	//! converts RGB data to RGBA data and returns the owning RGBA image data pointer
	uint8_t* rgb_to_rgba(const COMPUTE_IMAGE_TYPE& rgb_type,
						 const COMPUTE_IMAGE_TYPE& rgba_type,
						 const uint8_t* rgb_data,
						 const bool ignore_mip_levels = false);
	
	//! in-place converts RGB data to RGBA data
	//! NOTE: 'rgb_to_rgba_data' must point to sufficient enough memory that can hold the RGBA data
	void rgb_to_rgba_inplace(const COMPUTE_IMAGE_TYPE& rgb_type,
							 const COMPUTE_IMAGE_TYPE& rgba_type,
							 uint8_t* rgb_to_rgba_data,
							 const bool ignore_mip_levels = false);
	
	//! converts RGBA data to RGB data. if "dst_rgb_data" is non-null, the RGB data is directly written to it and no memory is
	//! allocated and nullptr is returned. otherwise RGB image data is allocated and an owning pointer to it is returned.
	uint8_t* rgba_to_rgb(const COMPUTE_IMAGE_TYPE& rgba_type,
						 const COMPUTE_IMAGE_TYPE& rgb_type,
						 const uint8_t* rgba_data,
						 uint8_t* dst_rgb_data = nullptr,
						 const bool ignore_mip_levels = false);
	
	// calls function "F" with (level, mip image dim, slice data size, mip-level size, args...) for each level of
	// the mip-map chain or only the single level of a non-mip-mapped image.
	// if function "F" returns false, this will immediately returns false; returns true otherwise
	// if "all_levels" is true, ignore the "generate_mip_maps" and run this on all mip-levels
	template <bool all_levels = false, typename F>
	bool apply_on_levels(F&& func, COMPUTE_IMAGE_TYPE override_image_type = COMPUTE_IMAGE_TYPE::NONE) {
		const COMPUTE_IMAGE_TYPE mip_image_type = (override_image_type != COMPUTE_IMAGE_TYPE::NONE ?
												   override_image_type : image_type);
		const auto dim_count = image_dim_count(mip_image_type);
		const auto slice_count = image_layer_count(image_dim, mip_image_type);
		const auto handled_level_count = (generate_mip_maps && !all_levels ? 1 : mip_level_count);
		uint4 mip_image_dim {
			image_dim.x,
			dim_count >= 2 ? image_dim.y : 0,
			dim_count >= 3 ? image_dim.z : 0,
			0
		};
		for(uint32_t level = 0; level < handled_level_count; ++level, mip_image_dim >>= 1) {
			const auto slice_data_size = (uint32_t)image_slice_data_size_from_types(mip_image_dim, mip_image_type, 1);
			const auto level_data_size = (uint32_t)(slice_data_size * slice_count);
			if(!std::forward<F>(func)(level, mip_image_dim, slice_data_size, level_data_size)) {
				return false;
			}
		}
		return true;
	}
	
	// mip-map minification program handling (for backends that need it)
	struct minify_program {
		shared_ptr<compute_program> program;
		unordered_map<COMPUTE_IMAGE_TYPE, pair<string, shared_ptr<compute_kernel>>> kernels;
	};
	static safe_mutex minify_programs_mtx;
	static unordered_map<compute_context*, unique_ptr<minify_program>> minify_programs GUARDED_BY(minify_programs_mtx);
	
	//! builds the mip-map minification program for this context and its devices
	//! NOTE: will only build once automatic mip-map chain generation is being used/requested
	void build_mip_map_minification_program() const;
	
	//! creates the mip-map chain for this image (if not using opengl and not manually generating mip-maps)
	virtual void generate_mip_map_chain(shared_ptr<compute_queue> cqueue) const;
	
};

FLOOR_POP_WARNINGS()

#endif
