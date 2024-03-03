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

#include <floor/compute/host/host_buffer.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>
#include <floor/floor/floor.hpp>
#include <floor/constexpr/const_string.hpp>

host_buffer::host_buffer(const compute_queue& cqueue,
						 const size_t& size_,
						 std::span<uint8_t> host_data_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_,
						 const uint32_t external_gl_object_,
						 compute_buffer* shared_buffer_) :
compute_buffer(cqueue, size_, host_data_, flags_, opengl_type_, external_gl_object_, shared_buffer_) {
	if(size < min_multiple()) return;
	
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

	// actually create the buffer
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool host_buffer::create_internal(const bool copy_host_data, const compute_queue& cqueue) {
	// TODO: handle the remaining flags + host ptr
	
	// always allocate host memory (even with OpenGL/Metal/Vulkan, memory needs to be copied somewhere)
	buffer = make_aligned_ptr<uint8_t>(size);

	// -> normal host buffer
	if (!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags) &&
		!has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags) &&
		!has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		// copy host memory to "device" if it is non-null and NO_INITIAL_COPY is not specified
		if (copy_host_data &&
			host_data.data() != nullptr &&
			!has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			memcpy(buffer.get(), host_data.data(), size);
		}
	}
#if !defined(FLOOR_NO_METAL)
	// -> shared host <-> Metal buffer
	else if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		if (!create_shared_buffer(copy_host_data)) {
			return false;
		}
		
		// acquire for use with the host
		const compute_queue* comp_mtl_queue = get_default_queue_for_memory(*shared_buffer);
		acquire_metal_buffer(&cqueue, (const metal_queue*)comp_mtl_queue);
	}
#endif
#if !defined(FLOOR_NO_VULKAN)
	// -> shared host <-> Vulkan buffer
	else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (!create_shared_buffer(copy_host_data)) {
			return false;
		}
		
		// acquire for use with the host
		const compute_queue* comp_vk_queue = get_default_queue_for_memory(*shared_buffer);
		acquire_vulkan_buffer(&cqueue, (const vulkan_queue*)comp_vk_queue);
	}
#endif
	// -> shared host/OpenGL buffer
	else {
		if (!create_gl_buffer(copy_host_data)) {
			return false;
		}
		
		// acquire for use with the host
		acquire_opengl_object(&cqueue);
	}

	return true;
}

host_buffer::~host_buffer() {
	// first, release and kill the opengl buffer
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("buffer still registered for opengl use - acquire before destructing a compute buffer!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
		delete_gl_buffer();
	}
}

void host_buffer::read(const compute_queue& cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_data.data(), size_, offset);
}

void host_buffer::read(const compute_queue& cqueue floor_unused, void* dst, const size_t size_, const size_t offset) {
	if (!buffer) return;

	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;

	GUARD(lock);
	memcpy(dst, buffer.get() + offset, read_size);
}

void host_buffer::write(const compute_queue& cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_data.data(), size_, offset);
}

void host_buffer::write(const compute_queue& cqueue floor_unused, const void* src, const size_t size_, const size_t offset) {
	if (!buffer) return;

	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	GUARD(lock);
	memcpy(buffer.get() + offset, src, write_size);
}

void host_buffer::copy(const compute_queue& cqueue floor_unused, const compute_buffer& src,
					   const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if (!buffer) return;

	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src.get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	src._lock();
	_lock();
	
	memcpy(buffer.get() + dst_offset, ((const host_buffer*)&src)->get_host_buffer_ptr() + src_offset, copy_size);
	
	_unlock();
	src._unlock();
}

