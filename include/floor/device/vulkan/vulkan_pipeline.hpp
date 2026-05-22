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

class vulkan_device;
struct vulkan_function_entry;

//! all Vulkan pipeline state
struct vulkan_pipeline_state_t {
	VkPipeline pipeline { nullptr };
	VkPipelineLayout layout { nullptr };
	vulkan_function_entry* vs_entry { nullptr };
	vulkan_function_entry* fs_entry { nullptr };
	vulkan_function_entry* ts_entry { nullptr };
	vulkan_function_entry* ms_entry { nullptr };
	
	bool is_valid() const {
		return (pipeline && layout && (vs_entry || ms_entry));
	}
};

class vulkan_pipeline final : public graphics_pipeline {
public:
	vulkan_pipeline(const render_pipeline_description& pipeline_desc,
					const std::vector<std::unique_ptr<device>>& devices,
					const bool with_multi_view_support = false);
	~vulkan_pipeline() override;
	
	//! key for a single vulkan_pipeline_state_t
	struct pipeline_key_t {
		//! NOTE: work-group sizes are stored as multiples of 16 -> 16 * #,
		//!       except for 0 which encodes 0 or 1 depending on the context
		//!       -> encodable values are 0, 1, [16, 2032], but we restrict it to 1024 max
		//! NOTE: as task/mesh work-group sizes generally need to stay <= 128, this is sufficient
		uint64_t task_wg_x : 7 { 0u };
		uint64_t task_wg_y : 7 { 0u };
		uint64_t task_wg_z : 7 { 0u };
		uint64_t mesh_wg_x : 7 { 0u };
		uint64_t mesh_wg_y : 7 { 0u };
		uint64_t mesh_wg_z : 7 { 0u };
		//! 0: none specialized, 1: 16, 2: 32, 3: 64, 4: 128, 5/6: unused
		//! NOTE: this *must* be the same for task and mesh shaders
		uint64_t simd : 3 { 0u };
		uint64_t is_multi_view : 1 { 0u };
		uint64_t is_indirect : 1 { 0u };
		uint64_t unused : 17 { 0u };
		
		explicit operator uint64_t() const {
			return std::bit_cast<uint64_t>(*this);
		}
		
		uint3 decode_task_local_size() const {
			return {
				task_wg_x == 0 ? 1u : task_wg_x * 16u,
				task_wg_y == 0 ? 1u : task_wg_y * 16u,
				task_wg_z == 0 ? 1u : task_wg_z * 16u,
			};
		}
		
		uint3 decode_mesh_local_size() const {
			return {
				mesh_wg_x == 0 ? 1u : mesh_wg_x * 16u,
				mesh_wg_y == 0 ? 1u : mesh_wg_y * 16u,
				mesh_wg_z == 0 ? 1u : mesh_wg_z * 16u,
			};
		}
		
		uint32_t decode_simd_width() const {
			switch (simd) {
				default: return 0u;
				case 1: return 16u;
				case 2: return 32u;
				case 3: return 64u;
				case 4: return 128u;
			}
		}
	};
	static_assert(sizeof(pipeline_key_t) == sizeof(uint64_t));
	
	//! returns the default device specific Vulkan pipeline state for the specified device + its key,
	//! creating it if it doesn't exist yet, returns invalid pipeline state on error (check is_valid())
	//! NOTE: the remaining pipeline_key_t values are the ones used to compile each pipeline function, or platform defaults if unspecified
	std::pair<vulkan_pipeline_state_t, pipeline_key_t>
	get_or_create_pipeline_state(const device& dev,
								 const bool is_multi_view,
								 const bool is_indirect) const REQUIRES(!pipelines_lock);
	
	//! returns the device specific Vulkan pipeline state for the specified device and pipeline key,
	//! creating it if it doesn't exist yet, returns invalid pipeline state on error (check is_valid())
	//! NOTE: mesh/task work-group sizes must be specified for this, SIMD may be 0, in which case the default or compiled SIMD is used
	std::pair<vulkan_pipeline_state_t, pipeline_key_t>
	get_or_create_pipeline_state(const device& dev, const pipeline_key_t key) const REQUIRES(!pipelines_lock);
	
	//! creates a pipeline key for the specified device based on the specified parameters, the compiled shader information,
	//! device limits, and pipeline description -> uses defaults or required sizes where necessary (with fallbacks)
	//! NOTE: check returned key for which sizes where actually used/set
	//! NOTE: throws if there is an unrecoverable error (e.g. unsupported sizes)
	pipeline_key_t create_key(const device& dev, const bool multi_view, const bool indirect,
							  const uint3 task_wg_size = {}, const uint3 mesh_wg_size = {}, const uint32_t simd_width = 0u) const;
	
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
	struct vulkan_internal_pipeline_state_t {
		VkPipeline pipeline { nullptr };
		VkPipelineLayout layout { nullptr };
	};
	vulkan_pipeline_state_t create_pipeline_state(const vulkan_internal_pipeline_state_t& internal_state) const;
	
	mutable safe_mutex pipelines_lock;
	mutable fl::flat_map<const device*, std::unordered_map<uint64_t, vulkan_internal_pipeline_state_t>>
	pipelines GUARDED_BY(pipelines_lock);
	
	std::unique_ptr<vulkan_pass> sv_vulkan_base_pass;
	std::unique_ptr<vulkan_pass> mv_vulkan_base_pass;
	
	vulkan_function_entry* vk_vs_entry { nullptr };
	vulkan_function_entry* vk_fs_entry { nullptr };
	vulkan_function_entry* vk_ts_entry { nullptr };
	vulkan_function_entry* vk_ms_entry { nullptr };
	
	//! per-device default keys for common configurations
	struct default_pipeline_keys_t {
		pipeline_key_t single_view_key {};
		pipeline_key_t multi_view_key {};
		pipeline_key_t indirect_single_key {};
		pipeline_key_t indirect_multi_key {};
	};
	fl::flat_map<const device*, default_pipeline_keys_t> device_default_keys;
	
	bool create_vulkan_pipeline(vulkan_internal_pipeline_state_t& state,
								const pipeline_key_t key,
								const vulkan_pass& vulkan_base_pass,
								const render_pipeline_description& pipeline_desc,
								const vulkan_device& vk_dev) const;
	
};

} // namespace fl

#endif
