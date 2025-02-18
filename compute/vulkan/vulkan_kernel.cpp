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

#include <floor/compute/vulkan/vulkan_kernel.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/compute_context.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_args.hpp>
#include <floor/compute/vulkan/vulkan_encoder.hpp>
#include <floor/compute/vulkan/vulkan_argument_buffer.hpp>
#include <floor/compute/vulkan/vulkan_fence.hpp>
#include <floor/compute/vulkan/vulkan_disassembly.hpp>
#include <floor/floor/floor.hpp>
#include <floor/compute/soft_printf.hpp>

using namespace llvm_toolchain;

uint64_t vulkan_kernel::vulkan_kernel_entry::make_spec_key(const uint3& work_group_size) {
#if defined(FLOOR_DEBUG)
	if((work_group_size.yz >= 65536u).any()) {
		log_error("work-group size is too big: $", work_group_size);
		return 0;
	}
#endif
	return (uint64_t(work_group_size.x) << 32ull |
			uint64_t(work_group_size.y) << 16ull |
			uint64_t(work_group_size.z));
}

bool vulkan_kernel::should_log_vulkan_binary(const string& function_name) {
	static const auto log_binaries = floor::get_toolchain_log_binaries();
	if (!log_binaries) {
		return false;
	}
	static const auto& log_binary_filter = floor::get_vulkan_log_binary_filter();
	if (log_binary_filter.empty()) {
		return true;
	}
	return (find_if(log_binary_filter.begin(), log_binary_filter.end(), [&function_name](const string& filter_name) {
		return (function_name.find(filter_name) != string::npos);
	}) != log_binary_filter.end());
}

vulkan_kernel::vulkan_kernel_entry::spec_entry* vulkan_kernel::vulkan_kernel_entry::specialize(const vulkan_device& device,
																							   const uint3& work_group_size) {
	if (info->has_valid_required_local_size() && (info->required_local_size != work_group_size).any()) {
		log_error("kernel $ has fixed compiled required local size of $, it may not be specialized to a different local size of $",
				  info->name, info->required_local_size, work_group_size);
		return nullptr;
	}
	
	const auto spec_key = vulkan_kernel_entry::make_spec_key(work_group_size);
	const auto iter = specializations.find(spec_key);
	if(iter != specializations.end()) {
		// already built this
		return &iter->second;
	}
	
	// work-group size specialization
	static constexpr const uint32_t spec_entry_count = 3;
	
	vulkan_kernel::vulkan_kernel_entry::spec_entry spec_entry;
	spec_entry.data.resize(spec_entry_count);
	spec_entry.data[0] = work_group_size.x;
	spec_entry.data[1] = work_group_size.y;
	spec_entry.data[2] = work_group_size.z;
	
	spec_entry.map_entries = {
		{ .constantID = 1, .offset = sizeof(uint32_t) * 0, .size = sizeof(uint32_t) },
		{ .constantID = 2, .offset = sizeof(uint32_t) * 1, .size = sizeof(uint32_t) },
		{ .constantID = 3, .offset = sizeof(uint32_t) * 2, .size = sizeof(uint32_t) },
	};
	
	spec_entry.info = VkSpecializationInfo {
		.mapEntryCount = uint32_t(spec_entry.map_entries.size()),
		.pMapEntries = spec_entry.map_entries.data(),
		.dataSize = spec_entry.data.size() * sizeof(decltype(spec_entry.data)::value_type),
		.pData = spec_entry.data.data(),
	};
	stage_info.pSpecializationInfo = &spec_entry.info;
	stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage_info.pNext = &stage_sub_group_info;
	
	VkPipelineCreateFlags pipeline_flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	
	// if binaries should be logged/dumped, create a pipeline cache from which we'll extract the binary
	VkPipelineCache cache { nullptr };
	const bool log_binary = should_log_vulkan_binary(info->name);
	if (log_binary) {
		const VkPipelineCacheCreateInfo cache_create_info {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
			.initialDataSize = 0,
			.pInitialData = nullptr,
		};
		vkCreatePipelineCache(device.device, &cache_create_info, nullptr, &cache);
		
		pipeline_flags |= VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR;
		pipeline_flags |= VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
	}

	// create the compute pipeline for this kernel + device + work-group size
	const VkComputePipelineCreateInfo pipeline_info {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = pipeline_flags,
		.stage = stage_info,
		.layout = pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0,
	};
	log_debug("specializing $ for $ ...", info->name, work_group_size);
	VK_CALL_RET(vkCreateComputePipelines(device.device, cache, 1, &pipeline_info, nullptr, &spec_entry.pipeline),
				"failed to create compute pipeline (" + info->name + ", " + work_group_size.to_string() + ")",
				nullptr)
	((const vulkan_compute*)device.context)->set_vulkan_debug_label(device, VK_OBJECT_TYPE_PIPELINE, uint64_t(spec_entry.pipeline),
																	"pipeline:" + info->name + ":spec:" + work_group_size.to_string());
	
	if (cache && log_binary) {
		vulkan_disassembly::disassemble(device, info->name + "_" + std::to_string(work_group_size.x) + "_" + std::to_string(work_group_size.y) +
										"_" + std::to_string(work_group_size.z), spec_entry.pipeline, &cache);
		vkDestroyPipelineCache(device.device, cache, nullptr);
	}
	
	if (const auto spec_iter = specializations.emplace(spec_key, spec_entry); spec_iter.second) {
		return &spec_iter.first->second;
	}
	return nullptr;
}

