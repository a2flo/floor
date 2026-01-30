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
#include <optional>

namespace fl {

//! load operation to be used on an attachment
enum class LOAD_OP {
	//! load value from attachment
	LOAD,
	//! use clear value instead of attachment value
	//! NOTE: clears the whole attachment regardless of active scissor rectangle
	CLEAR,
	//! loaded value is undefined
	DONT_CARE,
};

//! store operation to be used on an attachment
enum class STORE_OP {
	//! store value to attachment
	STORE,
	//! resolve MSAA buffer image to the resolve image
	RESOLVE,
	//! store to the MSAA buffer attachment and resolve the MSAA buffer image to the resolve image
	STORE_AND_RESOLVE,
	//! stored value is undefined
	DONT_CARE,
};

//! attachment clear color/depth
struct clear_value_t {
	//! RGBA color clear value
	float4 color { 0.0f, 0.0f, 0.0f, 0.0f };
	//! depth clear value
	float depth { 1.0f };
};

//! full pass description used to create pass objects
//! NOTE: for now, this always consists of a single sub-pass
struct render_pass_description {
	//! per-attachment description, i.e. how and which values are loaded from and stored to an attachment
	struct attachment_desc_t {
		//! base pixel format of the attachment
		//! requires: FORMAT, CHANNELS, DATA_TYPE, FLAG_DEPTH (if depth), FLAG_MSAA/FLAG_TRANSIENT/SAMPLE_COUNT_* (if MSAA)
		//! optional: LAYOUT, COMPRESSION, FLAG_NORMALIZED, FLAG_SRGB, FLAG_ARRAY, FLAG_STENCIL (not supported yet)
		//! e.g.: specify BGRA8UI_NORM, RGBA16F or D32F
		IMAGE_TYPE format { IMAGE_TYPE::NONE };
		//! load operation performed on the attachment
		LOAD_OP load_op { LOAD_OP::CLEAR };
		//! store operation performed on the attachment
		//! NOTE: when resolving an MSAA image, this should be set to RESOLVE rather than STORE_AND_RESOLVE for best performance (if the MSAA image
		//! content is no longer needed)
		STORE_OP store_op { STORE_OP::STORE };
		//! attachment clear color/depth if "load_op" is LOAD_OP::CLEAR
		//! depending on "format", either clear.color or clear.depth is active
		clear_value_t clear {};
		//! if enabled and "automatic_multi_view_handling" is enabled as well, allow automatic format transformation of this to a layer format
		//! NOTE: this flag enables per-attachment multi-view deactivation if only a singular attachment is wanted
		bool automatic_multi_view_transformation { true };
	};
	
	//! description of all attachments used/required for this pass
	//! NOTE: includes both color and depth attachments
	std::vector<attachment_desc_t> attachments;
	
	//! if enabled, performs automatic modification of this render pass description to enable multi-view rendering
	//! if not enabled, this render pass description must already be multi-view capable when used for multi-view rendering
	bool automatic_multi_view_handling { true };
	
	//! sets the debug label for passes created from this description (e.g. for display in a debugger)
	std::string debug_label;
};

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

//! pass object used for rendering with graphics_renderer
class graphics_pass {
public:
	explicit graphics_pass(const render_pass_description& pass_desc_, const bool with_multi_view_support = false);
	virtual ~graphics_pass() = default;
	
	//! returns the description of this pass
	const render_pass_description& get_description(const bool get_multi_view) const {
		return (!get_multi_view || !multi_view_pass_desc ? pass_desc : *multi_view_pass_desc);
	}
	
	//! returns true if this pass is in a valid state
	bool is_valid() const {
		return valid;
	}
	
	//! returns true if this pass can be used for multi-view rendering
	bool is_multi_view_capable() const {
		return multi_view_capable;
	}
	
	//! returns true if this pass can be used for single-view rendering
	//! NOTE: it is possible that this pass can be multi-view-only
	bool is_single_view_capable() const {
		return (!multi_view_capable || multi_view_pass_desc);
	}
	
protected:
	const render_pass_description pass_desc;
	const std::optional<render_pass_description> multi_view_pass_desc;
	bool valid { false };
	bool multi_view_capable { false };
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
