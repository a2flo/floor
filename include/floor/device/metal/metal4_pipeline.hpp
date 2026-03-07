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

#include <floor/device/device_function.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <Metal/Metal.h>

namespace fl {

class metal4_pipeline final : public graphics_pipeline {
public:
	metal4_pipeline(const render_pipeline_description& pipeline_desc,
					const std::vector<std::unique_ptr<device>>& devices,
					const bool with_multi_view_support = false);
	~metal4_pipeline() override;
	
	//! all Metal pipeline state
	struct metal4_pipeline_entry {
		id <MTLRenderPipelineState> pipeline_state { nil };
		id <MTLDepthStencilState> depth_stencil_state { nil };
		const device_function::function_entry* vs_entry { nullptr };
		const device_function::function_entry* fs_entry { nullptr };
		MTL4RenderPipelineDescriptor* pipeline_desc { nullptr };
	};
	
	//! return the device specific Metal pipeline state for the specified device (or nullptr if it doesn't exist)
	const metal4_pipeline_entry* get_metal_pipeline_entry(const device& dev) const;
	
	//! returns the corresponding MTLPrimitiveType for the specified PRIMITIVE
	static MTLPrimitiveType metal_primitive_type_from_primitive(const PRIMITIVE primitive);
	
	//! returns the corresponding MTLCullMode for the specified CULL_MODE
	static MTLCullMode metal_cull_mode_from_cull_mode(const CULL_MODE cull_mode);
	
	//! returns the corresponding MTLWinding for the specified FRONT_FACE
	static MTLWinding metal_winding_from_front_face(const FRONT_FACE front_face);
	
	//! returns the corresponding MTLBlendFactor for the specified BLEND_FACTOR
	static MTLBlendFactor metal_blend_factor_from_blend_factor(const BLEND_FACTOR blend_factor);
	
	//! returns the corresponding MTLBlendOperation for the specified BLEND_OP
	static MTLBlendOperation metal_blend_op_from_blend_op(const BLEND_OP blend_op);
	
	//! returns the corresponding MTLCompareFunction for the specified DEPTH_COMPARE
	static MTLCompareFunction metal_compare_func_from_depth_compare(const DEPTH_COMPARE depth_compare);
	
	//! returns the corresponding MTLIndexType for the specified INDEX_TYPE
	static MTLIndexType metal_index_type_from_index_type(const INDEX_TYPE index_type);
	
protected:
	fl::flat_map<const device*, metal4_pipeline_entry> pipelines;
	
};

} // namespace fl

#endif
