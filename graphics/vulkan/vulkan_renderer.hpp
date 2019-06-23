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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_RENDERER_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_RENDERER_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/graphics/graphics_renderer.hpp>

class vulkan_renderer final : public graphics_renderer {
public:
	vulkan_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline_);
	~vulkan_renderer() override;

	const drawable_t* get_next_drawable() override;
	void present() override;

protected:
	void draw_internal(const vector<multi_draw_entry>* draw_entries,
					   const vector<multi_draw_indexed_entry>* draw_indexed_entries,
					   const vector<compute_kernel_arg>& args) const override;
	
};

#endif

#endif
