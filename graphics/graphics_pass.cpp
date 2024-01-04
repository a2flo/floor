/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/graphics/graphics_pass.hpp>

static optional<render_pass_description> multi_view_pass_modification(const render_pass_description& pass_desc) {
	auto mv_pass_desc = pass_desc;
	if (mv_pass_desc.automatic_multi_view_handling) {
		for (auto& att : mv_pass_desc.attachments) {
			if (att.automatic_multi_view_transformation && att.format != COMPUTE_IMAGE_TYPE::NONE) {
				att.format |= COMPUTE_IMAGE_TYPE::FLAG_ARRAY;
			}
		}
	}
	return mv_pass_desc;
}

graphics_pass::graphics_pass(const render_pass_description& pass_desc_, const bool with_multi_view_support) :
pass_desc(pass_desc_), multi_view_pass_desc(with_multi_view_support && pass_desc_.automatic_multi_view_handling ?
											multi_view_pass_modification(pass_desc_) : nullopt), multi_view_capable(with_multi_view_support) {
	// TODO: check validity (more than one depth att, ...)
}
