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

#include <floor/device/metal/metal_pass.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/logger.hpp>

namespace fl {

static MTLRenderPassDescriptor* create_metal_render_pass_desc_from_description(const render_pass_description& desc,
																			   const bool is_multi_view) {
	MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
	
	size_t color_att_counter = 0;
	for (size_t i = 0, count = desc.attachments.size(); i < count; ++i) {
		const auto& att = desc.attachments[i];
		const auto is_multi_sampling = has_flag<IMAGE_TYPE::FLAG_MSAA>(att.format);
		const auto is_msaa_resolve = (att.store_op == STORE_OP::RESOLVE || att.store_op == STORE_OP::STORE_AND_RESOLVE);
		if (!is_multi_sampling && is_msaa_resolve) {
			log_warn("graphics_pass: MSAA resolve is set, but format is not MSAA");
		}
		
		if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(att.format)) {
			// depth attachment
			pass_desc.depthAttachment.loadAction = metal_pass::metal_load_action_from_load_op(att.load_op);
			pass_desc.depthAttachment.storeAction = metal_pass::metal_store_action_from_store_op(att.store_op);
			pass_desc.depthAttachment.clearDepth = double(att.clear.depth);
		} else {
			// color attachment
			pass_desc.colorAttachments[color_att_counter].loadAction = metal_pass::metal_load_action_from_load_op(att.load_op);
			pass_desc.colorAttachments[color_att_counter].storeAction = metal_pass::metal_store_action_from_store_op(att.store_op);
			const auto dbl_clear_color = att.clear.color.cast<double>();
			pass_desc.colorAttachments[color_att_counter].clearColor = MTLClearColorMake(dbl_clear_color.x, dbl_clear_color.y,
																						 dbl_clear_color.z, dbl_clear_color.w);
			++color_att_counter;
		}
	}
	if (is_multi_view) {
		pass_desc.renderTargetArrayLength = 2;
	}
	
	return pass_desc;
}

metal_pass::metal_pass(const render_pass_description& pass_desc_, const bool with_multi_view_support) :
graphics_pass(pass_desc_, with_multi_view_support) {
	const bool create_sv_pass = is_single_view_capable();
	const bool create_mv_pass = is_multi_view_capable();
	
	if (create_sv_pass) {
		sv_mtl_pass_desc = create_metal_render_pass_desc_from_description(pass_desc, false);
		if (!sv_mtl_pass_desc) {
			return;
		}
	}
	
	if (create_mv_pass) {
		mv_mtl_pass_desc = create_metal_render_pass_desc_from_description(!multi_view_pass_desc ? pass_desc : *multi_view_pass_desc, true);
		if (!mv_mtl_pass_desc) {
			return;
		}
	}
	
	// success
	valid = true;
}

metal_pass::~metal_pass() {
	@autoreleasepool {
		sv_mtl_pass_desc = nil;
		mv_mtl_pass_desc = nil;
	}
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
		case STORE_OP::RESOLVE:
			return MTLStoreActionMultisampleResolve;
		case STORE_OP::STORE_AND_RESOLVE:
			return MTLStoreActionStoreAndMultisampleResolve;
		case STORE_OP::DONT_CARE:
			return MTLStoreActionDontCare;
	}
}

id <MTLRenderCommandEncoder> metal_pass::create_encoder(id <MTLCommandBuffer> cmd_buffer,
														const bool create_multi_view) const {
	return [cmd_buffer renderCommandEncoderWithDescriptor:(!create_multi_view ?
														   sv_mtl_pass_desc : mv_mtl_pass_desc)];
}

} // namespace fl

#endif
