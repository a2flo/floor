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

#include <floor/core/essentials.hpp>
#include <floor/core/enum_helpers.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/device/backend/image_types.hpp>
#include <floor/device/graphics_vertex_format.hpp>
#include <floor/device/graphics_index_type.hpp>
#include <optional>

namespace fl {

enum class PRIMITIVE {
	POINT,
	LINE,
	LINE_STRIP,
	TRIANGLE,
	TRIANGLE_STRIP,
};

enum class CULL_MODE {
	NONE,
	BACK,
	FRONT,
};

enum class FRONT_FACE {
	CLOCKWISE,
	COUNTER_CLOCKWISE,
};

enum class DEPTH_COMPARE {
	NEVER,
	LESS,
	EQUAL,
	LESS_OR_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_OR_EQUAL,
	ALWAYS,
};

enum class BLEND_FACTOR {
	//! constants
	ZERO,
	ONE,
	
	//! color modes
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	DST_COLOR,
	ONE_MINUS_DST_COLOR,
	
	//! alpha modes
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DST_ALPHA,
	ONE_MINUS_DST_ALPHA,
	SRC_ALPHA_SATURATE,
	
	//! with constant values
	BLEND_COLOR,
	ONE_MINUS_BLEND_COLOR,
	BLEND_ALPHA,
	ONE_MINUE_BLEND_ALPHA,
};

enum class BLEND_OP {
	ADD,
	SUB,
	REV_SUB,
	MIN,
	MAX,
};

enum class TESSELLATION_SPACING {
	EQUAL,
	FRACTIONAL_ODD,
	FRACTIONAL_EVEN,
};

enum class TESSELLATION_WINDING {
	CLOCKWISE,
	COUNTER_CLOCKWISE,
};

class device_function;

//! full pipeline description used to create pipeline objects
struct render_pipeline_description {
	//! standard vertex shader or post-tessellation vertex shader (run after the fixed-function tessellator)
	//! NOTE: when tessellation is active, only the TES will be run and act as the vertex shader
	//! NOTE: for Vulkan, a synthetic/builtin pass-through "pre-tessellation vertex shader" will be used internally
	const device_function* vertex_shader { nullptr };
	//! standard fragment shader
	const device_function* fragment_shader { nullptr };
	
	//! to be rendered primitive type
	PRIMITIVE primitive { PRIMITIVE::TRIANGLE };
	
	//! geometry culling mode
	CULL_MODE cull_mode { CULL_MODE::BACK };
	
	//! geometry front facing order
	FRONT_FACE front_face { FRONT_FACE::COUNTER_CLOCKWISE };
	
	//! number of samples to be used for multi-sampling (must be a power-of-two in [0, 64])
	//! NOTE: a value of 0 or 1 signals that no multi-sampling is used
	//! NOTE: a minimum of 4 samples should be generally available on all targets
	uint32_t sample_count { 0u };
	
	//! render viewport
	//! NOTE: if set to ~0u, will cover the whole screen
	uint2 viewport { ~0u, ~0u };
	
	//! scissor testing / only render within an area of the screen
	struct scissor_t {
		//! offset within the viewport, (0, 0) is left top
		uint2 offset { 0, 0 };
		//! (width, height) extent of the scissor area
		//! NOTE: if set to ~0u, extent will be set to cover the whole viewport
		uint2 extent { ~0u, ~0u };
	} scissor;
	
	//! depth testing
	//! NOTE: depth testing is implicitly always enabled, set to DEPTH_COMPARE::ALWAYS to disable
	struct depth_t {
		//! flag if depth should be written to the depth attachment
		bool write { true };
		
		//! [min, max] range of the depth stored in the depth attachment
		//! NOTE: depth will be clamped to this range
		float2 range { 0.0f, 1.0f };
		
		//! depth test compare mode, i.e. pass fragment if "current depth <compare> stored depth" is true
		//! NOTE: "NEVER" will never pass any fragment regardless of stored depth, "ALWAYS" will always pass regardless of stored depth
		DEPTH_COMPARE compare { DEPTH_COMPARE::LESS };
	} depth;
	
	//! global blend constants (apply for all attachments that have blending enabled)
	struct blend_t {
		//! constant color used when BLEND_FACTOR is BLEND_COLOR or ONE_MINUS_BLEND_COLOR
		float3 constant_color;
		//! constant alpha used when BLEND_FACTOR is BLEND_ALPHA or ONE_MINUS_BLEND_ALPHA
		float constant_alpha { 0.0f };
	} blend;
	
	//! per-attachment blend state
	struct attachment_blend_t {
		//! flag if blending should be performed
		bool enable { false };
		//! RGBA write mask (default: enable all channels)
		bool4 write_mask { true, true, true, true };
		
		//! blend factor applied to the source color
		BLEND_FACTOR src_color_factor { BLEND_FACTOR::ONE };
		//! blend factor applied to the destination color
		BLEND_FACTOR dst_color_factor { BLEND_FACTOR::ONE };
		//! blend operation performed on the color results
		BLEND_OP color_blend_op { BLEND_OP::ADD };
		
		//! blend factor applied to the source alpha
		BLEND_FACTOR src_alpha_factor { BLEND_FACTOR::ONE };
		//! blend factor applied to the destination alpha
		BLEND_FACTOR dst_alpha_factor { BLEND_FACTOR::ONE };
		//! blend operation performed on the alpha results
		BLEND_OP alpha_blend_op { BLEND_OP::ADD };
	};
	
