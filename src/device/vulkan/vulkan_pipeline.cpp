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

#include <floor/device/vulkan/vulkan_pipeline.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_function.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_disassembly.hpp"
#include "internal/vulkan_descriptor_set.hpp"
#include "internal/vulkan_function_entry.hpp"
#include "internal/vulkan_debug.hpp"
#include "internal/vulkan_conversion.hpp"

namespace fl {

static std::unique_ptr<vulkan_pass> create_vulkan_base_pass_desc(const render_pipeline_description& pipeline_desc,
																 const std::vector<std::unique_ptr<device>>& devices,
																 const bool is_multi_view) {
	render_pass_description base_pass_desc {};
	if (is_multi_view) {
		// disable automatic transformation, since we already have made it
		base_pass_desc.automatic_multi_view_handling = false;
	}
	const IMAGE_TYPE msaa_format = (pipeline_desc.sample_count > 1 ?
									(IMAGE_TYPE::FLAG_MSAA | image_sample_type_from_count(pipeline_desc.sample_count)) :
									IMAGE_TYPE::NONE);
	for (const auto& color_att : pipeline_desc.color_attachments) {
		base_pass_desc.attachments.emplace_back(render_pass_description::attachment_desc_t {
			.format = color_att.format | msaa_format,
			.store_op = (pipeline_desc.sample_count > 1 ? STORE_OP::RESOLVE : STORE_OP::STORE),
			// NOTE: load op, clear color/depth do not matter (any combination is compatible)
		});
	}
	if (pipeline_desc.depth_attachment.format != IMAGE_TYPE::NONE) {
		base_pass_desc.attachments.emplace_back(render_pass_description::attachment_desc_t {
			.format = pipeline_desc.depth_attachment.format
		});
	}
	auto vulkan_base_pass = std::make_unique<vulkan_pass>(base_pass_desc, devices, is_multi_view);
	if (!vulkan_base_pass || !vulkan_base_pass->is_valid()) {
		log_error("failed to create$ Vulkan base pass for pipeline", (is_multi_view ? " multi-view" : ""));
		return {};
	}
	floor_return_no_nrvo(vulkan_base_pass);
}

vulkan_pipeline::vulkan_pipeline(const render_pipeline_description& pipeline_desc_, const std::vector<std::unique_ptr<device>>& devices,
								 const bool with_multi_view_support) :
graphics_pipeline(pipeline_desc_, with_multi_view_support) {
	const bool create_sv_pipeline = is_single_view_capable();
	const bool create_mv_pipeline = is_multi_view_capable();
	
	// Vulkan requires an actual render pass for pipeline creation, it is however allowed to use a compatible render pass later on
	// -> create a base render pass for this pipeline, since we can't access the actual corresponding render pass here (and there might be multiple ones)
	sv_vulkan_base_pass = (create_sv_pipeline ? create_vulkan_base_pass_desc(pipeline_desc, devices, false) : nullptr);
	mv_vulkan_base_pass = (create_mv_pipeline ? create_vulkan_base_pass_desc((multi_view_pipeline_desc ?
																			  *multi_view_pipeline_desc : pipeline_desc),
																			 devices, true) : nullptr);
	
	const auto vk_vs = (const vulkan_function*)pipeline_desc.vertex_shader;
	const auto vk_fs = (const vulkan_function*)pipeline_desc.fragment_shader;
	const auto vk_ts = (const vulkan_function*)pipeline_desc.task_shader;
	const auto vk_ms = (const vulkan_function*)pipeline_desc.mesh_shader;
	
	for (const auto& dev : devices) {
		// validate pipeline + function combinations for each device
		vk_vs_entry = (vk_vs != nullptr ? vk_vs->get_mutable_function_entry(*dev) : nullptr);
		vk_fs_entry = (vk_fs != nullptr ? vk_fs->get_mutable_function_entry(*dev) : nullptr);
		vk_ts_entry = (vk_ts != nullptr ? vk_ts->get_mutable_function_entry(*dev) : nullptr);
		vk_ms_entry = (vk_ms != nullptr ? vk_ms->get_mutable_function_entry(*dev) : nullptr);
		
		if (vk_vs_entry) {
			if (vk_vs_entry->info->type != toolchain::FUNCTION_TYPE::VERTEX) {
				log_error("expected a vertex shader instead of $ ($) in pipeline \"$\"",
						  vk_vs_entry->info->type, vk_vs_entry->info->name, pipeline_desc.debug_label);
				return;
			}
		} else {
			if (!vk_ms_entry) {
				log_error("expected either a vertex shader or a mesh shader in pipeline \"$\"",
						  pipeline_desc_.debug_label);
				return;
			} else if (vk_ms_entry->info->type != toolchain::FUNCTION_TYPE::MESH) {
				log_error("expected a mesh shader instead of $ ($) in pipeline \"$\"",
						  vk_ms_entry->info->type, vk_ms_entry->info->name, pipeline_desc.debug_label);
				return;
			}
			if (vk_ts_entry && vk_ts_entry->info->type != toolchain::FUNCTION_TYPE::TASK) {
				log_error("expected a task shader instead of $ ($) in pipeline \"$\"",
						  vk_ts_entry->info->type, vk_ts_entry->info->name, pipeline_desc.debug_label);
				return;
			}
		}
		
		// prime pipelines for this device
		pipelines.emplace(dev.get(), std::unordered_map<uint64_t, vulkan_internal_pipeline_state_t> {});
		
		// create pipelines + save their default keys
		default_pipeline_keys_t default_keys {};
		pipeline_key_t base_key = create_key(*dev, false, false);
		
		if (create_sv_pipeline) {
			auto sv_d_base_key = base_key;
			if (auto sv_d = get_or_create_pipeline_state(*dev, sv_d_base_key); sv_d.first.is_valid()) {
				default_keys.single_view_key = sv_d.second;
			} else {
				return;
			}
			
			if (pipeline_desc.options.support_indirect_rendering) {
				auto sv_id_base_key = base_key;
				sv_id_base_key.is_indirect = true;
				if (auto sv_id = get_or_create_pipeline_state(*dev, sv_id_base_key); sv_id.first.is_valid()) {
					default_keys.indirect_single_key = sv_id.second;
				} else {
					return;
				}
			}
		}
		
		if (create_mv_pipeline) {
			auto mv_d_base_key = base_key;
			mv_d_base_key.is_multi_view = true;
			if (auto mv_d = get_or_create_pipeline_state(*dev, mv_d_base_key); mv_d.first.is_valid()) {
				default_keys.multi_view_key = mv_d.second;
			} else {
				return;
			}
			
			if (pipeline_desc.options.support_indirect_rendering) {
				auto mv_id_base_key = base_key;
				mv_id_base_key.is_multi_view = true;
				mv_id_base_key.is_indirect = true;
				if (auto mv_id = get_or_create_pipeline_state(*dev, mv_id_base_key); mv_id.first.is_valid()) {
					default_keys.indirect_multi_key = mv_id.second;
				} else {
					return;
				}
			}
		}
		
		device_default_keys.emplace(dev.get(), default_keys);
	}
	
	// success
	valid = true;
}

vulkan_pipeline::~vulkan_pipeline() {
	// TODO: implement this
}

struct mesh_shading_spec_t {
	VkSpecializationInfo info;
	uint3 wg_size;
	VkPipelineShaderStageRequiredSubgroupSizeCreateInfo sub_group_info;
	
