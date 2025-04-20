/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/graphics_pass.hpp>
#include <floor/core/flat_map.hpp>
#include <memory>

namespace fl {

class device;

class vulkan_pass final : public graphics_pass {
public:
	vulkan_pass(const render_pass_description& pass_desc,
				const std::vector<std::unique_ptr<device>>& devices,
				const bool with_multi_view_support = false);
	~vulkan_pass() override;
	
	//! returns the Vulkan render pass object for the specified device
	VkRenderPass get_vulkan_render_pass(const device& dev, const bool get_multi_view) const;
	
	//! returns the attachment clear values defined for this pass
	const std::vector<VkClearValue>& get_vulkan_clear_values(const bool get_multi_view) const;
	
	//! returns true if this pass needs a clear / clear values, i.e. at least one load op is "clear"
	bool needs_clear() const {
		return has_any_clear_load_op;
	}
	
protected:
	struct vulkan_pass_t {
		VkRenderPass single_view_pass;
		VkRenderPass multi_view_pass;
	};
	fl::flat_map<const device*, vulkan_pass_t> render_passes;
	std::shared_ptr<std::vector<VkClearValue>> sv_clear_values;
	std::shared_ptr<std::vector<VkClearValue>> mv_clear_values;
	bool has_any_clear_load_op { false };
	
};

} // namespace fl

#endif
