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

#include <floor/graphics/vulkan/vulkan_renderer.hpp>

#if !defined(FLOOR_NO_VULKAN)

vulkan_renderer::vulkan_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline_) :
graphics_renderer(cqueue_, pass_, pipeline_) {
	// TODO: implement this
}

vulkan_renderer::~vulkan_renderer() {
	// TODO: implement this
}

const graphics_renderer::drawable_t* vulkan_renderer::get_next_drawable() {
	// TODO: implement this
	return nullptr;
}

void vulkan_renderer::present() {
	// TODO: implement this
}

void vulkan_renderer::draw_internal(const vector<multi_draw_entry>* draw_entries floor_unused,
									const vector<multi_draw_indexed_entry>* draw_indexed_entries floor_unused,
									const vector<compute_kernel_arg>& args floor_unused) const {
	// TODO: implement this
}

#endif
