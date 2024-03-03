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

#include <floor/compute/host/host_image.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>
#include <floor/floor/floor.hpp>
#include <floor/constexpr/const_string.hpp>

#if defined(FLOOR_DEBUG)
static constexpr const size_t protection_size { 1024u };
static constexpr const uint8_t protection_byte { 0xA5 };
#else
static constexpr const size_t protection_size { 0u };
#endif

host_image::host_image(const compute_queue& cqueue,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   std::span<uint8_t> host_data_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info,
					   compute_image* shared_image_) :
compute_image(cqueue, image_dim_, image_type_, host_data_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info, shared_image_, false) {
	// check Metal/Vulkan buffer sharing validity
#if defined(FLOOR_NO_METAL)
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		throw runtime_error("Metal support is not enabled");
	}
#endif
#if defined(FLOOR_NO_VULKAN)
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		throw runtime_error("Vulkan support is not enabled");
	}
#endif
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags) && has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		throw runtime_error("can now have both Metal and Vulkan sharing enabled");
	}
	
	// actually create the image
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool host_image::create_internal(const bool copy_host_data, const compute_queue& cqueue) {
	image = make_aligned_ptr<uint8_t>(image_data_size_mip_maps + protection_size);
	program_info.buffer = image.get();
	program_info.runtime_image_type = image_type;
	
	const auto dim_count = image_dim_count(image_type);
	uint4 mip_image_dim {
		image_dim.x,
		dim_count >= 2 ? image_dim.y : 0,
		dim_count >= 3 ? image_dim.z : 0,
		0
	};
	uint32_t level_offset = 0;
	for(size_t level = 0; level < host_limits::max_mip_levels; ++level, mip_image_dim >>= 1) {
		program_info.level_info[level].dim = mip_image_dim;
		
		const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type);
		const auto level_data_size = slice_data_size * layer_count;
		program_info.level_info[level].offset = level_offset;
		level_offset += level_data_size;
		
		program_info.level_info[level].clamp_dim_int = {
			mip_image_dim.x > 0 ? int(mip_image_dim.x - 1) : 0,
			mip_image_dim.y > 0 ? int(mip_image_dim.y - 1) : 0,
			mip_image_dim.z > 0 ? int(mip_image_dim.z - 1) : 0,
			0
		};
		program_info.level_info[level].clamp_dim_float = {
			mip_image_dim.x > 0 ? float(mip_image_dim.x) : 0.0f,
			mip_image_dim.y > 0 ? float(mip_image_dim.y) : 0.0f,
			mip_image_dim.z > 0 ? float(mip_image_dim.z) : 0.0f,
			0.0f
		};
		program_info.level_info[level].clamp_dim_float_excl = {
			mip_image_dim.x > 0 ? std::nextafterf(float(mip_image_dim.x), 0.0f) : 0.0f,
			mip_image_dim.y > 0 ? std::nextafterf(float(mip_image_dim.y), 0.0f) : 0.0f,
			mip_image_dim.z > 0 ? std::nextafterf(float(mip_image_dim.z), 0.0f) : 0.0f,
			0.0f
		};
	}
	
#if defined(FLOOR_DEBUG)
	// set protection bytes
	memset(image.get() + image_data_size_mip_maps, protection_byte, protection_size);
#endif
	
	// -> normal host image
	if (!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags) &&
		!has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags) &&
		!has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		// copy host memory to "device" if it is non-null and NO_INITIAL_COPY is not specified
		if (copy_host_data &&
			host_data.data() != nullptr &&
			!has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			memcpy(image.get(), host_data.data(),
				   // if mip-maps have to be created on the libfloor side (i.e. not provided by the user),
				   // only copy the data that is actually provided by the user
				   generate_mip_maps ? image_data_size : image_data_size_mip_maps);
			
			// manually create mip-map chain
			if(generate_mip_maps) {
				generate_mip_map_chain(cqueue);
			}
		}
	}
