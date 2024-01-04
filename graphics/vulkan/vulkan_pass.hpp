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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_PASS_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_PASS_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/graphics/graphics_pass.hpp>
#include <floor/core/flat_map.hpp>
#include <memory>

class compute_device;

class vulkan_pass final : public graphics_pass {
public:
	vulkan_pass(const render_pass_description& pass_desc,
				const vector<unique_ptr<compute_device>>& devices,
				const bool with_multi_view_support = false);
	~vulkan_pass() override;
	
	//! returns the corresponding VkAttachmentLoadOp for the specified LOAD_OP
	static VkAttachmentLoadOp vulkan_load_op_from_load_op(const LOAD_OP& load_op);
	
	//! returns the corresponding VkAttachmentStoreOp for the specified STORE_OP
	static VkAttachmentStoreOp vulkan_store_op_from_store_op(const STORE_OP& store_op);
	
	//! returns the Vulkan render pass object for the specified device
	VkRenderPass get_vulkan_render_pass(const compute_device& dev, const bool get_multi_view) const;
	
	//! returns the attachment clear values defined for this pass
	const vector<VkClearValue>& get_vulkan_clear_values(const bool get_multi_view) const {
		return (!get_multi_view ? sv_clear_values : mv_clear_values);
	}
	
	//! returns true if this pass needs a clear / clear values, i.e. at least one load op is "clear"
	bool needs_clear() const {
		return has_any_clear_load_op;
	}
	
protected:
	struct vulkan_pass_t {
		VkRenderPass single_view_pass;
		VkRenderPass multi_view_pass;
	};
	floor_core::flat_map<const compute_device&, vulkan_pass_t> render_passes;
	vector<VkClearValue> sv_clear_values;
	vector<VkClearValue> mv_clear_values;
	bool has_any_clear_load_op { false };
	
};

#endif

#endif
