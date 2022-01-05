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

#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/floor/floor.hpp>

static optional<render_pipeline_description> multi_view_pipeline_modification(const render_pipeline_description& pipeline_desc) {
	auto mv_pipeline_desc = pipeline_desc;
	if (mv_pipeline_desc.automatic_multi_view_handling) {
		for (auto& att : mv_pipeline_desc.color_attachments) {
			if (att.automatic_multi_view_transformation && att.format != COMPUTE_IMAGE_TYPE::NONE) {
				att.format |= COMPUTE_IMAGE_TYPE::FLAG_ARRAY;
			}
		}
		if (mv_pipeline_desc.depth_attachment.automatic_multi_view_transformation &&
			mv_pipeline_desc.depth_attachment.format != COMPUTE_IMAGE_TYPE::NONE) {
			mv_pipeline_desc.depth_attachment.format |= COMPUTE_IMAGE_TYPE::FLAG_ARRAY;
		}
	}
	return mv_pipeline_desc;
}

graphics_pipeline::graphics_pipeline(const render_pipeline_description& pipeline_desc_, const bool with_multi_view_support) :
pipeline_desc(handle_pipeline_defaults(pipeline_desc_, with_multi_view_support ? !pipeline_desc_.automatic_multi_view_handling : false)),
multi_view_pipeline_desc(with_multi_view_support && pipeline_desc_.automatic_multi_view_handling ?
						 multi_view_pipeline_modification(handle_pipeline_defaults(pipeline_desc_, true)) : nullopt),
multi_view_capable(with_multi_view_support) {
	// TODO: check validity
}

graphics_pipeline::~graphics_pipeline() {
}

uint2 graphics_pipeline::compute_dim_from_screen_or_user(const uint2& in_size, const bool is_vr) {
	auto ret = in_size;
	if (in_size.x == ~0u || in_size.y == ~0u) {
		const auto phys_size = (!is_vr ? floor::get_physical_screen_size() : floor::get_vr_physical_screen_size());
		if (in_size.x == ~0u) {
			ret.x = phys_size.x;
		}
		if (in_size.y == ~0u) {
			ret.y = phys_size.y;
		}
	}
	return ret;
}

render_pipeline_description graphics_pipeline::handle_pipeline_defaults(const render_pipeline_description& pipeline_desc_, const bool is_vr) {
	render_pipeline_description ret = pipeline_desc_;
	ret.viewport = compute_dim_from_screen_or_user(ret.viewport, is_vr);
	if (ret.scissor.extent.x == ~0u) {
		ret.scissor.extent.x = ret.viewport.x;
	}
	if (ret.scissor.extent.y == ~0u) {
		ret.scissor.extent.y = ret.viewport.y;
	}
	return ret;
}
