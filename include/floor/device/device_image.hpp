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

#include <floor/device/device_memory.hpp>
#include <floor/device/backend/image_types.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class floor;

class device_context;
class device_program;
class device_function;
class device_fence;

class vulkan_image;
class vulkan_queue;
class metal_image;
class metal_queue;

class device_image : public device_memory {
public:
	//! this sets the r/w flags in a MEMORY_FLAG enum according to the ones in an IMAGE_TYPE enum
	static constexpr MEMORY_FLAG infer_rw_flags(const IMAGE_TYPE& image_type, MEMORY_FLAG flags_) {
		// clear existing r/w flags
		flags_ &= ~MEMORY_FLAG::READ_WRITE;
		// set r/w flags from specified image type
		if (has_flag<IMAGE_TYPE::READ>(image_type)) {
			flags_ |= MEMORY_FLAG::READ;
		}
		if (has_flag<IMAGE_TYPE::WRITE>(image_type)) {
			flags_ |= MEMORY_FLAG::WRITE;
		}
		// mark image as writable if mip-maps need to be generated
		if (has_flag<MEMORY_FLAG::GENERATE_MIP_MAPS>(flags_)) {
			flags_ |= MEMORY_FLAG::WRITE;
		}
		
		// flag as render target memory if the image is a render target
		flags_ &= ~MEMORY_FLAG::RENDER_TARGET;
		if (has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type)) {
			flags_ |= MEMORY_FLAG::RENDER_TARGET;
		}
		
