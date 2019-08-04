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

#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/floor/floor.hpp>

graphics_pipeline::graphics_pipeline(const render_pipeline_description& pipeline_desc_) : pipeline_desc(handle_pipeline_defaults(pipeline_desc_)) {
	// TODO: check validity
}

graphics_pipeline::~graphics_pipeline() {
}

uint2 graphics_pipeline::compute_dim_from_screen_or_user(const uint2& in_size) {
	auto ret = in_size;
	if (in_size.x == ~0u || in_size.y == ~0u) {
		const auto phys_size = floor::get_physical_screen_size();
		if (in_size.x == ~0u) {
			ret.x = phys_size.x;
		}
		if (in_size.y == ~0u) {
			ret.y = phys_size.y;
		}
	}
	return ret;
}

render_pipeline_description graphics_pipeline::handle_pipeline_defaults(const render_pipeline_description& pipeline_desc_) {
	render_pipeline_description ret = pipeline_desc_;
	ret.viewport = compute_dim_from_screen_or_user(ret.viewport);
	if (ret.scissor.extent.x == ~0u) {
		ret.scissor.extent.x = ret.viewport.x;
	}
	if (ret.scissor.extent.y == ~0u) {
		ret.scissor.extent.y = ret.viewport.y;
	}
	return ret;
}