	static constexpr std::array<VkSpecializationMapEntry, 3u> map_entries {{
		{ .constantID = 1, .offset = sizeof(uint32_t) * 0, .size = sizeof(uint32_t) },
		{ .constantID = 2, .offset = sizeof(uint32_t) * 1, .size = sizeof(uint32_t) },
		{ .constantID = 3, .offset = sizeof(uint32_t) * 2, .size = sizeof(uint32_t) },
	}};
};
static void update_ts_ms_stage_info_for_key(VkPipelineShaderStageCreateInfo& stage_info,
											mesh_shading_spec_t& spec_entry,
											const vulkan_pipeline::pipeline_key_t key,
											const bool is_mesh_shader) {
	// set local size
	spec_entry.wg_size = (is_mesh_shader ? key.decode_mesh_local_size() : key.decode_task_local_size());
	spec_entry.info = {
		.mapEntryCount = uint32_t(mesh_shading_spec_t::map_entries.size()),
		.pMapEntries = mesh_shading_spec_t::map_entries.data(),
		.dataSize = sizeof(decltype(spec_entry.wg_size)),
		.pData = spec_entry.wg_size.data(),
	};
	stage_info.pSpecializationInfo = &spec_entry.info;
	
	// set/update SIMD width
	const auto simd_width = key.decode_simd_width();
	spec_entry.sub_group_info = VkPipelineShaderStageRequiredSubgroupSizeCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
		.pNext = nullptr,
		.requiredSubgroupSize = simd_width,
	};
	assert(stage_info.pNext);
	const_cast<VkShaderModuleCreateInfo*>((const VkShaderModuleCreateInfo*)stage_info.pNext)->pNext = &spec_entry.sub_group_info;
	
