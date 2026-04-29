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

#pragma once

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/device_function.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <floor/device/vulkan/vulkan_pass.hpp>

namespace fl {

//! all Vulkan pipeline state
struct vulkan_pipeline_state_t {
	VkPipeline pipeline { nullptr };
	VkPipelineLayout layout { nullptr };
	const device_function::function_entry* vs_entry { nullptr };
	const device_function::function_entry* fs_entry { nullptr };
	const device_function::function_entry* ts_entry { nullptr };
	const device_function::function_entry* ms_entry { nullptr };
};

class vulkan_pipeline final : public graphics_pipeline {
public:
	vulkan_pipeline(const render_pipeline_description& pipeline_desc,
					const std::vector<std::unique_ptr<device>>& devices,
					const bool with_multi_view_support = false);
	~vulkan_pipeline() override;
	
	//! Vulkan pipeline entry with single-view and multi-view rendering support
	struct vulkan_pipeline_entry_t {
		vulkan_pipeline_state_t single_view_pipeline {};
		vulkan_pipeline_state_t multi_view_pipeline {};
		vulkan_pipeline_state_t indirect_single_view_pipeline {};
		vulkan_pipeline_state_t indirect_multi_view_pipeline {};
	};
	
	//! return the device specific Vulkan pipeline state for the specified device (or nullptr if it doesn't exist)
	const vulkan_pipeline_state_t* get_vulkan_pipeline_state(const device& dev,
															 const bool get_multi_view,
															 const bool get_indirect) const;
	
	//! descriptor set layout/indices:
	//!   high count:
	//!     * samplers: [0]
	//!     * VS: [1] + [3, 8]
	//!     * TS: [1] + [4, 5]
	//!     * MS: [3] + [6, 8]
	//!     * FS: [2] + [9, 14]
	//!
	//!   low count:
	//!     * samplers: [0]
	//!     * VS: [1] + [3, 4]
	//!     * TS: [1] + N/A
	//!     * MS: [3] + N/A
	//!       * [4] unused if TS/MS
	//!     * FS: [2] + [5, 6]
	
	//! the descriptor set index where vertex shader argument buffers start at
	static constexpr const uint32_t argument_buffer_vs_start_set { 3u };
	//! the descriptor set index where task shader argument buffers start at
	static constexpr const uint32_t argument_buffer_ts_start_set_high { 4u };
	//! the descriptor set index where mesh shader argument buffers start at
	static constexpr const uint32_t argument_buffer_ms_start_set_high { 6u };
	//! the descriptor set index where fragment shader argument buffers start at
	static constexpr const uint32_t argument_buffer_fs_start_set_low { 5u };
	static constexpr const uint32_t argument_buffer_fs_start_set_high { 9u };
	
	//! returns the underlying/associated vulkan_pass for this pipeline,
	//! returns nullptr if there is none
	const vulkan_pass* get_vulkan_pass(const bool get_multi_view) const;
	
protected:
	fl::flat_map<const device*, vulkan_pipeline_entry_t> pipelines;
	std::unique_ptr<vulkan_pass> sv_vulkan_base_pass;
	std::unique_ptr<vulkan_pass> mv_vulkan_base_pass;
	
};

} // namespace fl

#endif