#if !defined(FLOOR_NO_METAL)
	// -> shared host/Metal image
	else if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		if (!create_shared_image(copy_host_data)) {
			return false;
		}
		
		// acquire for use with the host
		const compute_queue* comp_mtl_queue = get_default_queue_for_memory(*shared_image);
		acquire_metal_image(&cqueue, (const metal_queue*)comp_mtl_queue);
	}
#endif
#if !defined(FLOOR_NO_VULKAN)
	// -> shared host/Vulkan image
	else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (!create_shared_image(copy_host_data)) {
			return false;
		}
		
		// acquire for use with the host
		const compute_queue* comp_vk_queue = get_default_queue_for_memory(*shared_image);
		acquire_vulkan_image(&cqueue, (const vulkan_queue*)comp_vk_queue);
	}
#endif
	// -> shared host/OpenGL image
	else {
#if !defined(FLOOR_IOS)
		if (!create_gl_image(copy_host_data)) {
			return false;
		}
#endif
		
		// acquire for use with the host
		acquire_opengl_object(&cqueue);
	}
	
	return true;
}

host_image::~host_image() {
	// first, release and kill the opengl image
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("image still registered for opengl use - acquire before destructing a compute image!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
#if !defined(FLOOR_IOS)
		delete_gl_image();
#endif
	}
}

bool host_image::zero(const compute_queue& cqueue) {
	if (!image) return false;
	
	cqueue.finish();
	memset(image.get(), 0, image_data_size_mip_maps);
	return true;
}

void* __attribute__((aligned(128))) host_image::map(const compute_queue& cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if (!image) return nullptr;
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(blocking_map) {
		cqueue.finish();
	}
	return image.get();
}

bool host_image::unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if (!image) return false;
	if (mapped_ptr == nullptr) return false;
	
	// manually create mip-map chain
	if(generate_mip_maps) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

bool host_image::acquire_opengl_object(const compute_queue* cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// copy gl image data to host memory (if read access is set)
	const auto dim_count = image_dim_count(image_type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	int4 mip_image_dim {
		(int)image_dim.x,
		dim_count >= 2 ? (int)image_dim.y : 0,
		dim_count >= 3 ? (int)image_dim.z : 0,
		0
	};
	if(has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) {
		glBindTexture(opengl_type, gl_object);
		const uint8_t* level_data = image.get();
		for(size_t level = 0; level < mip_level_count; ++level, mip_image_dim >>= 1) {
			const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type);
			const auto level_data_size = slice_data_size * layer_count;
			
			if(!is_cube ||
			   // contrary to GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY can be copied directly
			   (is_cube && is_array)) {
				glGetTexImage(opengl_type, (int)level, gl_format, gl_type, (void*)level_data);
			}
			else {
				// must copy cube faces individually
				for(uint32_t i = 0; i < 6; ++i) {
					glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, (int)level, gl_format, gl_type, (void*)level_data);
					level_data += slice_data_size;
				}
			}
			
			level_data += level_data_size;
		}
		glBindTexture(opengl_type, 0);
		
#if defined(FLOOR_DEBUG)
		// check memory protection strip
		for(size_t i = 0; i < protection_size; ++i) {
			if(*(image.get() + image_data_size_mip_maps + i) != protection_byte) {
				log_error("DO PANIC: opengl wrote too many bytes: image: $X, gl object: $, @ protection byte #$, expected data size $",
						  this, gl_object, i, image_data_size_mip_maps);
				logger::flush();
				break;
			}
		}
#endif
	}
	
	gl_object_state = false;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

bool host_image::release_opengl_object(const compute_queue* cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if (!image) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been released for opengl use!");
#endif
		return true;
	}
	
	// copy the host data back to the gl image (if write access is set)
	if(has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		glBindTexture(opengl_type, gl_object);
		update_gl_image_data(image.get());
		glBindTexture(opengl_type, 0);
	}
	
	gl_object_state = true;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

static bool needs_sync_to_host(const COMPUTE_MEMORY_FLAG& flags) {
	return (!has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags) ||
			(has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags) &&
			 has_flag<COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_READ>(flags) &&
			 has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_WRITE>(flags)));
}

