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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>

vulkan_compute::vulkan_compute(const uint64_t platform_index_,
							   const vector<string> whitelist) : compute_context() {
	// if no platform was specified, use the one in the config (or default one, which is 0)
	const auto platform_index = (platform_index_ == ~0u ? floor::get_vulkan_platform() : platform_index_);

	// TODO: implement this
}

shared_ptr<compute_queue> vulkan_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) return {};
	
	// TODO: implement this
	return {};
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	// TODO: does device matter?
	return make_shared<vulkan_buffer>((vulkan_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													   const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													   void*, const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<vulkan_image>((vulkan_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> vulkan_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<vulkan_image>((vulkan_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> vulkan_compute::wrap_image(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													 const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::wrap_image(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													 void*, const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<vulkan_program> vulkan_compute::add_program(vulkan_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<vulkan_program>(move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> vulkan_compute::add_program_file(const string& file_name,
															 const string additional_options) {
	// compile the source file for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((vulkan_device*)dev.get(),
								  create_vulkan_program(dev, llvm_compute::compile_program_file(dev, file_name, additional_options,
																								llvm_compute::TARGET::SPIRV)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> vulkan_compute::add_program_source(const string& source_code,
															   const string additional_options) {
	// compile the source code for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((vulkan_device*)dev.get(),
								  create_vulkan_program(dev, llvm_compute::compile_program(dev, source_code, additional_options,
																						   llvm_compute::TARGET::SPIRV)));
	}
	return add_program(move(prog_map));
}

vulkan_program::vulkan_program_entry vulkan_compute::create_vulkan_program(shared_ptr<compute_device> device,
																		   pair<string, vector<llvm_compute::kernel_info>> program_data) {
	// TODO: implement this
	log_error("not yet supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_program> vulkan_compute::add_precompiled_program_file(const string& file_name floor_unused,
																		 const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> vulkan_compute::create_program_entry(shared_ptr<compute_device> device,
																				pair<string, vector<llvm_compute::kernel_info>> program_data) {
	return make_shared<vulkan_program::vulkan_program_entry>(create_vulkan_program(device, program_data));
}

#endif