bool host_buffer::fill(const compute_queue& cqueue floor_unused,
					   const void* pattern, const size_t& pattern_size,
					   const size_t size_, const size_t offset) {
	if (!buffer) return false;

	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return false;
	
	switch(pattern_size) {
		case 1:
			memset(buffer.get() + offset, *(const uint8_t*)pattern, fill_size);
			break;
#if defined(__APPLE__) // TODO: check for availability on linux, *bsd, windows
		// NOTE: memset_pattern* will simple truncate any overspill, so size checking is not necessary
		// NOTE: 3rd parameter is the fill/buffer size and has nothing to do with the pattern count
		case 4:
			memset_pattern4(buffer.get() + offset, (const void*)pattern, fill_size);
			break;
		case 8:
			memset_pattern8(buffer.get() + offset, (const void*)pattern, fill_size);
			break;
		case 16:
			memset_pattern16(buffer.get() + offset, (const void*)pattern, fill_size);
			break;
#endif
		default: {
			// not a pattern size that allows a fast memset
			// -> copy pattern manually in a loop
			const size_t pattern_count = fill_size / pattern_size;
			uint8_t* write_ptr = buffer.get() + offset;
			for(size_t i = 0; i < pattern_count; ++i) {
				memcpy(write_ptr, pattern, pattern_size);
				write_ptr += pattern_size;
			}
			break;
		}
	}
	
	return true;
}

bool host_buffer::zero(const compute_queue& cqueue floor_unused) {
	if (!buffer) return false;

	GUARD(lock);
	memset(buffer.get(), 0, size);
	return true;
}

void* __attribute__((aligned(128))) host_buffer::map(const compute_queue& cqueue,
													 const COMPUTE_MEMORY_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if (!buffer) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;

	if(blocking_map) {
		cqueue.finish();
	}
	
	// NOTE: this is returning a raw pointer to the internal buffer memory and specifically not creating+copying a new buffer
	// -> the user is always responsible for proper sync when mapping a buffer multiple times and this way, it should be
	// easier to detect any problems (race conditions, etc.)
	return buffer.get() + offset;
}

bool host_buffer::unmap(const compute_queue& cqueue floor_unused, void* __attribute__((aligned(128))) mapped_ptr) {
	if (!buffer) return false;
	if (mapped_ptr == nullptr) return false;

	// nop
	return true;
}

bool host_buffer::acquire_opengl_object(const compute_queue* cqueue floor_unused) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// copy gl buffer data to host memory (through r/o map)
	glBindBuffer(opengl_type, gl_object);
#if !defined(FLOOR_IOS)
	void* gl_data = glMapBuffer(opengl_type, GL_READ_ONLY);
#else
	void* gl_data = glMapBufferRange(opengl_type, 0, (GLsizeiptr)size, GL_MAP_READ_BIT);
#endif
	if(gl_data == nullptr) {
		log_error("failed to acquire opengl buffer - opengl buffer mapping failed");
		return false;
	}
	
	memcpy(buffer.get(), gl_data, size);
	
	if(!glUnmapBuffer(opengl_type)) {
		log_error("opengl buffer unmapping failed");
	}
	glBindBuffer(opengl_type, 0);
	
	gl_object_state = false;
	return true;
}

bool host_buffer::release_opengl_object(const compute_queue* cqueue floor_unused) {
	if(gl_object == 0) return false;
	if (!buffer) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been released for opengl use!");
#endif
		return true;
	}
	
	// copy the host data to the gl buffer
	glBindBuffer(opengl_type, gl_object);
	glBufferSubData(opengl_type, 0, (GLsizeiptr)size, buffer.get());
	glBindBuffer(opengl_type, 0);
	
	gl_object_state = true;
	return true;
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

template <auto backend_name, typename backend_queue_type, typename shared_buffer_type>
static inline bool acquire_sync_buffer(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									   aligned_ptr<uint8_t>& buffer, shared_buffer_type* shared_buffer_ptr,
									   bool& shared_object_state, const size_t& buffer_size,
									   const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_buffer_ptr || !buffer) {
		return false;
	}
	if (!shared_object_state) {
		// -> buffer has already been acquired for use with Host-Compute
		return true;
	}
	
	if (needs_sync_to_host(flags)) {
		const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
#if defined(FLOOR_DEBUG)
		// validate host queue
		if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
			log_error("specified host queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_buffer_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// read/copy shared buffer data to host memory
		shared_buffer_ptr->read(*comp_rqueue, buffer.get(), buffer_size, 0);
		
		// finish read
		comp_rqueue->finish();
	}
	
	shared_object_state = false;
	return true;
}

