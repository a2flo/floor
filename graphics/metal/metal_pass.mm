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

#include <floor/graphics/metal/metal_pass.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>

metal_pass::metal_pass(const render_pass_description& pass_desc_) : graphics_pass(pass_desc_) {
	mtl_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
	
	size_t color_att_counter = 0;
	for (size_t i = 0, count = pass_desc.attachments.size(); i < count; ++i) {
		const auto& att = pass_desc.attachments[i];
		
		if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(att.format)) {
			// depth attachment
			mtl_pass_desc.depthAttachment.loadAction = metal_load_action_from_load_op(att.load_op);
			mtl_pass_desc.depthAttachment.storeAction = metal_store_action_from_store_op(att.store_op);
			mtl_pass_desc.depthAttachment.clearDepth = double(att.clear.depth);
		} else {
			// color attachment
			mtl_pass_desc.colorAttachments[color_att_counter].loadAction = metal_load_action_from_load_op(att.load_op);
			mtl_pass_desc.colorAttachments[color_att_counter].storeAction = metal_store_action_from_store_op(att.store_op);
			const auto dbl_clear_color = att.clear.color.cast<double>();
			mtl_pass_desc.colorAttachments[color_att_counter].clearColor = MTLClearColorMake(dbl_clear_color.x, dbl_clear_color.y,
																							 dbl_clear_color.z, dbl_clear_color.w);
			++color_att_counter;
		}
	}
	
	// success
	valid = true;
}

metal_pass::~metal_pass() {
	mtl_pass_desc = nil;
}

MTLLoadAction metal_pass::metal_load_action_from_load_op(const LOAD_OP& load_op) {
	switch (load_op) {
		case LOAD_OP::LOAD:
			return MTLLoadActionLoad;
		case LOAD_OP::CLEAR:
			return MTLLoadActionClear;
		case LOAD_OP::DONT_CARE:
			return MTLLoadActionDontCare;
	}
}

MTLStoreAction metal_pass::metal_store_action_from_store_op(const STORE_OP& store_op) {
	switch (store_op) {
		case STORE_OP::STORE:
			return MTLStoreActionStore;
		case STORE_OP::DONT_CARE:
			return MTLStoreActionDontCare;
	}
}

id <MTLRenderCommandEncoder> metal_pass::create_encoder(id <MTLCommandBuffer> cmd_buffer) const {
	return [cmd_buffer renderCommandEncoderWithDescriptor:mtl_pass_desc];
}

#endif