vulkan_kernel::vulkan_kernel(const string_view kernel_name_, kernel_map_type&& kernels_) :
compute_kernel(kernel_name_), kernels(std::move(kernels_)) {
}

typename vulkan_kernel::kernel_map_type::iterator vulkan_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const vulkan_device*)&cqueue.get_device());
}

shared_ptr<vulkan_encoder> vulkan_kernel::create_encoder(const compute_queue& cqueue,
														 const vulkan_command_buffer* cmd_buffer_,
														 const VkPipeline pipeline,
														 const VkPipelineLayout pipeline_layout,
														 const vector<const vulkan_kernel_entry*>& entries,
														 const char* debug_label floor_unused_if_release,
														 bool& success) const {
	success = false;
	if (entries.empty()) return {};
	
	// create a command buffer if none was specified
	vulkan_command_buffer cmd_buffer;
	if (cmd_buffer_ == nullptr) {
		cmd_buffer = ((const vulkan_queue&)cqueue).make_command_buffer("encoder");
		if(cmd_buffer.cmd_buffer == nullptr) return {}; // just abort
		
		// begin recording
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					"failed to begin command buffer", {})
	} else {
		cmd_buffer = *(const vulkan_command_buffer*)cmd_buffer_;
	}
	
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	
	auto encoder = make_shared<vulkan_encoder>(vulkan_encoder {
		.cmd_buffer = cmd_buffer,
		.cqueue = (const vulkan_queue&)cqueue,
		.device = vk_dev,
		.pipeline = pipeline,
		.pipeline_layout = pipeline_layout,
		.entries = entries,
	});
	
#if defined(FLOOR_DEBUG)
	encoder->debug_label = "encoder_"s + (entries[0]->stage_info.stage == VK_SHADER_STAGE_COMPUTE_BIT ? "compute" : "graphics");
	if (entries[0]->info) {
		encoder->debug_label += "_" + entries[0]->info->name;
	}
	if (debug_label) {
		encoder->debug_label += '#';
		encoder->debug_label += debug_label;
	}
	((const vulkan_compute*)vk_dev.context)->vulkan_begin_cmd_debug_label(cmd_buffer.cmd_buffer, encoder->debug_label.c_str());
#endif
	
	vkCmdBindPipeline(cmd_buffer.cmd_buffer,
					  entries[0]->stage_info.stage == VK_SHADER_STAGE_COMPUTE_BIT ?
					  VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
					  pipeline);
	
	success = true;
	return encoder;
}

