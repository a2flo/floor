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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/host/host_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>

void host_compute::init(const bool use_platform_devices,
						const uint32_t platform_index_,
						const bool gl_sharing,
						const unordered_set<string> whitelist) {
	// TODO: implement this
}

shared_ptr<compute_queue> host_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) return {};
	// TODO: implement this
	return {};
}

shared_ptr<compute_buffer> host_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(shared_ptr<compute_device> device,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 const COMPUTE_MEMORY_FLAG flags) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(shared_ptr<compute_device> device,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_image> host_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<host_image>((host_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<host_image>((host_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::wrap_image(shared_ptr<compute_device> device,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   const COMPUTE_MEMORY_FLAG flags) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_image> host_compute::wrap_image(shared_ptr<compute_device> device,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags) {
	// TODO: implement this
	return {};
}

void host_compute::finish() {
	// TODO: !
}

void host_compute::flush() {
	// TODO: !
}

void host_compute::activate_context() {
	// TODO: !
}

void host_compute::deactivate_context() {
	// TODO: !
}

shared_ptr<compute_program> host_compute::add_program_file(const string& file_name,
														   const string additional_options) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_program> host_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_program> host_compute::add_program(pair<string, vector<llvm_compute::kernel_info>> program_data,
													  const string additional_options) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_program> host_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by host_compute!");
	return {};
}

#endif