template <auto backend_name, typename backend_queue_type, typename shared_buffer_type>
static inline bool release_sync_buffer(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									   const aligned_ptr<uint8_t>& buffer, shared_buffer_type* shared_buffer_ptr,
									   bool& shared_object_state, const size_t& buffer_size,
									   const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_buffer_ptr || !buffer) {
		return false;
	}
	if (shared_object_state) {
		// -> buffer has already been released for use with the render backend
		return true;
	}
	
	if (needs_sync_from_host(flags)) {
		const auto cqueue = (cqueue_ != nullptr ? cqueue_ : dev.context->get_device_default_queue(dev));
#if defined(FLOOR_DEBUG)
		// validate host queue
		if (const auto hst_queue = dynamic_cast<const host_queue*>(cqueue); hst_queue == nullptr) {
			log_error("specified host queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_buffer_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// write/copy the host data to the shared buffer
		shared_buffer_ptr->write(*comp_rqueue, buffer.get(), buffer_size, 0);
		
		// finish write
		comp_rqueue->finish();
	}
	
	shared_object_state = true;
	return true;
}

template <auto backend_name, typename backend_queue_type, typename shared_buffer_type>
static inline bool sync_shared_buffer(const compute_queue* cqueue_, const backend_queue_type* rqueue, const compute_device& dev,
									  const aligned_ptr<uint8_t>& buffer, shared_buffer_type* shared_buffer_ptr,
									  bool& shared_object_state, const size_t& buffer_size,
									  const COMPUTE_MEMORY_FLAG& flags) {
	if (!shared_buffer_ptr || !buffer) {
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
			log_error("specified host queue is not a Host-Compute queue");
			return false;
		}
#endif
		
		const auto comp_rqueue = (rqueue != nullptr ? (const compute_queue*)rqueue :
								  compute_memory::get_default_queue_for_memory(*shared_buffer_ptr));
		
		// full sync
		cqueue->finish();
		comp_rqueue->finish();
		
		// write/copy the host data to the shared buffer
		shared_buffer_ptr->write(*comp_rqueue, buffer.get(), buffer_size, 0);
		
		// finish write
		comp_rqueue->finish();
	}
	
	return true;
}

#if !defined(FLOOR_NO_METAL)
bool host_buffer::acquire_metal_buffer(const compute_queue* cqueue_, const metal_queue* mtl_queue_) const {
	return acquire_sync_buffer<"Metal"_cs>(cqueue_, mtl_queue_, dev, buffer, shared_buffer, mtl_object_state, size, flags);
}

bool host_buffer::release_metal_buffer(const compute_queue* cqueue_, const metal_queue* mtl_queue_) const {
	return release_sync_buffer<"Metal"_cs>(cqueue_, mtl_queue_, dev, buffer, shared_buffer, mtl_object_state, size, flags);
}

bool host_buffer::sync_metal_buffer(const compute_queue* cqueue_, const metal_queue* mtl_queue_) const {
	return sync_shared_buffer<"Metal"_cs>(cqueue_, mtl_queue_, dev, buffer, shared_buffer, mtl_object_state, size, flags);
}
#else
bool host_buffer::acquire_metal_buffer(const compute_queue*, const metal_queue*) const {
	return false;
}
bool host_buffer::release_metal_buffer(const compute_queue*, const metal_queue*) const {
	return false;
}
bool host_buffer::sync_metal_buffer(const compute_queue*, const metal_queue*) const {
	return false;
}
#endif

