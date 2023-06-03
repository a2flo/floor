/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/core/logger.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#endif

compute_buffer::compute_buffer(const compute_queue& cqueue,
							   const size_t& size_,
							   std::span<uint8_t> host_data_,
							   const COMPUTE_MEMORY_FLAG flags_,
							   const uint32_t opengl_type_,
							   const uint32_t external_gl_object_,
							   compute_buffer* shared_buffer_) :
compute_memory(cqueue, host_data_, flags_, opengl_type_, external_gl_object_), size(align_size(size_)), shared_buffer(shared_buffer_) {
	if (size == 0) {
		throw std::runtime_error("can't allocate a buffer of size 0!");
	}
	
	if (host_data.data() != nullptr && host_data.size_bytes() != size) {
		throw std::runtime_error("host data size " + std::to_string(host_data.size_bytes()) + " does not match buffer size " + std::to_string(size));
	}
	
	if (size_ != size) {
		log_error("buffer size must always be a multiple of $! - using size of $ instead of $ now",
				  min_multiple(), size, size_);
	}
	
	if (size > 0xFFFF'FFFFu && has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		log_error("using a buffer larger than 4GiB is not supported when using OpenGL sharing!");
		size = 0xFFFFFFFFu;
	}
	
	if (shared_buffer != nullptr) {
		if (!has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags) && !has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(flags)) {
			log_warn("provided a shared buffer, but no sharing flag is set");
		}
	}
	
	// if there is host data, it must have at least the same size as the buffer
	if (host_data.data() != nullptr && host_data.size_bytes() < size) {
		throw std::runtime_error("image host data size " + std::to_string(host_data.size_bytes()) +
								 " is smaller than the expected buffer size " + std::to_string(size));
	}
}

void compute_buffer::delete_gl_buffer() {
	if(has_external_gl_object) return;
	if(gl_object == 0) return;
	glDeleteBuffers(1, &gl_object);
	gl_object = 0;
}

bool compute_buffer::create_gl_buffer(const bool copy_host_data) {
	if(has_external_gl_object) return true;
	
	// clean up previous gl buffer that might still exist
	delete_gl_buffer();
	
	// create and init the opengl buffer
	glGenBuffers(1, &gl_object);
	glBindBuffer(opengl_type, gl_object);
	if(gl_object == 0 || !glIsBuffer(gl_object)) {
		log_error("created opengl buffer $ is invalid!", gl_object);
		return false;
	}
	
	if(copy_host_data &&
	   host_data.data() != nullptr &&
	   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		glBufferData(opengl_type, (GLsizeiptr)size, host_data.data(), GL_DYNAMIC_DRAW);
	}
	else {
		glBufferData(opengl_type, (GLsizeiptr)size, nullptr, GL_DYNAMIC_DRAW);
	}
	glBindBuffer(opengl_type, 0);
	
	return true;
}

compute_buffer::opengl_buffer_info compute_buffer::get_opengl_buffer_info(const uint32_t& opengl_buffer,
																		  const uint32_t& opengl_type_,
																		  const COMPUTE_MEMORY_FLAG& flags_ floor_unused) {
	if(opengl_buffer == 0) {
		log_error("invalid opengl buffer");
		return {};
	}
	if(!glIsBuffer(opengl_buffer)) {
		log_error("opengl object $ is not a buffer!", opengl_buffer);
		return {};
	}
	
	// assuming there is at least opengl 4.1 core support here (header-wise)
#if !defined(GL_ATOMIC_COUNTER_BUFFER)
#define GL_ATOMIC_COUNTER_BUFFER 0x92C0
#define GL_ATOMIC_COUNTER_BUFFER_BINDING 0x92C1
#endif
#if !defined(GL_DISPATCH_INDIRECT_BUFFER)
#define GL_DISPATCH_INDIRECT_BUFFER 0x90EE
#define GL_DISPATCH_INDIRECT_BUFFER_BINDING 0x90EF
#endif
#if !defined(GL_QUERY_BUFFER)
#define GL_QUERY_BUFFER 0x9192
#define GL_QUERY_BUFFER_BINDING 0x9193
#endif
#if !defined(GL_SHADER_STORAGE_BUFFER)
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_SHADER_STORAGE_BUFFER_BINDING 0x90D3
#endif
#if !defined(GL_PARAMETER_BUFFER_ARB)
#define GL_PARAMETER_BUFFER_ARB 0x80EE
#define GL_PARAMETER_BUFFER_BINDING_ARB 0x80EF
#endif
#if !defined(GL_NV_parameter_buffer_object)
#define GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV 0x8DA2
#define GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV 0x8DA3
#define GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV 0x8DA4
#endif
#if !defined(GL_VIDEO_BUFFER_NV)
#define GL_VIDEO_BUFFER_NV 0x9020
#define GL_VIDEO_BUFFER_BINDING_NV 0x9021
#endif
	
	// figure out the currently bound buffer object, so we can rebind/restore it at the end
	GLint cur_bound_object = -1;
	switch(opengl_type_) {
		// core opengl
		case GL_ARRAY_BUFFER:
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_ATOMIC_COUNTER_BUFFER:
			glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_DISPATCH_INDIRECT_BUFFER:
			glGetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &cur_bound_object);
			break;
#if !defined(FLOOR_IOS)
		case GL_DRAW_INDIRECT_BUFFER:
			glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &cur_bound_object);
			break;
