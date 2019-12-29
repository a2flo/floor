/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_METAL_COMPUTE_HPP__
#define __FLOOR_METAL_COMPUTE_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_context.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

#if defined(__OBJC__)
@class metal_view;
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#endif

class metal_program;
class metal_device;
class vr_context;

class metal_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	metal_compute(const bool enable_renderer = false,
				  vr_context* vr_ctx_ = nullptr,
				  const vector<string> whitelist = {});
	
	~metal_compute() override = default;
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return true; }
	
	bool is_vr_supported() const override;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::METAL; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_queue(const compute_device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 void* data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 void* data,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_universal_binary(const string& file_name) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// graphics functionality
	
	unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc) const override;
	
	unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc) const override;
	
	unique_ptr<graphics_renderer> create_graphics_renderer(const compute_queue& cqueue,
														   const graphics_pass& pass,
														   const graphics_pipeline& pipeline,
														   const bool create_multi_view_renderer = false) const override;
	
	COMPUTE_IMAGE_TYPE get_renderer_image_type() const override;

	uint4 get_renderer_image_dim() const override;

	vr_context* get_renderer_vr_context() const override;
	
	//////////////////////////////////////////
	// metal specific functions
	
	//! for debugging/testing purposes only (circumvents the internal program handling)
	shared_ptr<compute_program> create_metal_test_program(shared_ptr<compute_program::program_entry> entry);
	
	// Metal functions only available in Objective-C/C++ mode
#if defined(__OBJC__)
	//! if this context was created with renderer support, this returns the underlying pixel format of the Metal view
	MTLPixelFormat get_metal_renderer_pixel_format() const;
	
	//! if this context was created with renderer support, return the next drawable of the Metal view
	id <CAMetalDrawable> get_metal_next_drawable(id <MTLCommandBuffer> cmd_buffer) const;
	
	//! if this context was created with renderer and VR support, return the next drawable VR Metal image
	shared_ptr<compute_image> get_metal_next_vr_drawable() const;
	
	//! presents the specified VR drawable
	void present_metal_vr_drawable(const compute_queue& cqueue, const compute_image& img) const;
#endif
	
protected:
	void* ctx { nullptr };
	vr_context* vr_ctx { nullptr };
	
	bool enable_renderer { false };
#if defined(__OBJC__)
	metal_view* view { nullptr };
#else
	void* view { nullptr };
#endif
	const metal_device* render_device { nullptr };
	
	flat_map<const compute_device&, shared_ptr<compute_queue>> internal_queues;
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<metal_program>> programs GUARDED_BY(programs_lock);
	
	// VR handling
	struct vr_image_t {
		shared_ptr<compute_image> image;
		atomic_spin_lock image_lock;
	};
	static constexpr const uint32_t vr_image_count { 2 };
	mutable array<vr_image_t, vr_image_count> vr_images;
	mutable uint32_t vr_image_index { 0 };
	bool init_vr_renderer();
	
};

#endif

#endif
