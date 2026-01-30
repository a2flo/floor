/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/metal/metal_function.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <floor/device/graphics_renderer.hpp>
#include <Metal/Metal.h>

namespace fl {

class metal_shader final : public metal_function {
public:
	metal_shader(function_map_type&& functions);
	~metal_shader() override = default;
	
	//! override, since execute is not supported/allowed with shaders
	void execute(const device_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const std::vector<device_function_arg>& args,
				 const std::vector<const device_fence*>& wait_fences,
				 const std::vector<device_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	//! sets and handles all vertex and fragment shader arguments in the specified encoder
	void set_shader_arguments(const device_queue& cqueue,
							  id <MTLRenderCommandEncoder> encoder,
							  id <MTLCommandBuffer> cmd_buffer,
							  const metal_function_entry* vertex_shader,
							  const metal_function_entry* fragment_shader,
							  const std::vector<device_function_arg>& args) const;
	
	//! enqueue draw call(s) of the specified primitive type in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
			  const std::vector<graphics_renderer::multi_draw_entry>& draw_entries) const;
	
	//! enqueue draw call(s) with indexing of the specified primitive type in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
			  const std::vector<graphics_renderer::multi_draw_indexed_entry>& draw_indexed_entries) const;
	
	//! enqueue a patch draw call in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder,
			  const graphics_renderer::patch_draw_entry& draw_entry) const;
	
	//! enqueue an indexed patch draw call in the specified encoder
	void draw(id <MTLRenderCommandEncoder> encoder,
			  const graphics_renderer::patch_draw_indexed_entry& draw_indexed_entry,
			  const uint32_t index_size) const;
	
};

} // namespace fl

#endif
