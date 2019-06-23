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

#ifndef __FLOOR_GRAPHICS_GRAPHICS_PASS_HPP__
#define __FLOOR_GRAPHICS_GRAPHICS_PASS_HPP__

#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/device/image_types.hpp>

//! load operation to be used on an attachment
enum class LOAD_OP {
	//! load value from attachment
	LOAD,
	//! use clear value instead of attachment value
	CLEAR,
	//! loaded value is undefined
	DONT_CARE,
};

//! store operation to be used on an attachment
enum class STORE_OP {
	//! store value to attachment
	STORE,
	//! stored value is undefined
	DONT_CARE,
};

//! full pass description used to create pass objects
//! NOTE: for now, this always consists of a single sub-pass
struct render_pass_description {
	//! per-attachment description, i.e. how and which values are loaded from and stored to an attachment
	struct attachment_desc_t {
		//! base pixel format of the attachment
		//! requires: FORMAT, CHANNELS, DATA_TYPE, FLAG_DEPTH (if depth)
		//! optional: LAYOUT, COMPRESSION, FLAG_NORMALIZED, FLAG_SRGB, FLAG_STENCIL (not supported yet)
		//! e.g.: specify BGRA8UI_NORM, RGBA16F or D32F
		COMPUTE_IMAGE_TYPE format;
		//! load operation performed on the attachment
		LOAD_OP load_op { LOAD_OP::CLEAR };
		//! store operation performed on the attachment
		STORE_OP store_op { STORE_OP::STORE };
		//! attachment clear color/depth if "load_op" is LOAD_OP::CLEAR
		//! depending on "format", either clear.color or clear.depth is active
		struct {
			//! RGBA color clear value
			float4 color { 0.0f, 0.0f, 0.0f, 0.0f };
			//! depth clear value
			float depth { 1.0f };
		} clear;
	};
	
	//! description of all attachments used/required for this pass
	//! NOTE: includes both color and depth attachments
	vector<attachment_desc_t> attachments;
};

//! pass object used for rendering with graphics_renderer
class graphics_pass {
public:
	graphics_pass(const render_pass_description& pass_desc_);
	virtual ~graphics_pass();
	
	//! returns the description of this pass
	const render_pass_description& get_description() const {
		return pass_desc;
	}
	
	//! returns true if this pass is in a valid state
	bool is_valid() const {
		return valid;
	}
	
protected:
	const render_pass_description pass_desc;
	bool valid { false };
	
};

#endif