static bool needs_sync_from_host(const COMPUTE_MEMORY_FLAG& flags) {
	return (!has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags) ||
			(has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags) &&
			 has_flag<COMPUTE_MEMORY_FLAG::SHARING_COMPUTE_WRITE>(flags) &&
			 has_flag<COMPUTE_MEMORY_FLAG::SHARING_RENDER_READ>(flags)));
}

template <auto backend_name, typename backend_queue_type, typename shared_image_type>
static inline bool acquire_sync_image(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									  aligned_ptr<uint8_t>& image, shared_image_type* shared_image_ptr,
									  bool& shared_object_state, const size_t& image_data_size,
									  const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_image_ptr || !image) {
		return false;
	}
	if (!shared_object_state) {
		// -> image has already been acquired for use with Host-Compute
		return true;
	}
	
	if (needs_sync_to_host(flags)) {
		const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
#if defined(FLOOR_DEBUG)
		// validate host queue
		if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
			log_error("specified queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		auto shared_image_base_ptr = (compute_image*)shared_image_ptr;
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_image_base_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// read/copy shared image data to host memory
		auto img_data = shared_image_base_ptr->map(*comp_rqueue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		memcpy(image.get(), img_data, image_data_size);
		shared_image_base_ptr->unmap(*comp_rqueue, img_data);
		
		// finish read
		comp_rqueue->finish();
	}
	
	shared_object_state = false;
	return true;
}

template <auto backend_name, typename backend_queue_type, typename shared_image_type>
static inline bool release_sync_image(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									  const aligned_ptr<uint8_t>& image, shared_image_type* shared_image_ptr,
									  bool& shared_object_state, const size_t& image_data_size,
									  const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_image_ptr || !image) {
		return false;
	}
	if (shared_object_state) {
		// -> image has already been released for use with the render backend
		return true;
	}
	
	if (needs_sync_from_host(flags)) {
		const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
#if defined(FLOOR_DEBUG)
		// validate host queue
		if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
			log_error("specified queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		auto shared_image_base_ptr = (compute_image*)shared_image_ptr;
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_image_base_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// write/copy the host data to the shared image
		auto img_data = shared_image_base_ptr->map(*comp_rqueue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		memcpy(img_data, image.get(), image_data_size);
		shared_image_base_ptr->unmap(*comp_rqueue, img_data);
		
		// finish write
		comp_rqueue->finish();
	}
	
	shared_object_state = true;
	return true;
}

template <auto backend_name, typename backend_queue_type, typename shared_image_type>
static inline bool sync_shared_image(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									 const aligned_ptr<uint8_t>& image, shared_image_type* shared_image_ptr,
									 const bool& shared_object_state, const size_t& image_data_size,
									 const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_image_ptr || !image) {
		return false;
	}
	if (shared_object_state) {
		// no need, already acquired for shared/render backend use
		return true;
	}
	
	if (needs_sync_from_host(flags)) {
		const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
#if defined(FLOOR_DEBUG)
		// validate host queue
		if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
			log_error("specified queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		auto shared_image_base_ptr = (compute_image*)shared_image_ptr;
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_image_base_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// write/copy the host data to the shared image
		auto img_data = shared_image_base_ptr->map(*comp_rqueue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		memcpy(img_data, image.get(), image_data_size);
		shared_image_base_ptr->unmap(*comp_rqueue, img_data);
		
		// finish write
		comp_rqueue->finish();
	}
	
	return true;
}

#if !defined(FLOOR_NO_METAL)
bool host_image::acquire_metal_image(const compute_queue* cqueue, const metal_queue* mtl_queue) const {
	return acquire_sync_image<"Metal"_cs>(cqueue, mtl_queue, dev, image, shared_mtl_image, mtl_object_state, image_data_size, flags);
}

bool host_image::release_metal_image(const compute_queue* cqueue, const metal_queue* mtl_queue) const {
	return release_sync_image<"Metal"_cs>(cqueue, mtl_queue, dev, image, shared_mtl_image, mtl_object_state, image_data_size, flags);
}

bool host_image::sync_metal_image(const compute_queue* cqueue_, const metal_queue* mtl_queue_) const {
	return sync_shared_image<"Metal"_cs>(cqueue_, mtl_queue_, dev, image, shared_mtl_image, mtl_object_state, image_data_size, flags);
}
#else
bool host_image::acquire_metal_image(const compute_queue*, const metal_queue*) const {
	return false;
}
bool host_image::release_metal_image(const compute_queue*, const metal_queue*) const {
	return false;
}
bool host_image::sync_metal_image(const compute_queue*, const metal_queue*) const {
	return false;
}
#endif

#if !defined(FLOOR_NO_VULKAN)
bool host_image::acquire_vulkan_image(const compute_queue* cqueue, const vulkan_queue* vk_queue) const {
	return acquire_sync_image<"Vulkan"_cs>(cqueue, vk_queue, dev, image, shared_vk_image, vk_object_state, image_data_size, flags);
}

bool host_image::release_vulkan_image(const compute_queue* cqueue, const vulkan_queue* vk_queue) const {
	return release_sync_image<"Vulkan"_cs>(cqueue, vk_queue, dev, image, shared_vk_image, vk_object_state, image_data_size, flags);
}

bool host_image::sync_vulkan_image(const compute_queue* cqueue_, const vulkan_queue* vk_queue_) const {
	return sync_shared_image<"Vulkan"_cs>(cqueue_, vk_queue_, dev, image, shared_vk_image, vk_object_state, image_data_size, flags);
}
#else
bool host_image::acquire_vulkan_image(const compute_queue*, const vulkan_queue*) const {
	return false;
}
bool host_image::release_vulkan_image(const compute_queue*, const vulkan_queue*) const {
	return false;
}
bool host_image::sync_vulkan_image(const compute_queue*, const vulkan_queue*) const {
	return false;
}
#endif

bool host_image::create_shared_image(const bool copy_host_data) {
	if (shared_image != nullptr && host_shared_image == nullptr) {
		// wrapping an existing Metal/Vulkan image
		return true;
	}
	
	// get the render/graphics context so that we can create an image (TODO: allow specifying a different context?)
	auto render_ctx = floor::get_render_context();
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		if (render_ctx->get_compute_type() != COMPUTE_TYPE::METAL) {
			log_error("Host/Metal image sharing failed: render context is not Metal");
			return false;
		}
	} else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (render_ctx->get_compute_type() != COMPUTE_TYPE::VULKAN) {
			log_error("Host/Vulkan image sharing failed: render context is not Vulkan");
			return false;
		}
	}
	
	// get the device and its default queue where we want to create the image on/in
	// NOTE: we never have a corresponding device here, so simply use the fastest device
	const compute_device* render_dev = render_ctx->get_device(compute_device::TYPE::FASTEST);
	if (render_dev == nullptr) {
		log_error("Host <-> Metal/Vulkan image sharing failed: failed to find a Metal/Vulkan device");
		return false;
	}
	
	// create the underlying Metal/Vulkan image
	auto default_queue = render_ctx->get_device_default_queue(*render_dev);
	auto shared_image_flags = compute_memory::make_host_shared_memory_flags(flags, *render_dev, copy_host_data);
	host_shared_image = render_ctx->create_image(*default_queue, image_dim, image_type, host_data, shared_image_flags);
	host_shared_image->set_debug_label("host_shared_image");
	if (!host_shared_image) {
		log_error("Host <-> Metal/Vulkan image sharing failed: failed to create the underlying shared Metal/Vulkan image");
		return false;
	}
	shared_image = host_shared_image.get();
	
	return true;
}

void* host_image::get_host_image_program_info_with_sync() const {
#if !defined(FLOOR_NO_METAL) || !defined(FLOOR_NO_VULKAN)
	if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
#if !defined(FLOOR_NO_METAL)
		if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
			// -> acquire for compute use, release from Metal use
			acquire_metal_image(nullptr, nullptr);
		}
#endif
#if !defined(FLOOR_NO_VULKAN)
		if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
			// -> acquire for compute use, release from Vulkan use
			acquire_vulkan_image(nullptr, nullptr);
		}
#endif
	}
#endif
	return (void*)&program_info;
}

#endif