#if !defined(FLOOR_NO_VULKAN)
bool host_buffer::acquire_vulkan_buffer(const compute_queue* cqueue_, const vulkan_queue* vk_queue) const {
	return acquire_sync_buffer<"Vulkan"_cs>(cqueue_, vk_queue, dev, buffer, shared_buffer, vk_object_state, size, flags);
}

bool host_buffer::release_vulkan_buffer(const compute_queue* cqueue_, const vulkan_queue* vk_queue) const {
	return release_sync_buffer<"Vulkan"_cs>(cqueue_, vk_queue, dev, buffer, shared_buffer, vk_object_state, size, flags);
}

bool host_buffer::sync_vulkan_buffer(const compute_queue* cqueue_, const vulkan_queue* vk_queue) const {
	return sync_shared_buffer<"Vulkan"_cs>(cqueue_, vk_queue, dev, buffer, shared_buffer, vk_object_state, size, flags);
}
#else
bool host_buffer::acquire_vulkan_buffer(const compute_queue*, const vulkan_queue*) const {
	return false;
}
bool host_buffer::release_vulkan_buffer(const compute_queue*, const vulkan_queue*) const {
	return false;
}
bool host_buffer::sync_vulkan_buffer(const compute_queue*, const vulkan_queue*) const {
	return false;
}
#endif

bool host_buffer::create_shared_buffer(const bool copy_host_data) {
	if (shared_buffer != nullptr && host_shared_buffer == nullptr) {
		// wrapping an existing Metal/Vulkan buffer
		return true;
	}
	
	// get the render/graphics context so that we can create a buffer (TODO: allow specifying a different context?)
	auto render_ctx = floor::get_render_context();
	if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
		if (render_ctx->get_compute_type() != COMPUTE_TYPE::METAL) {
			log_error("Host/Metal buffer sharing failed: render context is not Metal");
			return false;
		}
	} else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (render_ctx->get_compute_type() != COMPUTE_TYPE::VULKAN) {
			log_error("Host/Vulkan buffer sharing failed: render context is not Vulkan");
			return false;
		}
	}
	
	// get the device and its default queue where we want to create the buffer on/in
	// NOTE: we never have a corresponding device here, so simply use the fastest device
	const compute_device* render_dev = render_ctx->get_device(compute_device::TYPE::FASTEST);
	if (render_dev == nullptr) {
		log_error("Host <-> Metal/Vulkan buffer sharing failed: failed to find a Metal/Vulkan device");
		return false;
	}
	
	// create the underlying Metal/Vulkan buffer
	auto default_queue = render_ctx->get_device_default_queue(*render_dev);
	auto shared_buffer_flags = compute_memory::make_host_shared_memory_flags(flags, *render_dev, copy_host_data);
	host_shared_buffer = (host_data.data() != nullptr ?
						  render_ctx->create_buffer(*default_queue, host_data, shared_buffer_flags) :
						  render_ctx->create_buffer(*default_queue, size, shared_buffer_flags));
	if (!host_shared_buffer) {
		log_error("Host <-> Metal/Vulkan buffer sharing failed: failed to create the underlying shared Metal/Vulkan buffer");
		return false;
	}
	host_shared_buffer->set_debug_label("host_shared_buffer");
	shared_buffer = host_shared_buffer.get();
	
	return true;
}

decltype(aligned_ptr<uint8_t>{}.get()) host_buffer::get_host_buffer_ptr_with_sync() const {
#if !defined(FLOOR_NO_METAL) || !defined(FLOOR_NO_VULKAN)
	if (has_flag<COMPUTE_MEMORY_FLAG::SHARING_SYNC>(flags)) {
#if !defined(FLOOR_NO_METAL)
		if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
			// -> acquire for compute use, release from Metal use
			acquire_metal_buffer(nullptr, nullptr);
		}
#endif
#if !defined(FLOOR_NO_VULKAN)
		if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
			// -> acquire for compute use, release from Vulkan use
			acquire_vulkan_buffer(nullptr, nullptr);
		}
#endif
	}
#endif
	return buffer.get();
}

#endif
