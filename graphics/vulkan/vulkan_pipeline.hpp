/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_PIPELINE_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_PIPELINE_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/vulkan/vulkan_pass.hpp>

class vulkan_pipeline final : public graphics_pipeline {
public:
	vulkan_pipeline(const render_pipeline_description& pipeline_desc,
					const vector<unique_ptr<compute_device>>& devices,
					const bool with_multi_view_support = false);
	~vulkan_pipeline() override;
	
	//! all Vulkan pipeline state
	struct vulkan_pipeline_state_t {
		VkPipeline pipeline { nullptr };
		VkPipelineLayout layout { nullptr };
		const compute_kernel::kernel_entry* vs_entry { nullptr };
		const compute_kernel::kernel_entry* fs_entry { nullptr };
	};

	//! Vulkan pipeline entry with single-view and multi-view rendering support
	struct vulkan_pipeline_entry_t {
		vulkan_pipeline_state_t single_view_pipeline {};
		vulkan_pipeline_state_t multi_view_pipeline {};
	};
	
	//! return the device specific Vulkan pipeline state for the specified device (or nullptr if it doesn't exist)
	const vulkan_pipeline_state_t* get_vulkan_pipeline_state(const compute_device& dev, const bool get_multi_view) const;
	
	//! returns the corresponding VkPrimitiveTopology for the specified PRIMITIVE
	static VkPrimitiveTopology vulkan_primitive_topology_from_primitive(const PRIMITIVE& primitive);
	
	//! returns the corresponding VkCullModeFlagBits for the specified CULL_MODE
	static VkCullModeFlagBits vulkan_cull_mode_from_cull_mode(const CULL_MODE& cull_mode);
	
	//! returns the corresponding VkFrontFace for the specified FRONT_FACE
	static VkFrontFace vulkan_front_face_from_front_face(const FRONT_FACE& front_face);
	
	//! returns the corresponding VkBlendFactor for the specified BLEND_FACTOR
	static VkBlendFactor vulkan_blend_factor_from_blend_factor(const BLEND_FACTOR& blend_factor);
	
	//! returns the corresponding VkBlendOp for the specified BLEND_OP
	static VkBlendOp vulkan_blend_op_from_blend_op(const BLEND_OP& blend_op);
	
	//! returns the corresponding VkCompareOp for the specified DEPTH_COMPARE
	static VkCompareOp vulkan_compare_op_from_depth_compare(const DEPTH_COMPARE& depth_compare);
	
protected:
	flat_map<const compute_device&, vulkan_pipeline_entry_t> pipelines;
	unique_ptr<vulkan_pass> sv_vulkan_base_pass;
	unique_ptr<vulkan_pass> mv_vulkan_base_pass;
	
};

#endif

#endif
