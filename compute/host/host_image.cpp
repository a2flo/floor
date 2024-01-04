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
		const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type);
		const auto level_data_size = slice_data_size * layer_count;
		program_info.level_info[level].offset = level_offset;
		level_offset += level_data_size;
		
		program_info.level_info[level].dim = mip_image_dim;
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
		acquire_metal_image(cqueue, (const metal_queue&)*comp_mtl_queue);
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
		acquire_vulkan_image(cqueue, (const vulkan_queue&)*comp_vk_queue);
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

#if !defined(FLOOR_NO_METAL)
bool host_image::acquire_metal_image(const compute_queue& cqueue, const metal_queue& mtl_queue) {
	if (shared_mtl_image == nullptr) return false;
	if (!mtl_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Metal image has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(&cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif
	
	const auto& comp_mtl_queue = (const compute_queue&)mtl_queue;
	
	// full sync
	cqueue.finish();
	comp_mtl_queue.finish();
	
	// read/copy Metal image data to host memory
	auto img_data = shared_image->map(comp_mtl_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(image.get(), img_data, image_data_size);
	shared_image->unmap(comp_mtl_queue, img_data);
	
	// finish read
	comp_mtl_queue.finish();
	
	mtl_object_state = false;
	return true;
}

bool host_image::release_metal_image(const compute_queue& cqueue, const metal_queue& mtl_queue) {
	if (shared_mtl_image == nullptr) return false;
	if (!image) return false;
	if (mtl_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Metal image has already been released for Metal use!");
#endif
		return true;
	}
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(&cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif

	const auto& comp_mtl_queue = (const compute_queue&)mtl_queue;
	
	// full sync
	cqueue.finish();
	comp_mtl_queue.finish();
	
	// write/copy the host data to the Metal image
	auto img_data = shared_image->map(comp_mtl_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(img_data, image.get(), image_data_size);
	shared_image->unmap(comp_mtl_queue, img_data);
	
	// finish write
	comp_mtl_queue.finish();
	
	mtl_object_state = true;
	return true;
}

bool host_image::sync_metal_image(const compute_queue* cqueue_, const metal_queue* mtl_queue_) const {
	if (shared_mtl_image == nullptr) return false;
	if (!image) return false;
	if (mtl_object_state) {
		// no need, already acquired for Metal use
		return true;
	}
	
	const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
	const auto comp_mtl_queue = (mtl_queue_ != nullptr ? (const compute_queue*)mtl_queue_ : get_default_queue_for_memory(*shared_image));
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif

	// full sync
	cqueue->finish();
	comp_mtl_queue->finish();
	
	// write/copy the host data to the Metal image
	auto img_data = shared_image->map(*comp_mtl_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(img_data, image.get(), image_data_size);
	shared_image->unmap(*comp_mtl_queue, img_data);
	
	// finish write
	comp_mtl_queue->finish();
	
	return true;
}
#else
bool host_image::acquire_metal_image(const compute_queue&, const metal_queue&) {
	return false;
}
bool host_image::release_metal_image(const compute_queue&, const metal_queue&) {
	return false;
}
bool host_image::sync_metal_image(const compute_queue*, const metal_queue*) const {
	return false;
}
#endif

#if !defined(FLOOR_NO_VULKAN)
bool host_image::acquire_vulkan_image(const compute_queue& cqueue, const vulkan_queue& vk_queue) {
	if (shared_vk_image == nullptr) return false;
	if (!vk_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Vulkan image has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(&cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif
	
	const auto& comp_vk_queue = (const compute_queue&)vk_queue;
	
	// full sync
	cqueue.finish();
	comp_vk_queue.finish();
	
	// read/copy Vulkan image data to host memory
	auto img_data = shared_image->map(comp_vk_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(image.get(), img_data, image_data_size);
	shared_image->unmap(comp_vk_queue, img_data);
	
	// finish read
	comp_vk_queue.finish();
	
	vk_object_state = false;
	return true;
}

bool host_image::release_vulkan_image(const compute_queue& cqueue, const vulkan_queue& vk_queue) {
	if (shared_vk_image == nullptr) return false;
	if (!image) return false;
	if (vk_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Vulkan image has already been released for Vulkan use!");
#endif
		return true;
	}
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(&cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif

	const auto& comp_vk_queue = (const compute_queue&)vk_queue;
	
	// full sync
	cqueue.finish();
	comp_vk_queue.finish();
	
	// write/copy the host data to the Vulkan image
	auto img_data = shared_image->map(comp_vk_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(img_data, image.get(), image_data_size);
	shared_image->unmap(comp_vk_queue, img_data);
	
	// finish write
	comp_vk_queue.finish();
	
	vk_object_state = true;
	return true;
}

bool host_image::sync_vulkan_image(const compute_queue* cqueue_, const vulkan_queue* vk_queue_) const {
	if (shared_vk_image == nullptr) return false;
	if (!image) return false;
	if (vk_object_state) {
		// no need, already acquired for Vulkan use
		return true;
	}
	
	const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
	const auto comp_vk_queue = (vk_queue_ != nullptr ? (const compute_queue*)vk_queue_ : get_default_queue_for_memory(*shared_image));
	
	// validate host queue
#if defined(FLOOR_DEBUG)
	if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
		log_error("specified queue is not a host-compute queue");
		return false;
	}
#endif

	// full sync
	cqueue->finish();
	comp_vk_queue->finish();
	
	// write/copy the host data to the Vulkan image
	auto img_data = shared_image->map(*comp_vk_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	memcpy(img_data, image.get(), image_data_size);
	shared_image->unmap(*comp_vk_queue, img_data);
	
	// finish write
	comp_vk_queue->finish();
	
	return true;
}
#else
bool host_image::acquire_vulkan_image(const compute_queue&, const vulkan_queue&) {
	return false;
}
bool host_image::release_vulkan_image(const compute_queue&, const vulkan_queue&) {
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
	auto shared_image_flags = flags | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE;
	if (!copy_host_data) {
		shared_image_flags |= COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY;
	}
	host_shared_image = render_ctx->create_image(*default_queue, image_dim, image_type, host_data, shared_image_flags);
	host_shared_image->set_debug_label("host_shared_image");
	if (!host_shared_image) {
		log_error("Host <-> Metal/Vulkan image sharing failed: failed to create the underlying shared Metal/Vulkan image");
		return false;
	}
	shared_image = host_shared_image.get();
	
	return true;
}

#endif