	//! color attachment state
	struct color_attachment_t {
		//! base pixel format of the attachment
		//! requires: FORMAT, CHANNELS, DATA_TYPE
		//! optional: LAYOUT, COMPRESSION, FLAG_NORMALIZED, FLAG_SRGB, FLAG_ARRAY
		//! e.g.: specify BGRA8UI_NORM or RGBA16F
		IMAGE_TYPE format { IMAGE_TYPE::NONE };
		//! blend state of this attachment
		attachment_blend_t blend;
		//! if enabled and "automatic_multi_view_handling" is enabled as well, allow automatic format transformation of this to a layer format
		//! NOTE: this flag enables per-attachment multi-view deactivation if only a singular attachment is wanted
		bool automatic_multi_view_transformation { true };
	};
	std::vector<color_attachment_t> color_attachments;
	
	//! depth attachment state
	struct depth_attachment_t {
		//! base pixel format of the depth attachment
		//! requires: FLAG_DEPTH, FORMAT, CHANNELS, DATA_TYPE
		//! optional: FLAG_STENCIL (not supported yet), FLAG_ARRAY
		//! NOTE: no depth attachment when NONE (default)
		//! e.g.: specify D32F or D24
		IMAGE_TYPE format { IMAGE_TYPE::NONE };
		//! if enabled and "automatic_multi_view_handling" is enabled as well, allow automatic format transformation of this to a layer format
		//! NOTE: this flag enables per-attachment multi-view deactivation if only a singular attachment is wanted
		bool automatic_multi_view_transformation { true };
	};
	depth_attachment_t depth_attachment;
	
	//! tessellation state
	struct tessellation_t {
		//! maximum tessellation factor that may be used
		//! NOTE: tessellation is inactive if this is 0
		//! NOTE: for active tessellation, the value must be in [1, 64]
		uint32_t max_factor { 0u };
		
		//! vertex attributes of the control points that are read by the fixed-function tessellator (e.g. position, tex coord, ...)
		//! NOTE: it is assumed that each specified vertex attribute is a separate vertex buffer
		//! NOTE: for each attribute, it is assumed that data is densely packed without any padding (stride == size of type)
		std::vector<VERTEX_FORMAT> vertex_attributes;
		
		//! tessellation spacing/partition-mode of the output primitives
		TESSELLATION_SPACING spacing { TESSELLATION_SPACING::EQUAL };
		
		//! winding order of the output primitives
		TESSELLATION_WINDING winding { TESSELLATION_WINDING::COUNTER_CLOCKWISE };
		
		//! only either of "draw_patches()" or "draw_patches_indexed()" is allowed and must be known at pipeline creation time
		bool is_indexed_draw { false };
		
		//! when index drawing is used, this defines the underlying type of indices in the index buffer
		INDEX_TYPE index_type { INDEX_TYPE::UINT };
	} tessellation;
	
	//! if enabled, performs automatic modification of this render pipeline description to enable multi-view rendering
	//! if not enabled, this render pipeline description must already be multi-view capable when used for multi-view rendering
	bool automatic_multi_view_handling { true };
	
	//! if enabled, allows the use of the graphics_pipeline in indirect rendering (indirect_command_pipeline, ...)
	bool support_indirect_rendering { false };
	
	//! if enabled, renders all geometry in wireframe mode (lines fill mode)
	bool render_wireframe { false };
	
	//! sets the debug label for pipelines created from this description (e.g. for display in a debugger)
	std::string debug_label;
};

//! pipeline object used for rendering with graphics_renderer
//! NOTE: this is costly to create, try to avoid doing this at run-time, prefer creation during init
class graphics_pipeline {
public:
	explicit graphics_pipeline(const render_pipeline_description& pipeline_desc_, const bool with_multi_view_support = false);
	virtual ~graphics_pipeline();
	
	//! returns the description of this pipeline
	const render_pipeline_description& get_description(const bool get_multi_view) const {
		return (!get_multi_view || !multi_view_pipeline_desc ? pipeline_desc : *multi_view_pipeline_desc);
	}
	
	//! returns true if this pipeline is in a valid state
	bool is_valid() const {
		return valid;
	}
	
	//! returns true if this pipeline can be used for multi-view rendering
	bool is_multi_view_capable() const {
		return multi_view_capable;
	}
	
	//! returns true if this pipeline can be used for single-view rendering
	//! NOTE: it is possible that this pipeline can be multi-view-only
	bool is_single_view_capable() const {
		return (!multi_view_capable || multi_view_pipeline_desc);
	}
	
protected:
	const render_pipeline_description pipeline_desc;
	const std::optional<render_pipeline_description> multi_view_pipeline_desc;
	bool valid { false };
	bool multi_view_capable { false };
	
	//! takes the 2D input size, sets ~0u components to the physical screen size and returns the result
	//! NOTE: this is used for the viewport and scissor extent computation
	static uint2 compute_dim_from_screen_or_user(const uint2& in_size, const bool is_vr);
	
	//! handles pipeline defaults like setting viewport or scissor extent (when set to auto/default-init)
	static render_pipeline_description handle_pipeline_defaults(const render_pipeline_description& pipeline_desc_, const bool is_vr);
	
};

} // namespace fl
