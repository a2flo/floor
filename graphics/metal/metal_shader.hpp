/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_METAL_METAL_SHADER_HPP__
#define __FLOOR_GRAPHICS_METAL_METAL_SHADER_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/graphics_renderer.hpp>
#include <Metal/Metal.h>

class metal_shader final : public metal_kernel {
public:
	metal_shader(kernel_map_type&& kernels);
	~metal_shader() override = default;
	
	//! override, since execute is not supported/allowed with shaders
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	//! sets and handles all vertex and fragment shader arguments in the specified encoder
	void set_shader_arguments(const compute_queue& cqueue,
							  id <MTLRenderCommandEncoder> encoder,
							  id <MTLCommandBuffer> cmd_buffer,
							  const metal_kernel_entry* vertex_shader,
							  const metal_kernel_entry* fragment_shader,
							  const vector<compute_kernel_arg>& args) const;
	
	//! enqueue draw call(s) of the specified primitive type in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
			  const vector<graphics_renderer::multi_draw_entry>& draw_entries) const;
	
	//! enqueue draw call(s) with indexing of the specified primitive type in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
			  const vector<graphics_renderer::multi_draw_indexed_entry>& draw_indexed_entries) const;

};

#endif

#endif