	if ((spec_entry.wg_size.x % simd_width) == 0u) {
		stage_info.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
	}
}

bool vulkan_pipeline::create_vulkan_pipeline(vulkan_internal_pipeline_state_t& state,
											 const pipeline_key_t key,
											 const vulkan_pass& vulkan_base_pass,
											 const render_pipeline_description& desc,
											 const vulkan_device& vk_dev) const {
	assert(vk_vs_entry != nullptr || vk_ms_entry != nullptr);
	
	// tessellation is not supported
	if (desc.tessellation.max_factor > 0u) {
		log_error("tessellation is not supported by the Vulkan backend");
		return false;
	}
	
	// create the pipeline layout
	std::vector<VkDescriptorSetLayout> desc_set_layouts {
		vk_dev.fixed_sampler_desc_set_layout
	};
	
	// set argument buffer descriptor set layouts + fill unused sets with an empty descriptor set
	// NOTE: Vulkan has no way of specifying explicit descriptor set offsets, so we must specify everything in range, even if unused
	assert(!(vk_vs_entry && vk_ms_entry));
	const auto has_arg_buffers_vs = (vk_vs_entry && !vk_vs_entry->argument_buffers.empty());
	const auto has_arg_buffers_ts = (vk_ts_entry && !vk_ts_entry->argument_buffers.empty());
	const auto has_arg_buffers_ms = (vk_ms_entry && !vk_ms_entry->argument_buffers.empty());
	const auto has_arg_buffers_fs = (vk_fs_entry && !vk_fs_entry->argument_buffers.empty());
	const auto has_empty_mesh_stages = (vk_ms_entry && (!vk_ts_entry || !vk_fs_entry));
	const auto empty_desc_set = (has_arg_buffers_vs || has_arg_buffers_ts || has_arg_buffers_ms || has_arg_buffers_fs || has_empty_mesh_stages ?
								 vulkan_program::get_empty_descriptor_set(vk_dev) : nullptr);
	if (vk_vs_entry) {
		desc_set_layouts.emplace_back(vk_vs_entry->desc_set_layout);
		if (vk_fs_entry != nullptr) {
			desc_set_layouts.emplace_back(vk_fs_entry->desc_set_layout);
		}
	} else {
		assert(vk_ms_entry);
		desc_set_layouts.emplace_back(vk_ts_entry ? vk_ts_entry->desc_set_layout : empty_desc_set);
		desc_set_layouts.emplace_back(vk_fs_entry ? vk_fs_entry->desc_set_layout : empty_desc_set);
		desc_set_layouts.emplace_back(vk_ms_entry->desc_set_layout);
	}
	if (vk_vs_entry) {
		if (has_arg_buffers_vs) {
			desc_set_layouts.resize(vulkan_pipeline::argument_buffer_vs_start_set, empty_desc_set);
			for (const auto& arg_buf : vk_vs_entry->argument_buffers) {
				desc_set_layouts.emplace_back(arg_buf.layout.desc_set_layout);
			}
		}
	} else {
		if (has_arg_buffers_ts) {
			assert(!has_flag<toolchain::FUNCTION_FLAGS::VULKAN_LOW_DS>(vk_ts_entry->info->flags));
			desc_set_layouts.resize(vulkan_pipeline::argument_buffer_ts_start_set_high, empty_desc_set);
			for (const auto& arg_buf : vk_ts_entry->argument_buffers) {
				desc_set_layouts.emplace_back(arg_buf.layout.desc_set_layout);
			}
		}
		if (has_arg_buffers_ms) {
			assert(!has_flag<toolchain::FUNCTION_FLAGS::VULKAN_LOW_DS>(vk_ms_entry->info->flags));
			desc_set_layouts.resize(vulkan_pipeline::argument_buffer_ms_start_set_high, empty_desc_set);
			for (const auto& arg_buf : vk_ms_entry->argument_buffers) {
				desc_set_layouts.emplace_back(arg_buf.layout.desc_set_layout);
			}
		}
	}
	if (has_arg_buffers_fs) {
		desc_set_layouts.resize(has_flag<toolchain::FUNCTION_FLAGS::VULKAN_LOW_DS>(vk_fs_entry->info->flags) ?
								vulkan_pipeline::argument_buffer_fs_start_set_low :
								vulkan_pipeline::argument_buffer_fs_start_set_high, empty_desc_set);
		for (const auto& arg_buf : vk_fs_entry->argument_buffers) {
			desc_set_layouts.emplace_back(arg_buf.layout.desc_set_layout);
		}
	}
	const VkPipelineLayoutCreateInfo pipeline_layout_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = uint32_t(desc_set_layouts.size()),
		.pSetLayouts = desc_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
	VK_CALL_RET(vkCreatePipelineLayout(vk_dev.device, &pipeline_layout_info, nullptr, &state.layout),
				"failed to create pipeline layout", false)
#if defined(FLOOR_DEBUG)
	if (!desc.debug_label.empty()) {
		set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_PIPELINE_LAYOUT, uint64_t(state.layout), "layout:" + desc.debug_label);
	}