VkPipeline vulkan_kernel::get_pipeline_spec(const vulkan_device& device,
											vulkan_kernel_entry& entry,
											const uint3& work_group_size) const {
	GUARD(entry.specializations_lock);
	
	// try to find a pipeline that has already been built/specialized for this work-group size
	const auto spec_key = vulkan_kernel_entry::make_spec_key(work_group_size);
	const auto iter = entry.specializations.find(spec_key);
	if(iter != entry.specializations.end()) {
		return iter->second.pipeline;
	}
	
	// not built/specialized yet, do so now
	const auto spec_entry = entry.specialize(device, work_group_size);
	if(spec_entry == nullptr) {
		log_error("run-time specialization of kernel $ with work-group size $ failed",
				  entry.info->name, work_group_size);
		return entry.specializations.begin()->second.pipeline;
	}
	return spec_entry->pipeline;
}

void vulkan_kernel::execute(const compute_queue& cqueue,
							vulkan_command_buffer* external_cmd_buffer,
							const bool& is_cooperative,
							const bool& wait_until_completion,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size,
							const uint3& local_work_size_,
							const vector<compute_kernel_arg>& args,
							const vector<const compute_fence*>& wait_fences_,
							const vector<compute_fence*>& signal_fences_,
							const char* debug_label,
							kernel_completion_handler_f&& completion_handler) const {
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Vulkan");
		return;
	}
	
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
	if (kernel_iter == kernels.cend()) {
		log_error("no kernel \"$\" for this compute queue/device exists!", kernel_name);
		return;
	}
	
	const auto& vk_dev = *kernel_iter->first;
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	
	// check work size
	const uint3 block_dim = check_local_work_size(kernel_iter->second, local_work_size_);
	if (kernel_iter->second.info->has_valid_required_local_size() &&
		(kernel_iter->second.info->required_local_size != local_work_size_.maxed(1u)).any()) {
		log_error("kernel $ has fixed compiled required local size of $, it may not be executed with a different local size of $",
				  kernel_iter->second.info->name, kernel_iter->second.info->required_local_size, local_work_size_);
		return;
	}
	
	const uint3 grid_dim_overflow {
		global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	
	// create command buffer ("encoder") for this kernel execution
	const vector<const vulkan_kernel_entry*> shader_entries {
		&kernel_iter->second
	};
	bool encoder_success = false;
	auto encoder = create_encoder(cqueue, external_cmd_buffer,
								  get_pipeline_spec(vk_dev, kernel_iter->second, block_dim),
								  kernel_iter->second.pipeline_layout,
								  shader_entries, debug_label, encoder_success);
	if (!encoder_success) {
		log_error("failed to create vulkan encoder / command buffer for kernel \"$\"", kernel_iter->second.info->name);
		return;
	}
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;
	
	// create + init printf buffer if this function uses soft-printf
	shared_ptr<compute_buffer> printf_buffer;
	const auto is_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags);
	if (is_soft_printf) {
		printf_buffer = allocate_printf_buffer(cqueue);
		initialize_printf_buffer(cqueue, *printf_buffer);
		implicit_args.emplace_back(printf_buffer);
	}
	
	// acquire kernel descriptor sets/buffers and constant buffer
	const auto& entry = kernel_iter->second;
	if (entry.desc_buffer.desc_buffer_container) {
		encoder->acquired_descriptor_buffers.emplace_back(entry.desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
		if (entry.constant_buffers) {
			encoder->acquired_constant_buffers.emplace_back(entry.constant_buffers->acquire());
			encoder->constant_buffer_mappings.emplace_back(entry.constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
			encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {
				&entry.constant_buffer_info,
				encoder->acquired_constant_buffers.back().first,
				{ (uint8_t*)encoder->constant_buffer_mappings.back(), encoder->acquired_constant_buffers.back().first->get_size() }
			});
		} else {
			encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {});
		}
	}
	
	// set and handle arguments
	vulkan_args::transition_info_t transition_info {};
	if (!set_and_handle_arguments(false, *encoder, shader_entries, args, implicit_args, transition_info)) {
		return;
	}
	
	// for kernel executions, we can immediately create a pipeline barrier for all image transitions in the same cmd buffer
	if (!transition_info.barriers.empty()) {
		const VkDependencyInfo dep_info {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers = nullptr,
			.imageMemoryBarrierCount = uint32_t(transition_info.barriers.size()),
			.pImageMemoryBarriers = &transition_info.barriers[0],
		};
		if (cqueue.get_queue_type() == compute_queue::QUEUE_TYPE::COMPUTE) {
			// if this is a compute-only queue, we need to rewrite all VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT stage masks
			for (auto& bar : transition_info.barriers) {
				if ((bar.srcStageMask & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) != 0) {
					bar.srcStageMask = ((bar.srcStageMask & ~VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) |
										VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
										VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
				}
				if ((bar.dstStageMask & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) != 0) {
					bar.dstStageMask = ((bar.dstStageMask & ~VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) |
										VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
										VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
				}
			}
		}
		vkCmdPipelineBarrier2(encoder->cmd_buffer.cmd_buffer, &dep_info);
	}
	
	// set/write/update descriptors
	const VkBindDescriptorBufferEmbeddedSamplersInfoEXT bind_embedded_info {
		.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
		.pNext = nullptr,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.layout = entry.pipeline_layout,
		.set = 0 /* always set #0 */,
	};
	vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(encoder->cmd_buffer.cmd_buffer, &bind_embedded_info);
	
	if (!encoder->acquired_descriptor_buffers.empty() || !encoder->argument_buffers.empty()) {
		// setup + bind descriptor buffers
		const auto desc_buf_count = uint32_t(encoder->acquired_descriptor_buffers.size()) + uint32_t(encoder->argument_buffers.size());
		vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
		desc_buf_bindings.resize(desc_buf_count);
		vector<VkBufferUsageFlags2CreateInfo> desc_buf_bindings_usage;
		desc_buf_bindings_usage.resize(desc_buf_count);
		size_t desc_buf_binding_idx = 0;
		
		if (!encoder->acquired_descriptor_buffers.empty()) {
			assert(encoder->acquired_descriptor_buffers.size() == 1);
			const auto& kernel_desc_buffer = *(const vulkan_buffer*)encoder->acquired_descriptor_buffers[0].desc_buffer;
			desc_buf_bindings_usage[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
				.pNext = nullptr,
				.usage = kernel_desc_buffer.get_vulkan_buffer_usage(),
			};
			desc_buf_bindings[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
				.address = kernel_desc_buffer.get_vulkan_buffer_device_address(),
				.usage = VK_LEGACY_USAGE_FLAGS_WORKAROUND(kernel_desc_buffer.get_vulkan_buffer_usage()),
			};
			++desc_buf_binding_idx;
		}
		
		for (const auto& arg_buffer : encoder->argument_buffers) {
			assert(arg_buffer.first == 0);
			desc_buf_bindings_usage[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
				.pNext = nullptr,
				.usage = arg_buffer.second->get_vulkan_buffer_usage(),
			};
			desc_buf_bindings[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
				.address = arg_buffer.second->get_vulkan_buffer_device_address(),
				.usage = VK_LEGACY_USAGE_FLAGS_WORKAROUND(arg_buffer.second->get_vulkan_buffer_usage()),
			};
			++desc_buf_binding_idx;
		}
		
		vkCmdBindDescriptorBuffersEXT(encoder->cmd_buffer.cmd_buffer, desc_buf_count, desc_buf_bindings.data());
		
		// kernel descriptor set + any argument buffers are stored in contiguous descriptor set indices
		const vector<VkDeviceSize> offsets(desc_buf_count, 0); // always 0 for all
		vector<uint32_t> buffer_indices(desc_buf_count, 0);
		for (uint32_t i = 1; i < desc_buf_count; ++i) {
			buffer_indices[i] = i;
		}
		const auto start_set = (encoder->acquired_descriptor_buffers.empty() ? 2u /* first arg buffer set */ : 1u /* kernel set */);
		const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
			.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
			.pNext = nullptr,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.layout = entry.pipeline_layout,
			.firstSet = start_set,
			.setCount = desc_buf_count,
			.pBufferIndices = buffer_indices.data(),
			.pOffsets = offsets.data(),
		};
		vkCmdSetDescriptorBufferOffsets2EXT(encoder->cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
	}
	
	// set dims + pipeline
	// TODO: check if grid_dim matches compute shader definition
	vkCmdDispatch(encoder->cmd_buffer.cmd_buffer, grid_dim.x, grid_dim.y, grid_dim.z);
	
	// all done here, end + submit
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_dev.context)->vulkan_end_cmd_debug_label(encoder->cmd_buffer.cmd_buffer);
#endif
	VK_CALL_RET(vkEndCommandBuffer(encoder->cmd_buffer.cmd_buffer), "failed to end command buffer")
	// add completion handler if required
	if (completion_handler) {
		vk_queue.add_completion_handler(encoder->cmd_buffer, [handler = std::move(completion_handler)]() {
			handler();
		});
	}
	
	vector<vulkan_queue::wait_fence_t> wait_fences;
	vector<vulkan_queue::signal_fence_t> signal_fences;
	for (const auto& fence : wait_fences_) {
		if (!fence) {
			continue;
		}
		const auto& vk_fence = (const vulkan_fence&)*fence;
		wait_fences.emplace_back(vulkan_queue::wait_fence_t {
			.fence = fence,
			.signaled_value = vk_fence.get_signaled_value(),
			.stage = compute_fence::SYNC_STAGE::NONE,
		});
	}
	for (auto& fence : signal_fences_) {
		if (!fence) {
			continue;
		}
		auto& vk_fence = (vulkan_fence&)*fence;
		if (!vk_fence.next_signal_value()) {
			throw runtime_error("failed to set next signal value on fence");
		}
		signal_fences.emplace_back(vulkan_queue::signal_fence_t {
			.fence = fence,
			.unsignaled_value = vk_fence.get_unsignaled_value(),
			.signaled_value = vk_fence.get_signaled_value(),
			.stage = compute_fence::SYNC_STAGE::NONE,
		});
	}
	
	vk_queue.submit_command_buffer(std::move(encoder->cmd_buffer),
								   std::move(wait_fences), std::move(signal_fences),
								   [encoder](const vulkan_command_buffer&) {
		// -> completion handler
		
		// kill constant buffers after the kernel has finished execution
		encoder->constant_buffers.clear();
	}, wait_until_completion || is_soft_printf /* must block when soft-print is used */);
	
	// release all acquired descriptor sets/buffers and constant buffers again
	for (auto& desc_buf_instance : encoder->acquired_descriptor_buffers) {
		entry.desc_buffer.desc_buffer_container->release_descriptor_buffer(desc_buf_instance);
	}
	encoder->acquired_descriptor_buffers.clear();
	for (auto& acq_constant_buffer : encoder->acquired_constant_buffers) {
		entry.constant_buffers->release(acq_constant_buffer);
	}
	
	// if soft-printf is being used, read-back results
	if (is_soft_printf) {
		auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(cqueue, cpu_printf_buffer.get());
		handle_printf_buffer(span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
	}
}

bool vulkan_kernel::set_and_handle_arguments(const bool is_shader, vulkan_encoder& encoder,
											 const vector<const vulkan_kernel_entry*>& shader_entries,
											 const vector<compute_kernel_arg>& args,
											 const vector<compute_kernel_arg>& implicit_args,
											 vulkan_args::transition_info_t& transition_info) const {
	if (encoder.acquired_descriptor_buffers.empty()) {
		return true;
	}
	
	// this will directly write into the host-visible memory of the acquired descriptor buffers
	// -> use a span for each buffer/set so that we can do a bounds check (in debug mode)
	vector<span<uint8_t>> host_desc_data;
	{
		host_desc_data.reserve(shader_entries.size());
		size_t acq_desc_buffer_idx = 0;
		for (const auto& entry : shader_entries) {
			if (!entry || !entry->desc_buffer.desc_buffer_container) {
				host_desc_data.emplace_back(span<uint8_t> {});
			} else {
				assert(acq_desc_buffer_idx < encoder.acquired_descriptor_buffers.size());
				host_desc_data.emplace_back(encoder.acquired_descriptor_buffers[acq_desc_buffer_idx].mapped_host_memory);
				++acq_desc_buffer_idx;
			}
		}
	}
	
	vector<const vector<VkDeviceSize>*> argument_offsets;
	vector<const function_info*> entries;
	for (const auto& entry : shader_entries) {
		argument_offsets.emplace_back(entry ? &entry->desc_buffer.argument_offsets : nullptr);
		entries.emplace_back(entry ? entry->info : nullptr);
	}
	vector<const vulkan_args::constant_buffer_wrapper_t*> const_buffers;
	for (const auto& constant_buffer_wrapper : encoder.constant_buffer_wrappers) {
		const_buffers.emplace_back(constant_buffer_wrapper.constant_buffer_storage ? &constant_buffer_wrapper : nullptr);
	}
	bool args_success = false;
	if (is_shader) {
		std::tie(args_success, encoder.argument_buffers) = vulkan_args::set_arguments<vulkan_args::ENCODER_TYPE::SHADER>(encoder.device,
																														 host_desc_data,
																														 entries,
																														 argument_offsets,
																														 const_buffers,
																														 args, implicit_args,
																														 &transition_info);
	} else {
		std::tie(args_success, encoder.argument_buffers) = vulkan_args::set_arguments<vulkan_args::ENCODER_TYPE::COMPUTE>(encoder.device,
																														  host_desc_data,
																														  entries,
																														  argument_offsets,
																														  const_buffers,
																														  args, implicit_args,
																														  &transition_info);
	}
	return args_success;
}

const compute_kernel::kernel_entry* vulkan_kernel::get_kernel_entry(const compute_device& dev) const {
	if (const auto iter = kernels.find((const vulkan_device*)&dev); iter != kernels.end()) {
		return &iter->second;
	}
	return nullptr;
}

unique_ptr<argument_buffer> vulkan_kernel::create_argument_buffer_internal(const compute_queue& cqueue,
																		   const kernel_entry& kern_entry,
																		   const llvm_toolchain::arg_info& arg floor_unused,
																		   const uint32_t& user_arg_index,
																		   const uint32_t& ll_arg_index,
																		   const COMPUTE_MEMORY_FLAG& add_mem_flags) const {
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	const auto& vk_ctx = (const vulkan_compute&)*vk_dev.context;
	const auto& vulkan_entry = (const vulkan_kernel_entry&)kern_entry;
	
	if (!has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_VULKAN_DESCRIPTOR_BUFFER>(vulkan_entry.info->flags)) {
		log_error("can't use a function that has not been built with descriptor buffer support with descriptor/argument buffers: in function $",
				  vulkan_entry.info->name);
		return {};
	}
	
	// check if info exists
	const auto& arg_info = vulkan_entry.info->args[ll_arg_index].argument_buffer_info;
	if (!arg_info) {
		log_error("no argument buffer info for arg at index #$", user_arg_index);
		return {};
	}
	
	const auto arg_buf_name = vulkan_entry.info->name + ".arg_buffer@" + to_string(user_arg_index);
	
	// argument buffers are implemented as individual descriptor sets inside the parent function/program, stored in descriptor buffers
	// -> grab the descriptor set layout from the parent function that has already created this + allocate a descriptor buffer
	if (vulkan_entry.argument_buffers.empty()) {
		log_error("no argument buffer info in function entry (in $)", vulkan_entry.info->name);
		return {};
	}
	
	// find the argument buffer info index
	uint32_t arg_buf_idx = 0;
	for (uint32_t i = 0, count = uint32_t(vulkan_entry.info->args.size()); i < min(ll_arg_index, count); ++i) {
		if (i == ll_arg_index) {
			break;
		}
		if (vulkan_entry.info->args[i].special_type == SPECIAL_TYPE::ARGUMENT_BUFFER) {
			++arg_buf_idx;
		}
	}
	if (arg_buf_idx >= vulkan_entry.argument_buffers.size()) {
		log_error("argument buffer info index is out-of-bounds for function entry (arg #$ in $)",
				  user_arg_index, vulkan_entry.info->name);
		return {};
	}
	const auto& arg_buf_info = vulkan_entry.argument_buffers[arg_buf_idx];
	
	// the argument buffer size is device/driver dependent (can't be statically known)
	// -> query required buffer size + ensure good alignment
	const auto arg_buffer_size = const_math::round_next_multiple(arg_buf_info.layout.layout_size, uint64_t(256));
	
	// query offset for each binding/argument
	vector<VkDeviceSize> argument_offsets;
	argument_offsets.reserve(arg_buf_info.layout.bindings.size());
	for (const auto& binding : arg_buf_info.layout.bindings) {
		VkDeviceSize offset = 0;
		vkGetDescriptorSetLayoutBindingOffsetEXT(vk_dev.device, arg_buf_info.layout.desc_set_layout, binding.binding, &offset);
		argument_offsets.emplace_back(offset);
	}
	
	// alloc with Vulkan flags: need host-visible/host-coherent and descriptor buffer usage flags
	auto arg_buffer_storage = vk_ctx.create_buffer(cqueue, arg_buffer_size,
												   // NOTE: read-only on the device side (until writable argument buffers are implemented)
												   COMPUTE_MEMORY_FLAG::READ |
												   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE |
												   COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT |
												   COMPUTE_MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER |
												   add_mem_flags);
	arg_buffer_storage->set_debug_label(arg_buf_name);
	auto mapped_host_ptr = arg_buffer_storage->map(cqueue, (COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE |
															COMPUTE_MEMORY_MAP_FLAG::BLOCK));
	span<uint8_t> mapped_host_memory { (uint8_t*)mapped_host_ptr, arg_buffer_size };
	
	// if this needs a constant buffer, also allocate it
	// TODO: allocate together with arg buf storage + offset/align?
	shared_ptr<compute_buffer> constant_buffer_storage;
	span<uint8_t> constant_buffer_mapping;
	if (arg_buf_info.layout.constant_buffer_size > 0) {
		// align size to 16 bytes
		const uint32_t alignment_pot = 4u, alignment = 1u << alignment_pot;
		const auto constant_buffer_size = ((arg_buf_info.layout.constant_buffer_size + alignment - 1u) / alignment) * alignment;
		const auto& ctx = *(const vulkan_compute*)vk_dev.context;
		
		// allocate in device-local/host-coherent memory
		constant_buffer_storage = ctx.create_buffer(cqueue, constant_buffer_size,
													COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE |
													COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT);
		constant_buffer_storage->set_debug_label(arg_buf_name + ":const_buf");
		
		auto constant_buffer_host_ptr = constant_buffer_storage->map(cqueue,
																	 COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE |
																	 COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		if (constant_buffer_host_ptr == nullptr) {
			log_error("failed to memory map constant buffer for argument buffer @$ in function $", user_arg_index, vulkan_entry.info->name);
			return {};
		}
		constant_buffer_mapping = { (uint8_t*)constant_buffer_host_ptr, constant_buffer_size };
	}
	
	// create the argument buffer
	return make_unique<vulkan_argument_buffer>(*this, arg_buffer_storage, *arg_info, arg_buf_info.layout, std::move(argument_offsets),
											   mapped_host_memory, constant_buffer_storage, constant_buffer_mapping);
}

#endif
