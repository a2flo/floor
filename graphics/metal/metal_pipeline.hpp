/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_METAL_METAL_PIPELINE_HPP__
#define __FLOOR_GRAPHICS_METAL_METAL_PIPELINE_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_kernel.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <Metal/Metal.h>

class metal_pipeline final : public graphics_pipeline {
public:
	metal_pipeline(const render_pipeline_description& pipeline_desc,
				   const vector<unique_ptr<compute_device>>& devices,
				   const bool with_multi_view_support = false);
	~metal_pipeline() override;
	
	//! all Metal pipeline state
	struct metal_pipeline_entry {
		id <MTLRenderPipelineState> pipeline_state;
		id <MTLDepthStencilState> depth_stencil_state;
		const compute_kernel::kernel_entry* vs_entry { nullptr };
		const compute_kernel::kernel_entry* fs_entry { nullptr };
	};
	
	//! return the device specific Metal pipeline state for the specified device (or nullptr if it doesn't exist)
	const metal_pipeline_entry* get_metal_pipeline_entry(const compute_device& dev) const;
	
	//! returns the corresponding MTLPrimitiveType for the specified PRIMITIVE
	static MTLPrimitiveType metal_primitive_type_from_primitive(const PRIMITIVE& primitive);
	
	//! returns the corresponding MTLCullMode for the specified CULL_MODE
	static MTLCullMode metal_cull_mode_from_cull_mode(const CULL_MODE& cull_mode);
	
	//! returns the corresponding MTLWinding for the specified FRONT_FACE
	static MTLWinding metal_winding_from_front_face(const FRONT_FACE& front_face);
	
	//! returns the corresponding MTLBlendFactor for the specified BLEND_FACTOR
	static MTLBlendFactor metal_blend_factor_from_blend_factor(const BLEND_FACTOR& blend_factor);
	
	//! returns the corresponding MTLBlendOperation for the specified BLEND_OP
	static MTLBlendOperation metal_blend_op_from_blend_op(const BLEND_OP& blend_op);
	
	//! returns the corresponding MTLCompareFunction for the specified DEPTH_COMPARE
	static MTLCompareFunction metal_compare_func_from_depth_compare(const DEPTH_COMPARE& depth_compare);
	
	//! returns the corresponding MTLTessellationPartitionMode for the specified TESSELLATION_SPACING
	static MTLTessellationPartitionMode metal_tessellation_partition_mode_from_spacing(const TESSELLATION_SPACING& spacing);
	
	//! returns the corresponding MTLWinding for the specified TESSELLATION_WINDING
	static MTLWinding metal_winding_from_winding(const TESSELLATION_WINDING& winding);
	
	//! returns the corresponding MTLVertexFormat for the specified VERTEX_FORMAT
	//! NOTE: returns MTLVertexFormatInvalid when there is no Metal-compatible vertex format
	static MTLVertexFormat metal_vertex_format_from_vertex_format(const VERTEX_FORMAT& vertex_format);
	
protected:
	floor_core::flat_map<const compute_device&, metal_pipeline_entry> pipelines;
	
};

#endif

#endif