#endif
	
	// setup the pipeline
	const VkPipelineVertexInputStateCreateInfo vertex_input_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		// unnecessary when using SSBOs
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};
	const VkPipelineInputAssemblyStateCreateInfo input_assembly_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = vulkan_primitive_topology_from_primitive(desc.primitive),
		.primitiveRestartEnable = false,
	};
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)desc.viewport.x,
		.height = (float)desc.viewport.y,
		.minDepth = desc.depth.range.x,
		.maxDepth = desc.depth.range.y,
	};
	const VkRect2D scissor_rect {
		// NOTE: Vulkan uses signed integers for the offset, but doesn't actually allow it to be < 0
		.offset = { int(desc.scissor.offset.x), int(desc.scissor.offset.y) },
		.extent = { desc.scissor.extent.x, desc.scissor.extent.y },
	};
	const VkPipelineViewportStateCreateInfo viewport_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor_rect,
	};
	const VkPipelineRasterizationStateCreateInfo raster_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = false,
		.rasterizerDiscardEnable = false,
		.polygonMode = (!desc.options.render_wireframe ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE),
		.cullMode = vulkan_cull_mode_from_cull_mode(desc.cull_mode),
		.frontFace = vulkan_front_face_from_front_face(desc.front_face),
		.depthBiasEnable = false,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisample_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = sample_count_to_vulkan_sample_count(desc.sample_count),
		.sampleShadingEnable = false,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = false,
		.alphaToOneEnable = false,
	};
	
	// set color attachments
	std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
	for (const auto& color_att : desc.color_attachments) {
		if (color_att.format == IMAGE_TYPE::NONE) {
			log_error("color attachment image type must not be NONE!"); // TODO: -> move to prior validity check
			return false;
		}
		
		VkColorComponentFlags mask = 0;
		if (color_att.blend.write_mask.x) {
			mask |= VK_COLOR_COMPONENT_R_BIT;
		}
		if (color_att.blend.write_mask.y) {
			mask |= VK_COLOR_COMPONENT_G_BIT;
		}
		if (color_att.blend.write_mask.z) {
			mask |= VK_COLOR_COMPONENT_B_BIT;
		}
		if (color_att.blend.write_mask.w) {
			mask |= VK_COLOR_COMPONENT_A_BIT;
		}
		
		color_blend_attachment_states.emplace_back(VkPipelineColorBlendAttachmentState {
			.blendEnable = color_att.blend.enable,
			.srcColorBlendFactor = vulkan_blend_factor_from_blend_factor(color_att.blend.src_color_factor),
			.dstColorBlendFactor = vulkan_blend_factor_from_blend_factor(color_att.blend.dst_color_factor),
			.colorBlendOp = vulkan_blend_op_from_blend_op(color_att.blend.color_blend_op),
			.srcAlphaBlendFactor = vulkan_blend_factor_from_blend_factor(color_att.blend.src_alpha_factor),
			.dstAlphaBlendFactor = vulkan_blend_factor_from_blend_factor(color_att.blend.dst_alpha_factor),
			.alphaBlendOp = vulkan_blend_op_from_blend_op(color_att.blend.alpha_blend_op),
			.colorWriteMask = mask,
		});
	}
	const VkPipelineColorBlendStateCreateInfo color_blend_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.logicOp = VK_LOGIC_OP_CLEAR,
		.attachmentCount = uint32_t(color_blend_attachment_states.size()),
		.pAttachments = (!color_blend_attachment_states.empty() ? color_blend_attachment_states.data() : nullptr),
		.blendConstants = {
			desc.blend.constant_color.x,
			desc.blend.constant_color.y,
			desc.blend.constant_color.z,
			desc.blend.constant_alpha
		},
	};
	
	// set optional depth attachment
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	const bool has_depth_attachment = (desc.depth_attachment.format != IMAGE_TYPE::NONE);
	if (has_depth_attachment) {
		depth_stencil_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = (desc.depth.compare != DEPTH_COMPARE::ALWAYS),
			.depthWriteEnable = desc.depth.write,
			.depthCompareOp = vulkan_compare_op_from_depth_compare(desc.depth.compare),
			.depthBoundsTestEnable = false,
			.stencilTestEnable = false,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 0.0f,
		};
	}
	
	// create pipeline
	std::array<VkPipelineShaderStageCreateInfo, 3> stages {};
	mesh_shading_spec_t ts_spec_entry, ms_spec_entry;
	uint32_t active_stages = 0u;
	if (vk_vs_entry) {
		stages[active_stages++] = vk_vs_entry->stage_info;
	} else {
		if (vk_ts_entry) {
			auto ts_stage_info = vk_ts_entry->stage_info;
			update_ts_ms_stage_info_for_key(ts_stage_info, ts_spec_entry, key, false);
			stages[active_stages++] = ts_stage_info;
		}
		auto ms_stage_info = vk_ms_entry->stage_info;
		update_ts_ms_stage_info_for_key(ms_stage_info, ms_spec_entry, key, true);
		stages[active_stages++] = ms_stage_info;
	}
	if (vk_fs_entry != nullptr) {
		stages[active_stages++] = vk_fs_entry->stage_info;
	}
	
	const auto render_pass = vulkan_base_pass.get_vulkan_render_pass(vk_dev, key.is_multi_view);
	if (render_pass == nullptr) {
		log_error("no base render pass for device $", vk_dev.name);
		return false;
	}
	
	VkPipelineCreateFlags pipeline_flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	
	// if binaries should be logged/dumped, create a pipeline cache from which we'll extract the binary
	VkPipelineCache cache { nullptr };
	const bool log_binary = ((vk_vs_entry ? vulkan_function::should_log_vulkan_binary(vk_vs_entry->info->name) : false) ||
							 (vk_ts_entry ? vulkan_function::should_log_vulkan_binary(vk_ts_entry->info->name) : false) ||
							 (vk_ms_entry ? vulkan_function::should_log_vulkan_binary(vk_ms_entry->info->name) : false) ||
							 (vk_fs_entry ? vulkan_function::should_log_vulkan_binary(vk_fs_entry->info->name) : false));
	if (log_binary) {
		const VkPipelineCacheCreateInfo cache_create_info {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
			.initialDataSize = 0,
			.pInitialData = nullptr,
		};
		vkCreatePipelineCache(vk_dev.device, &cache_create_info, nullptr, &cache);
		
		pipeline_flags |= VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR;
		pipeline_flags |= VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
	}
	
	// determine all required dynamic change:
	// for indirect pipelines (-> secondary command buffers later on), we can't (or need to) use dynamic state
	std::array<VkDynamicState, 3> dyn_state_arr;
	uint32_t active_dyn_state_size = 0u;
	if (!key.is_indirect) {
		dyn_state_arr[active_dyn_state_size++] = VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT;
		dyn_state_arr[active_dyn_state_size++] = VkDynamicState::VK_DYNAMIC_STATE_SCISSOR;
		if (desc.options.dynamic_cull_state) {
			dyn_state_arr[active_dyn_state_size++] = VkDynamicState::VK_DYNAMIC_STATE_CULL_MODE;
		}
	}
	const VkPipelineDynamicStateCreateInfo dyn_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.dynamicStateCount = active_dyn_state_size,
		.pDynamicStates = &dyn_state_arr[0],
	};
	
	const VkGraphicsPipelineCreateInfo gfx_pipeline_info {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = pipeline_flags,
		.stageCount = active_stages,
		.pStages = stages.data(),
		.pVertexInputState = &vertex_input_state,
		.pInputAssemblyState = &input_assembly_state,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &raster_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = (has_depth_attachment ? &depth_stencil_state : nullptr),
		.pColorBlendState = &color_blend_state,
		.pDynamicState = (active_dyn_state_size > 0 ? &dyn_state : nullptr),
		.layout = state.layout,
		.renderPass = render_pass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0,
	};
	VK_CALL_RET(vkCreateGraphicsPipelines(vk_dev.device, cache, 1, &gfx_pipeline_info, nullptr, &state.pipeline),
				"failed to create pipeline", false)