#endif
		case GL_ELEMENT_ARRAY_BUFFER:
			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_QUERY_BUFFER:
			glGetIntegerv(GL_QUERY_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_SHADER_STORAGE_BUFFER:
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &cur_bound_object);
			break;
#if !defined(FLOOR_IOS)
		case GL_TEXTURE_BUFFER:
			glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &cur_bound_object);
			break;
#endif
		case GL_TRANSFORM_FEEDBACK_BUFFER:
			glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_UNIFORM_BUFFER:
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &cur_bound_object);
			break;
		// extensions
		case GL_PARAMETER_BUFFER_ARB:
			glGetIntegerv(GL_PARAMETER_BUFFER_BINDING_ARB, &cur_bound_object);
			break;
		case GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV:
		case GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV:
		case GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV:
			// no binding query support, just fall through
			break;
		case GL_VIDEO_BUFFER_NV:
			glGetIntegerv(GL_VIDEO_BUFFER_BINDING_NV, &cur_bound_object);
			break;
		// else
		case GL_COPY_READ_BUFFER:
		case GL_COPY_WRITE_BUFFER:
		case GL_PIXEL_PACK_BUFFER:
		case GL_PIXEL_UNPACK_BUFFER:
			log_error("can't create a buffer of opengl type $X!", opengl_type_);
			break;
		default:
			// unknown buffer type, can't do anything here
			break;
	}
	
	opengl_buffer_info info;
	
	GLint buffer_size = 0;
	glBindBuffer(opengl_type_, opengl_buffer);
	glGetBufferParameteriv(opengl_type_, GL_BUFFER_SIZE, &buffer_size);
	info.size = (uint32_t)buffer_size;
	
	// size check
	if(buffer_size % int(min_multiple()) != 0) {
		log_warn("opengl buffer size should be aligned to $ bytes!", min_multiple());
	}
	
	// restore/rebind
	if(cur_bound_object > 0) glBindBuffer(opengl_type_, (GLuint)cur_bound_object);
	
	info.valid = true;
	return {};
}

shared_ptr<compute_buffer> compute_buffer::clone(const compute_queue& cqueue, const bool copy_contents,
												 const COMPUTE_MEMORY_FLAG flags_override) {
	if (dev.context == nullptr) {
		log_error("invalid buffer/device state");
		return {};
	}
	
	auto clone_flags = (flags_override != COMPUTE_MEMORY_FLAG::NONE ? flags_override : flags);
	shared_ptr<compute_buffer> ret;
	if (host_data.data() != nullptr) {
		// never copy host data on the newly created buffer
		clone_flags |= COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY;
		assert(size == host_data.size_bytes());
		ret = dev.context->create_buffer(cqueue, host_data, clone_flags, opengl_type);
	} else {
		ret = dev.context->create_buffer(cqueue, size, clone_flags, opengl_type);
	}
	if (ret == nullptr) {
		return {};
	}
	
	if (copy_contents) {
		ret->copy(cqueue, *this);
	}
	
	return ret;
}

const vulkan_buffer* compute_buffer::get_underlying_vulkan_buffer_safe() const {
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		const vulkan_buffer* ret = nullptr;
		if (ret = get_shared_vulkan_buffer(); !ret) {
			ret = (const vulkan_buffer*)this;
		} else {
			if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING_SYNC_SHARED>(flags)) {
				sync_vulkan_buffer();
			}
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_VULKAN)
		if (auto test_cast_vk_buffer = dynamic_cast<const vulkan_buffer*>(ret); !test_cast_vk_buffer) {
			throw runtime_error("specified buffer is neither a Vulkan buffer nor a shared Vulkan buffer");
		}
#endif
		return ret;
	}
	return (const vulkan_buffer*)this;
}
