/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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
#include <floor/core/logger.hpp>

compute_buffer::compute_buffer(compute_device* device,
							   const size_t& size_,
							   void* host_ptr_,
							   const COMPUTE_MEMORY_FLAG flags_,
							   const uint32_t opengl_type_,
							   const uint32_t external_gl_object_) :
compute_memory(device, host_ptr_, flags_, opengl_type_, external_gl_object_), size(align_size(size_)) {
	if(size == 0) {
		log_error("can't allocate a buffer of size 0!");
	}
	else if(size_ != size) {
		log_error("buffer size must always be a multiple of %u! - using size of %u instead of %u now",
				  min_multiple(), size, size_);
	}
#if defined(PLATFORM_X64)
	else if(size > 0xFFFFFFFFu && has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		log_error("using a buffer larger than 4GiB is not supported when using OpenGL sharing!");
		size = 0xFFFFFFFFu;
	}
#endif
}

compute_buffer::~compute_buffer() {}

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
		log_error("created opengl buffer %u is invalid!", gl_object);
		return false;
	}
	
	if(copy_host_data &&
	   host_ptr != nullptr &&
	   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		glBufferData(opengl_type, (GLsizeiptr)size, host_ptr, GL_DYNAMIC_DRAW);
	}
	else {
		glBufferData(opengl_type, (GLsizeiptr)size, nullptr, GL_DYNAMIC_DRAW);
	}
	glBindBuffer(opengl_type, 0);
	
	return true;
}

compute_buffer::opengl_buffer_info compute_buffer::get_opengl_buffer_info(const uint32_t& opengl_buffer,
																		  const uint32_t& opengl_type,
																		  const COMPUTE_MEMORY_FLAG& flags floor_unused) {
	if(opengl_buffer == 0) {
		log_error("invalid opengl buffer");
		return {};
	}
	if(!glIsBuffer(opengl_buffer)) {
		log_error("opengl object %u is not a buffer!", opengl_buffer);
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
	switch(opengl_type) {
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
#if !defined(FLOOR_IOS) || defined(PLATFORM_X64)
		case GL_TRANSFORM_FEEDBACK_BUFFER:
			glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &cur_bound_object);
			break;
		case GL_UNIFORM_BUFFER:
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &cur_bound_object);
			break;
#endif
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
#if !defined(FLOOR_IOS) || defined(PLATFORM_X64)
		// else
		case GL_COPY_READ_BUFFER:
		case GL_COPY_WRITE_BUFFER:
		case GL_PIXEL_PACK_BUFFER:
		case GL_PIXEL_UNPACK_BUFFER:
			log_error("can't create a buffer of opengl type %X!", opengl_type);
			break;
#endif
		default:
			// unknown buffer type, can't do anything here
			break;
	}
	
	opengl_buffer_info info;
	
	GLint buffer_size = 0;
	glBindBuffer(opengl_type, opengl_buffer);
	glGetBufferParameteriv(opengl_type, GL_BUFFER_SIZE, &buffer_size);
	info.size = (uint32_t)buffer_size;
	
	// size check
	if(buffer_size % int(min_multiple()) != 0) {
		log_warn("opengl buffer size should be aligned to %u bytes!", min_multiple());
	}
	
	// restore/rebind
	if(cur_bound_object > 0) glBindBuffer(opengl_type, (GLuint)cur_bound_object);
	
	info.valid = true;
	return {};
}