		return flags_;
	}
	
	//! automatically sets/infers image_type flags when certain conditions are met
	static constexpr IMAGE_TYPE infer_image_flags(IMAGE_TYPE image_type) {
		return image_type;
	}
	
	//! handles misc image type modifications (infer no-sampler flag, strip mip-mapped flag if mip-level count <= 1)
	static constexpr IMAGE_TYPE handle_image_type(const uint4& image_dim_, IMAGE_TYPE image_type_, const MEMORY_FLAG flags_) {
		IMAGE_TYPE ret = infer_image_flags(image_type_);
		if (has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type_)) {
			if (image_mip_level_count(image_dim_, image_type_) <= 1) {
				ret &= ~IMAGE_TYPE::FLAG_MIPMAPPED;
			}
		}
		// mark image as writable if mip-maps need to be generated
		if (has_flag<MEMORY_FLAG::GENERATE_MIP_MAPS>(flags_)) {
			ret |= IMAGE_TYPE::WRITE;
		}
		return ret;
	}
	
	~device_image() override = default;
	
	// TODO: read, copy, fill
	// TODO: map with dim size and dim coords/offset
	
	//! blits the "src" image onto this image, returns true on success
	//! NOTE: dim must be identical, format must be compatible
	//! TODO: implement this everywhere
	virtual bool blit(const device_queue& cqueue floor_unused, device_image& src floor_unused) { return false; }
	
	
	//! asynchronously blits the "src" image onto this image, returns true if encoding was successful,
	//! waits for all fences specified in "wait_fences" and signals all fences specified in "signal_fences"
	//! NOTE: dim must be identical, format must be compatible
	//! TODO: implement this everywhere
	virtual bool blit_async(const device_queue& cqueue floor_unused, device_image& src floor_unused,
							std::vector<const device_fence*>&& wait_fences floor_unused,
							std::vector<device_fence*>&& signal_fences floor_unused) { return false; }
	
	//! writes/copies host data from "src" with "src_size" bytes into this image,
	//! at 3D offset/coordinate "offset", with extent/size "extent",
	//! with inclusive "mip_level_range" [start, end] range and inclusive "layer_range" [start, end] range
	//! TODO: implement this everywhere
	virtual bool write(const device_queue& cqueue floor_unused, const void* src floor_unused, const size_t src_size floor_unused,
					   const uint3 offset floor_unused, const uint3 extent floor_unused,
					   const uint2 mip_level_range floor_unused, const uint2 layer_range floor_unused) {
		return false;
	}
	//! writes/copies host data from "src" into this image,
	//! at 3D offset/coordinate "offset", with extent/size "extent",
	//! with inclusive "mip_level_range" [start, end] range and inclusive "layer_range" [start, end] range
	//! TODO: implement this everywhere
	template <typename data_type>
	bool write(const device_queue& cqueue, const std::span<data_type> src,
			   const uint3 offset, const uint3 extent, const uint2 mip_level_range, const uint2 layer_range) {
		return write(cqueue, (const void*)src.data(), src.size_bytes(), offset, extent, mip_level_range, layer_range);
	}
	
	//! maps device memory into host accessible memory,
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void* __attribute__((aligned(128))) map(const device_queue& cqueue,
													const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK)) = 0;
	
	//! maps device memory into host accessible memory,
	//! returning the mapped pointer as an std::array<> of "data_type" with "n" elements
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type, size_t n>
	std::array<data_type, n>* map(const device_queue& cqueue,
								  const MEMORY_MAP_FLAG flags_ = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK)) {
		return (std::array<data_type, n>*)map(cqueue, flags_);
	}
	
	//! unmaps a previously mapped memory pointer, returns true on success
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) = 0;
	
	//! clones this image, optionally copying its contents as well
	//! NOTE: contents can only be copied if the image is READ_WRITE
	//! NOTE: if "image_type_override" is set (not NONE), the cloned image type will be set to this (caller must ensure compatibility!)
	virtual std::shared_ptr<device_image> clone(const device_queue& cqueue, const bool copy_contents = false,
												const MEMORY_FLAG flags_override = MEMORY_FLAG::NONE,
												const IMAGE_TYPE image_type_override = IMAGE_TYPE::NONE);
	
	//! creates the mip-map chain for this image (if not manually generating mip-maps)
	virtual void generate_mip_map_chain(const device_queue& cqueue);
	
	//! for debugging purposes: dump IMAGE_TYPE information into a human-readable string
	static std::string image_type_to_string(const IMAGE_TYPE& type);
	
	//! returns the internal shared Metal image if there is one, returns nullptr otherwise
	metal_image* get_shared_metal_image() {
		return shared_mtl_image;
	}
	
	//! returns the internal shared Metal image if there is one, returns nullptr otherwise
	const metal_image* get_shared_metal_image() const {
		return shared_mtl_image;
	}
	
	//! acquires the associated Metal image for use with compute (-> release from Metal use)
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "mtl_queue" must be a device_queue of the Metal context (or nullptr)
	virtual bool acquire_metal_image(const device_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	//! releases the associated Metal image from use with compute (-> acquire for Metal use)
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "mtl_queue" must be a device_queue of the Metal context (or nullptr)
	virtual bool release_metal_image(const device_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	//! synchronizes the contents of this image with the shared Metal image
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "mtl_queue" must be a device_queue of the Metal context (or nullptr)
	virtual bool sync_metal_image(const device_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	
	//! returns the underlying Metal image that should be used on the device (i.e. this or a shared image)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	metal_image* get_underlying_metal_image_safe();
	
	//! returns the underlying Metal image that should be used on the device (i.e. this or a shared image)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	const metal_image* get_underlying_metal_image_safe() const;
	
	//! returns the internal shared Vulkan image if there is one, returns nullptr otherwise
	vulkan_image* get_shared_vulkan_image() {
		return shared_vk_image;
	}
	
	//! returns the internal shared Vulkan image if there is one, returns nullptr otherwise
	const vulkan_image* get_shared_vulkan_image() const {
		return shared_vk_image;
	}
	
	//! acquires the associated Vulkan image for use with compute (-> release from Vulkan use)
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "vk_queue" must be a device_queue of the Vulkan context (or nullptr)
	virtual bool acquire_vulkan_image(const device_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	//! releases the associated Vulkan image from use with compute (-> acquire for Vulkan use)
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "vk_queue" must be a device_queue of the Vulkan context (or nullptr)
	virtual bool release_vulkan_image(const device_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	//! synchronizes the contents of this image with the shared Vulkan image
	//! NOTE: "cqueue" must be a device_queue of the compute context (or nullptr), "vk_queue" must be a device_queue of the Vulkan context (or nullptr)
	virtual bool sync_vulkan_image(const device_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	
	//! returns the underlying Vulkan image that should be used on the device (i.e. this or a shared image)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	vulkan_image* get_underlying_vulkan_image_safe();
	
	//! returns the underlying Vulkan image that should be used on the device (i.e. this or a shared image)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	const vulkan_image* get_underlying_vulkan_image_safe() const;
	
	//! returns the image type of this image
	const IMAGE_TYPE& get_image_type() const {
		return image_type;
	}
	
	//! returns the image dim variable with which this image has been created
	const uint4& get_image_dim() const {
		return image_dim;
	}
	
	//! returns the image width/height aspect ratio
	float get_aspect_ratio() const {
		return float(image_dim.x) / float(image_dim.y);
	}
	
	//! returns the amount of image layers with which this image has been created
	//! NOTE: this count includes cube map sides (layers)
	const uint32_t& get_layer_count() const {
		return layer_count;
	}
	
	//! returns the data size necessary to store this image in memory
	const size_t& get_image_data_size() const {
		return image_data_size;
	}
	
	//! returns the data size necessary to store this image in memory at the specified mip level
	size_t get_image_data_size_at_mip_level(const uint32_t mip_level) const {
		if (mip_level >= mip_level_count) {
			return 0u;
		}
		return image_mip_level_data_size_from_types(image_dim, image_type, mip_level);
	}
	
	//! returns true if automatic mip-map chain generation is enabled
	bool get_generate_mip_maps() const {
		return generate_mip_maps;
	}
	
	//! returns the dimensionality of this image
	uint32_t get_dim_count() const {
		return image_dim_count(image_type);
	}
	
	//! returns the storage dimensionality of this image
	uint32_t get_storage_dim_count() const {
		return image_storage_dim_count(image_type);
	}
	
	//! returns the channel count of this image
	uint32_t get_channel_count() const {
		return image_channel_count(image_type);
	}
	
	//! returns the format of this image
	IMAGE_TYPE get_format() const {
		return (image_type & IMAGE_TYPE::__FORMAT_MASK);
	}
	
	//! returns the anisotropy of this image
	uint32_t get_anisotropy() const {
		return image_anisotropy(image_type);
	}
	
	//! returns the sample count of this image
	uint32_t get_image_sample_count() const {
		return image_anisotropy(image_type);
	}
	
	//! returns the amount of bits needed to store one pixel
	uint32_t get_bits_per_pixel() const {
		return image_bits_per_pixel(image_type);
	}
	
	//! returns the amount of bytes needed to store one pixel
	uint32_t get_bytes_per_pixel() const {
		return image_bytes_per_pixel(image_type);
	}
	
	//! returns the amount of mip-map levels used by this image
	//! NOTE: #mip-levels from image dim to 1px
	uint32_t get_mip_level_count() const {
		return mip_level_count;
	}
	
	//! returns true if this image is using a compressed image format
	bool is_compressed() const {
		return image_compressed(image_type);
	}
	
	//! returns the 2D block size of the compression method that is being used
	uint2 get_compression_block_size() const {
		return image_compression_block_size(image_type);
	}
	
	//! returns the total amount of bytes needed to store a slice of within this image
	//! (or of the complete image w/o mip levels if it isn't an array or cube image)
	size_t get_slice_data_size() const {
		return image_slice_data_size_from_types(image_dim, image_type);
	}
	
	//! returns true if the image layout is R, RG, RGB or RGBA
	bool is_layout_rgba() const {
		return image_layout_rgba(image_type);
	}
	
	//! returns true if the image layout is ABGR or BGR
	bool is_layout_abgr() const {
		return image_layout_abgr(image_type);
	}
	
	//! returns true if the image layout is BGRA
	bool is_layout_bgra() const {
		return image_layout_bgra(image_type);
	}
	
	//! returns true if the image layout is ARGB
	bool is_layout_argb() const {
		return image_layout_argb(image_type);
	}
	
	//! returns true if this is a 1D image
	constexpr bool is_image_1d() const {
		return fl::is_image_1d(image_type);
	}
	
	//! returns true if this is a 1D image array
	constexpr bool is_image_1d_array() const {
		return fl::is_image_1d_array(image_type);
	}
	
	//! returns true if this is a 1D image buffer
	constexpr bool is_image_1d_buffer() const {
		return fl::is_image_1d_buffer(image_type);
	}
	
	//! returns true if this is a 2D image
	constexpr bool is_image_2d() const {
		return fl::is_image_2d(image_type);
	}
	
	//! returns true if this is a 2D image array
	constexpr bool is_image_2d_array() const {
		return fl::is_image_2d_array(image_type);
	}
	
	//! returns true if this is a 2D MSAA image
	constexpr bool is_image_2d_msaa() const {
		return fl::is_image_2d_msaa(image_type);
	}
	
	//! returns true if this is a 2D MSAA image array
	constexpr bool is_image_2d_msaa_array() const {
		return fl::is_image_2d_msaa_array(image_type);
	}
	
	//! returns true if this is a cube image
	constexpr bool is_image_cube() const {
		return fl::is_image_cube(image_type);
	}
	
	//! returns true if this is a cube image array
	constexpr bool is_image_cube_array() const {
		return fl::is_image_cube_array(image_type);
	}
	
	//! returns true if this is a 2D depth image
	constexpr bool is_image_depth() const {
		return fl::is_image_depth(image_type);
	}
	
	//! returns true if this is a 2D depth/stencil image
	constexpr bool is_image_depth_stencil() const {
		return fl::is_image_depth_stencil(image_type);
	}
	
	//! returns true if this is a 2D depth image array
	constexpr bool is_image_depth_array() const {
		return fl::is_image_depth_array(image_type);
	}
	
	//! returns true if this is a cube depth image
	constexpr bool is_image_depth_cube() const {
		return fl::is_image_depth_cube(image_type);
	}
	
	//! returns true if this is a cube depth image array
	constexpr bool is_image_depth_cube_array() const {
		return fl::is_image_depth_cube_array(image_type);
	}
	
	//! returns true if this is a 2D MSAA depth image
	constexpr bool is_image_depth_msaa() const {
		return fl::is_image_depth_msaa(image_type);
	}
	
	//! returns true if this is a 2D MSAA depth image array
	constexpr bool is_image_depth_msaa_array() const {
		return fl::is_image_depth_msaa_array(image_type);
	}
	
	//! returns true if this is a 3D image
	constexpr bool is_image_3d() const {
		return fl::is_image_3d(image_type);
	}
	
	//! returns true if this is image is read-only (non-writable and not usable as a render target)
	constexpr bool is_image_read_only() const {
		return (!has_flag<IMAGE_TYPE::WRITE>(image_type) &&
				!has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type));
	}
	
	//! for debugging purposes: dumps the content of this image into a file using the specified "value_type" operator<<
	//! NOTE: each value will printed in one line (terminated by \n)
	template <typename value_type>
	bool dump_to_file(const device_queue& cqueue, const std::string& file_name) {
		return device_memory::dump_to_file<value_type>(*this, image_data_size, cqueue, file_name);
	}
	
	//! for debugging purposes: dumps the binary content of this image into a file
	bool dump_binary_to_file(const device_queue& cqueue, const std::string& file_name) {
		return device_memory::dump_binary_to_file(*this, image_data_size, cqueue, file_name);
	}
	
protected:
	friend class floor;
	
	device_image(const device_queue& cqueue,
				  const uint4 image_dim_,
				  const IMAGE_TYPE image_type_,
				  std::span<uint8_t> host_data_,
				  const MEMORY_FLAG flags_,
				  device_image* shared_image_,
				  const bool backend_may_need_shim_type,
				  const uint32_t mip_level_limit) :
	device_memory(cqueue, host_data_, infer_rw_flags(image_type_, flags_)),
	image_dim(image_dim_), image_type(handle_image_type(image_dim_, image_type_, flags)),
	is_mip_mapped(has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type)),
	generate_mip_maps(is_mip_mapped &&
					  has_flag<MEMORY_FLAG::GENERATE_MIP_MAPS>(flags_)),
	mip_level_count(is_mip_mapped ? std::min(uint32_t(image_mip_level_count(image_dim, image_type)),
											 mip_level_limit > 0u ? mip_level_limit : ~0u) : 1u),
	image_data_size(image_data_size_from_types(image_dim, image_type, generate_mip_maps, mip_level_count)),
	layer_count(image_layer_count(image_dim, image_type)),
	shared_image(shared_image_),
	image_data_size_mip_maps(image_data_size_from_types(image_dim, image_type, false)) {
		if (backend_may_need_shim_type) {
			// set shim format to the corresponding 4-channel format
			// compressed images will always be used in their original state, even if they are RGB
			if (image_channel_count(image_type) == 3 && !image_compressed(image_type)) {
				shim_image_type = (image_type & ~IMAGE_TYPE::__CHANNELS_MASK) | IMAGE_TYPE::RGBA;
				shim_image_data_size = image_data_size_from_types(image_dim, shim_image_type, generate_mip_maps);
				shim_image_data_size_mip_maps = image_data_size_from_types(image_dim, shim_image_type, false);
			}
			// == original type if not 3-channel -> 4-channel emulation
			else shim_image_type = image_type;
		}
		
		// can't be both mip-mapped and a render target
		if (has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type) &&
			(has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type) ||
			 has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type))) {
			throw std::runtime_error("image can't be both mip-mapped and a render and/or transient target!");
		}
		// can't be both mip-mapped and a multi-sampled image
		if (has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type) &&
			has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type)) {
			throw std::runtime_error("image can't be both mip-mapped and a multi-sampled image!");
		}
		// writing to compressed formats is not supported anywhere
		if (image_compressed(image_type) && has_flag<IMAGE_TYPE::WRITE>(image_type)) {
			throw std::runtime_error("image can not be compressed and writable!");
		}
		// TODO: make sure format is supported, fail early if not
		if (!image_format_valid(image_type)) {
			throw std::runtime_error("invalid image format: " + std::to_string(std::underlying_type_t<IMAGE_TYPE>(image_type)));
		}
		// can't generate compressed mip-levels right now
		if (image_compressed(image_type) && generate_mip_maps) {
			throw std::runtime_error("generating mip-maps for compressed image data is not supported!");
		}
		// can't generate mip-levels for transient images
		if (has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type) && generate_mip_maps) {
			throw std::runtime_error("generating mip-maps for a transient image is not supported!");
		}
		// warn about missing sharing flag if shared image is set
		if (shared_image != nullptr) {
			if (!has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags) && !has_flag<MEMORY_FLAG::METAL_SHARING>(flags)) {
				log_warn("provided a shared image, but no sharing flag is set");
			}
		}
		// if there is host data, it must have at least the same size as the image
		if (host_data.data() != nullptr && host_data.size_bytes() < image_data_size) {
			throw std::runtime_error("image host data size " + std::to_string(host_data.size_bytes()) +
									 " is smaller than the expected image size " + std::to_string(image_data_size));
		}
	}
	
	const uint4 image_dim;
	const IMAGE_TYPE image_type;
	const bool is_mip_mapped;
	const bool generate_mip_maps;
	const uint32_t mip_level_count;
	const size_t image_data_size;
	const uint32_t layer_count;
	
	// NOTE: only one of these can be active at a time
	union {
		device_image* shared_image { nullptr };
		// shared Vulkan image object when Vulkan sharing is used
		vulkan_image* shared_vk_image;
		// shared Metal image object when Metal sharing is used
		metal_image* shared_mtl_image;
	};
	
	// for use with 3-channel image "emulation" through a corresponding 4-channel image
	IMAGE_TYPE shim_image_type { IMAGE_TYPE::NONE };
	size_t shim_image_data_size { 0 };
	
	// when automatically generating mip-maps, we also need to store all mip-maps
	// manually (thus != image_data_size), otherwise this is equal to image_data_size
	const size_t image_data_size_mip_maps;
	size_t shim_image_data_size_mip_maps { 0 };
	
	//! converts RGB data to RGBA data and returns the owning RGBA image data pointer
	std::pair<std::unique_ptr<uint8_t[]>, size_t> rgb_to_rgba(const IMAGE_TYPE& rgb_type,
															  const IMAGE_TYPE& rgba_type,
															  const std::span<const uint8_t> rgb_data,
															  const bool ignore_mip_levels = false);
	
	//! in-place converts RGB data to RGBA data
	//! NOTE: 'rgb_to_rgba_data' must point to sufficient enough memory that can hold the RGBA data
	void rgb_to_rgba_inplace(const IMAGE_TYPE& rgb_type,
							 const IMAGE_TYPE& rgba_type,
							 std::span<uint8_t> rgb_to_rgba_data,
							 const bool ignore_mip_levels = false);
	
	//! converts RGBA data to RGB data. if "dst_rgb_data" is non-null, the RGB data is directly written to it and no memory is
	//! allocated and nullptr is returned. otherwise RGB image data is allocated and an owning pointer to it is returned.
	std::pair<std::unique_ptr<uint8_t[]>, size_t> rgba_to_rgb(const IMAGE_TYPE& rgba_type,
															  const IMAGE_TYPE& rgb_type,
															  const std::span<const uint8_t> rgba_data,
															  std::span<uint8_t> dst_rgb_data = {},
															  const bool ignore_mip_levels = false);
	
	// calls function "F" with (level, mip image dim, slice data size, mip-level size, args...) for each level of
	// the mip-map chain or only the single level of a non-mip-mapped image.
	// if function "F" returns false, this will immediately returns false; returns true otherwise
	// if "all_levels" is true, ignore the "generate_mip_maps" and run this on all mip-levels
	template <bool all_levels = false, typename F>
	bool apply_on_levels(F&& func, IMAGE_TYPE override_image_type = IMAGE_TYPE::NONE) const {
		const IMAGE_TYPE mip_image_type = (override_image_type != IMAGE_TYPE::NONE ?
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
		for (uint32_t level = 0; level < handled_level_count; ++level, mip_image_dim >>= 1) {
			const auto slice_data_size = (uint32_t)image_slice_data_size_from_types(mip_image_dim, mip_image_type);
			const auto level_data_size = (uint32_t)(slice_data_size * slice_count);
			if (!std::forward<F>(func)(level, mip_image_dim, slice_data_size, level_data_size)) {
				return false;
			}
		}
		return true;
	}
	
	// mip-map minification program handling (for backends that need it)
	struct minify_program {
		std::shared_ptr<device_program> program;
		std::unordered_map<IMAGE_TYPE, std::pair<std::string, std::shared_ptr<device_function>>> functions;
	};
	static safe_mutex minify_programs_mtx;
	static std::unordered_map<device_context*, std::unique_ptr<minify_program>> minify_programs GUARDED_BY(minify_programs_mtx);
	static void destroy_minify_programs();
	
	//! builds the mip-map minification program for this context and its devices
	//! NOTE: will only build once automatic mip-map chain generation is being used/requested
	void build_mip_map_minification_program() const;
	
	//! this is used to provide the underlying minify program,
	//! returns true on success
	static bool provide_minify_program(device_context& ctx, std::shared_ptr<device_program> prog);
	
	//! adds the embedded mip-map minify FUBAR program if one is available and compatible to "ctx",
	//! returns true on success
	bool add_embedded_minify_program(device_context& ctx);
	
	//! returns true if "src" can be blitted onto this image, false if not (prints errors)
	bool blit_check(const device_queue& cqueue, const device_image& src);
	
	//! returns true if host data can be written into this image using the specified parameters, false if not (prints errors)
	//! NOTE: in some situations, the presence of MEMORY_FLAG::HOST_WRITE may not be required -> "needs_host_write" can be set to false then
	bool write_check(const size_t src_size, const uint3 offset, const uint3 extent, const uint2 mip_level_range, const uint2 layer_range,
					 const bool needs_host_write = true);
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