#if defined(FLOOR_DEBUG)
	if (!desc.debug_label.empty()) {
		set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_PIPELINE, uint64_t(state.pipeline), desc.debug_label);
	}
#endif
	
	if (cache && log_binary) {
		vulkan_disassembly::disassemble(vk_dev, (vk_vs_entry ? vk_vs_entry->info->name : vk_ms_entry->info->name) +
										(vk_fs_entry ? "_" + vk_fs_entry->info->name : ""), state.pipeline, &cache);
		vkDestroyPipelineCache(vk_dev.device, cache, nullptr);
	}
	
	return true;
}

std::pair<vulkan_pipeline_state_t, vulkan_pipeline::pipeline_key_t>
vulkan_pipeline::get_or_create_pipeline_state(const device& dev, const bool is_multi_view, const bool is_indirect) const {
	auto keys_iter = device_default_keys.find(&dev);
	if (keys_iter == device_default_keys.end()) {
		log_error("pipeline $ was not initialized for device $", pipeline_desc.debug_label, dev.name);
		return { {}, {} };
	}
	return get_or_create_pipeline_state(dev, !is_multi_view ?
										(!is_indirect ? keys_iter->second.single_view_key : keys_iter->second.indirect_single_key) :
										(!is_indirect ? keys_iter->second.multi_view_key : keys_iter->second.indirect_multi_key));
}

std::pair<vulkan_pipeline_state_t, vulkan_pipeline::pipeline_key_t>
vulkan_pipeline::get_or_create_pipeline_state(const device& dev, const pipeline_key_t key) const {
	GUARD(pipelines_lock);
	
	auto dev_iter = pipelines.find(&dev);
	if (dev_iter == pipelines.end()) {
		log_error("pipeline $ was not initialized for device $", pipeline_desc.debug_label, dev.name);
		return { {}, {} };
	}
	auto& dev_pipelines = dev_iter->second;
	
	auto pipeline_iter = dev_pipelines.find(uint64_t(key));
	if (pipeline_iter != dev_pipelines.end()) {
		return { create_pipeline_state(pipeline_iter->second), key };
	}
	
	vulkan_internal_pipeline_state_t pipeline_state {};
	if (create_vulkan_pipeline(pipeline_state, key,
							   key.is_multi_view ? *mv_vulkan_base_pass : *sv_vulkan_base_pass,
							   pipeline_desc, (const vulkan_device&)dev)) [[likely]] {
		auto [created_pipeline_iter, success] = dev_pipelines.emplace(key, std::move(pipeline_state));
		if (success) [[likely]] {
			return { create_pipeline_state(created_pipeline_iter->second), key };
		} else {
			log_error("failed to insert created pipeline $ for device $", pipeline_desc.debug_label, dev.name);
		}
	} else {
		log_error("failed to create pipeline $ for device $", pipeline_desc.debug_label, dev.name);
	}
	
	return { {}, {} };
}

vulkan_pipeline::pipeline_key_t vulkan_pipeline::create_key(const device& dev, const bool multi_view, const bool indirect,
															const uint3 task_wg_size_, const uint3 mesh_wg_size_,
															const uint32_t simd_width_) const {
	pipeline_key_t key {
		.is_multi_view = multi_view,
		.is_indirect = indirect,
	};
	
	if (vk_ms_entry) {
		// validate specified local sizes, reset and continue if invalid
		uint3 task_wg_size = task_wg_size_;
		if (vk_ts_entry && vk_ts_entry->info->has_valid_required_local_size() &&
			(!task_wg_size.is_null() && task_wg_size != vk_ts_entry->info->required_local_size)) {
			log_error("unsupported task shader local size: $ when $ is required",
					  task_wg_size, vk_ts_entry->info->required_local_size);
			task_wg_size = {};
		}
		
		uint3 mesh_wg_size = mesh_wg_size_;
		if (vk_ms_entry->info->has_valid_required_local_size() &&
			(!mesh_wg_size.is_null() && mesh_wg_size != vk_ms_entry->info->required_local_size)) {
			log_error("unsupported mesh shader local size: $ when $ is required",
					  mesh_wg_size, vk_ms_entry->info->required_local_size);
			mesh_wg_size = {};
		}
		
		// set required sizes
		if (vk_ts_entry && (task_wg_size.is_null() || vk_ts_entry->info->has_valid_required_local_size())) {
			task_wg_size = vk_ts_entry->info->required_local_size;
		}
		
		if (mesh_wg_size.is_null() || vk_ms_entry->info->has_valid_required_local_size()) {
			mesh_wg_size = vk_ms_entry->info->required_local_size;
		}
		
		// handle/validate SIMD
		uint32_t req_simd_width = 0u;
		if (vk_ts_entry && vk_ts_entry->info->has_valid_required_simd_width()) {
			req_simd_width = vk_ts_entry->info->required_simd_width;
		}
		if (vk_ms_entry->info->has_valid_required_simd_width()) {
			if (req_simd_width == 0u) {
				req_simd_width = vk_ms_entry->info->required_simd_width;
			} else {
				if (req_simd_width != vk_ms_entry->info->required_simd_width) {
					// this is an unrecoverable error, so throw
					throw std::runtime_error("required task and mesh SIMD widths must match: " +
											 std::to_string(req_simd_width) + " != " + std::to_string(vk_ms_entry->info->required_simd_width));
				}
			}
		}
		if (req_simd_width != 0 && (req_simd_width < dev.simd_range.x || req_simd_width > dev.simd_range.y)) {
			// once more an unrecoverable error
			throw std::runtime_error("required task/mesh SIMD width " + std::to_string(req_simd_width) + " is not supported by the device");
		}
		
		uint32_t simd_width = simd_width_;
		if (req_simd_width != 0) {
			if (simd_width > 0 && simd_width != req_simd_width) {
				log_error("unsupported SIMD width: $ is required, but $ was requested", req_simd_width, simd_width);
			}
			simd_width = req_simd_width;
		} else {
			if (simd_width != 0 && (simd_width < dev.simd_range.x || simd_width > dev.simd_range.y)) {
				log_error("unsupported SIMD width: $ when devices supports [$, $]",
						  simd_width, dev.simd_range.x, dev.simd_range.y);
				simd_width = math::max(dev.simd_width, dev.simd_range.x);
			} else {
				if (simd_width == 0) {
					simd_width = math::max(dev.simd_width, dev.simd_range.x);
				}
				// else: specified SIMD width is supported
			}
		}
		assert(simd_width != 0);
		
		// update key
		switch (simd_width) {
			case 16: key.simd = 1u; break;
			case 32: key.simd = 2u; break;
			case 64: key.simd = 3u; break;
			case 128: key.simd = 4u; break;
			default:
				throw std::runtime_error("unsupported SIMD width " + std::to_string(simd_width));
		}
		
		// more local size validation and handling based on SIMD width
		if (vk_ts_entry) {
			if (task_wg_size.is_null()) {
				task_wg_size = { simd_width, 1u, 1u };
			} else {
				task_wg_size.max(1u);
			}
		}
		if (mesh_wg_size.is_null()) {
			mesh_wg_size = { simd_width, 1u, 1u };
		} else {
			mesh_wg_size.max(1u);
		}
		
		if (vk_ts_entry && (((task_wg_size % simd_width) != 0u).any() && ~(task_wg_size == 1u))) {
			throw std::runtime_error("task shader local sizes " + task_wg_size.to_string() +
									 " must be a multiple of SIMD width " + std::to_string(simd_width));
		}
		if (((mesh_wg_size % simd_width) != 0u).any() && ~(mesh_wg_size == 1u)) {
			throw std::runtime_error("mesh shader local sizes " + mesh_wg_size.to_string() +
									 " must be a multiple of SIMD width " + std::to_string(simd_width));
		}
		
		if (vk_ts_entry && (task_wg_size.extent() % simd_width) != 0u) {
			throw std::runtime_error("task shader total local size " + std::to_string(task_wg_size.extent()) +
									 " must be a multiple of SIMD width " + std::to_string(simd_width));
		}
		if ((mesh_wg_size.extent() % simd_width) != 0u) {
			throw std::runtime_error("mesh shader total local size " + std::to_string(mesh_wg_size.extent()) +
									 " must be a multiple of SIMD width " + std::to_string(simd_width));
		}
		
		if ((task_wg_size > 1024u).any()) {
			throw std::runtime_error("unsupported task shader local size " + task_wg_size.to_string());
		}
		if ((mesh_wg_size > 1024u).any()) {
			throw std::runtime_error("unsupported mesh shader local size " + mesh_wg_size.to_string());
		}
		
		// finally: set task/mesh local sizes
		static const auto encode_wg_value = [](const uint32_t size) {
			if (size <= 1u) {
				return 0u;
			}
			assert((size % 16u) == 0u);
			const auto enc_size = size / 16u;
			assert(enc_size <= 64u);
			return enc_size;
		};
		if (vk_ts_entry) {
			key.task_wg_x = encode_wg_value(task_wg_size.x);
			key.task_wg_y = encode_wg_value(task_wg_size.y);
			key.task_wg_z = encode_wg_value(task_wg_size.z);
		}
		key.mesh_wg_x = encode_wg_value(mesh_wg_size.x);
		key.mesh_wg_y = encode_wg_value(mesh_wg_size.y);
		key.mesh_wg_z = encode_wg_value(mesh_wg_size.z);
	}
	
	return key;
}

vulkan_pipeline_state_t vulkan_pipeline::create_pipeline_state(const vulkan_internal_pipeline_state_t& internal_state) const {
	return {
		.pipeline = internal_state.pipeline,
		.layout = internal_state.layout,
		.vs_entry = vk_vs_entry,
		.fs_entry = vk_fs_entry,
		.ts_entry = vk_ts_entry,
		.ms_entry = vk_ms_entry,
	};
}

const vulkan_pass* vulkan_pipeline::get_vulkan_pass(const bool get_multi_view) const {
	if (!get_multi_view) {
		return (sv_vulkan_base_pass ? sv_vulkan_base_pass.get() : nullptr);
	} else {
		return (mv_vulkan_base_pass ? mv_vulkan_base_pass.get() : nullptr);
	}
}

} // namespace fl

#endif
