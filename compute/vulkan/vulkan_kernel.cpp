/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <floor/compute/vulkan/vulkan_encoder.hpp>
#include <floor/compute/vulkan/vulkan_argument_buffer.hpp>
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

vulkan_kernel::vulkan_kernel_entry::spec_entry* vulkan_kernel::vulkan_kernel_entry::specialize(const vulkan_device& device,
																							   const uint3& work_group_size) {
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

	// create the compute pipeline for this kernel + device + work-group size
	const VkComputePipelineCreateInfo pipeline_info {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = (device.descriptor_buffer_support ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT : 0),
		.stage = stage_info,
		.layout = pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0,
	};
	log_debug("specializing $ for $ ...", info->name, work_group_size); logger::flush();
	VK_CALL_RET(vkCreateComputePipelines(device.device, nullptr, 1, &pipeline_info, nullptr,
										 &spec_entry.pipeline),
				"failed to create compute pipeline (" + info->name + ", " + work_group_size.to_string() + ")",
				nullptr)
	((const vulkan_compute*)device.context)->set_vulkan_debug_label(device, VK_OBJECT_TYPE_PIPELINE, uint64_t(spec_entry.pipeline),
																	"pipeline:" + info->name + ":spec:" + work_group_size.to_string());
	
	auto spec_iter = specializations.insert(spec_key, spec_entry);
	if(!spec_iter.first) return nullptr;
	return &spec_iter.second->second;
}

vulkan_kernel::vulkan_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

typename vulkan_kernel::kernel_map_type::iterator vulkan_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const vulkan_device&)cqueue.get_device());
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
	
#if defined(FLOOR_DEBUG)
	string encoder_label = "encoder_"s + (entries[0]->stage_info.stage == VK_SHADER_STAGE_COMPUTE_BIT ? "compute" : "graphics");
	if (entries[0]->info) {
		encoder_label += "_" + entries[0]->info->name;
	}
	if (debug_label) {
		encoder_label += '#';
		encoder_label += debug_label;
	}
	((const vulkan_compute*)vk_dev.context)->vulkan_begin_cmd_debug_label(cmd_buffer.cmd_buffer, encoder_label);
#endif
	
	vkCmdBindPipeline(cmd_buffer.cmd_buffer,
					  entries[0]->stage_info.stage == VK_SHADER_STAGE_COMPUTE_BIT ?
					  VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
					  pipeline);
	
	auto encoder = make_shared<vulkan_encoder>(vulkan_encoder {
		.cmd_buffer = cmd_buffer,
		.cqueue = (const vulkan_queue&)cqueue,
		.device = vk_dev,
		.pipeline = pipeline,
		.pipeline_layout = pipeline_layout,
		.entries = entries,
	});
	
	if (!vk_dev.descriptor_buffer_support) {
		// allocate #args write descriptor sets + allocate #IUBs additional IUB write descriptor sets
		// NOTE: any stage_input arguments have to be ignored
		size_t arg_count = 0, iub_count = 0;
		for (const auto& entry : entries) {
			if (entry == nullptr) continue;
			for (const auto& arg : entry->info->args) {
				if (arg.special_type != SPECIAL_TYPE::STAGE_INPUT) {
					++arg_count;
					
					// +1 for read/write images
					if (arg.image_type != ARG_IMAGE_TYPE::NONE &&
						arg.image_access == ARG_IMAGE_ACCESS::READ_WRITE) {
						++arg_count;
					}
					
					// handle IUBs
					if (arg.special_type == SPECIAL_TYPE::IUB) {
						++iub_count;
					}
				}
			}
			
			// implicit printf buffer
			if (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
				++arg_count;
			}
		}
		encoder->legacy.write_descs.resize(arg_count);
		if (iub_count > 0) {
			encoder->legacy.iub_descs.resize(iub_count);
		}
	}
	
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
							const bool& is_cooperative,
							const bool& wait_until_completion,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size,
							const uint3& local_work_size_,
							const vector<compute_kernel_arg>& args,
							const vector<const compute_fence*>& wait_fences floor_unused,
							const vector<const compute_fence*>& signal_fences floor_unused,
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
		log_error("no kernel for this compute queue/device exists!");
		return;
	}
	
	const auto& vk_dev = kernel_iter->first.get();
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	const auto& vk_ctx = *(vulkan_compute*)vk_dev.context;
	
	// check work size
	const uint3 block_dim = check_local_work_size(kernel_iter->second, local_work_size_);
	
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
	auto encoder = create_encoder(cqueue, nullptr,
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
	if (vk_dev.descriptor_buffer_support) {
		if (entry.desc_buffer.desc_buffer_container) {
			encoder->acquired_descriptor_buffers.emplace_back(entry.desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			if (entry.constant_buffers) {
				encoder->acquired_constant_buffers.emplace_back(entry.constant_buffers->acquire());
				encoder->constant_buffer_mappings.emplace_back(entry.constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
			}
		}
	} else {
		if (entry.desc_legacy.desc_set_container) {
			encoder->legacy.acquired_descriptor_sets.emplace_back(entry.desc_legacy.desc_set_container->acquire_descriptor_set());
			if (entry.constant_buffers) {
				encoder->acquired_constant_buffers.emplace_back(entry.constant_buffers->acquire());
				encoder->constant_buffer_mappings.emplace_back(entry.constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
			}
		}
	}
	
	// set and handle arguments
	idx_handler idx;
	if (vk_dev.descriptor_buffer_support) {
		if (!set_and_handle_arguments(*encoder, shader_entries, idx, args, implicit_args)) {
			return;
		}
	} else {
		if (!set_and_handle_arguments_legacy(*encoder, shader_entries, idx, args, implicit_args)) {
			return;
		}
	}
	
	// run
	// TODO: implement waiting for "wait_fences"
	
	// set/write/update descriptors
	if (vk_dev.descriptor_buffer_support) {
		// this always exists
		vk_ctx.vulkan_cmd_bind_descriptor_buffer_embedded_samplers(encoder->cmd_buffer.cmd_buffer,
																   VK_PIPELINE_BIND_POINT_COMPUTE,
																   entry.pipeline_layout, 0 /* always set #0 */);
		
		if (!encoder->acquired_descriptor_buffers.empty() || !encoder->argument_buffers.empty()) {
			// setup + bind descriptor buffers
			const auto desc_buf_count = uint32_t(encoder->acquired_descriptor_buffers.size()) + uint32_t(encoder->argument_buffers.size());
			vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
			desc_buf_bindings.reserve(desc_buf_count);
			
			if (!encoder->acquired_descriptor_buffers.empty()) {
				assert(encoder->acquired_descriptor_buffers.size() == 1);
				const auto& kernel_desc_buffer = *(const vulkan_buffer*)encoder->acquired_descriptor_buffers[0].desc_buffer;
				desc_buf_bindings.emplace_back(VkDescriptorBufferBindingInfoEXT {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
					.pNext = nullptr,
					.address = kernel_desc_buffer.get_vulkan_buffer_device_address(),
					.usage = kernel_desc_buffer.get_vulkan_buffer_usage(),
				});
			}
			
			for (const auto& arg_buffer : encoder->argument_buffers) {
				assert(arg_buffer.first == 0);
				desc_buf_bindings.emplace_back(VkDescriptorBufferBindingInfoEXT {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
					.pNext = nullptr,
					.address = arg_buffer.second->get_vulkan_buffer_device_address(),
					.usage = arg_buffer.second->get_vulkan_buffer_usage(),
				});
			}
			
			vk_ctx.vulkan_cmd_bind_descriptor_buffers(encoder->cmd_buffer.cmd_buffer,
													  desc_buf_count, desc_buf_bindings.data());
			
			// kernel descriptor set + any argument buffers are stored in contiguous descriptor set indices
			const vector<VkDeviceSize> offsets(desc_buf_count, 0); // always 0 for all
			vector<uint32_t> buffer_indices(desc_buf_count, 0);
			for (uint32_t i = 1; i < desc_buf_count; ++i) {
				buffer_indices[i] = i;
			}
			const auto start_set = (encoder->acquired_descriptor_buffers.empty() ? 2u /* first arg buffer set */ : 1u /* kernel set */);
			vk_ctx.vulkan_cmd_set_descriptor_buffer_offsets(encoder->cmd_buffer.cmd_buffer,
															VK_PIPELINE_BIND_POINT_COMPUTE,
															entry.pipeline_layout,
															start_set, desc_buf_count,
															buffer_indices.data(), offsets.data());
		}
	} else {
		vkUpdateDescriptorSets(vk_dev.device,
							   (uint32_t)encoder->legacy.write_descs.size(),
							   encoder->legacy.write_descs.data(),
							   // never copy (bad for performance)
							   0, nullptr);
		
		// final desc set binding after all parameters have been updated/set
		VkDescriptorSet entry_desc_set { nullptr };
		if (!encoder->legacy.acquired_descriptor_sets.empty()) {
			entry_desc_set = encoder->legacy.acquired_descriptor_sets[0].desc_set;
		}
		const VkDescriptorSet desc_sets[2] {
			vk_dev.legacy_fixed_sampler_desc_set,
			entry_desc_set,
		};
		vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
								VK_PIPELINE_BIND_POINT_COMPUTE,
								entry.pipeline_layout,
								0,
								(entry_desc_set != nullptr ? 2 : 1),
								desc_sets,
								encoder->legacy.dyn_offsets.empty() ? 0 : (uint32_t)encoder->legacy.dyn_offsets.size(),
								encoder->legacy.dyn_offsets.empty() ? nullptr : encoder->legacy.dyn_offsets.data());
	}
	
	// set dims + pipeline
	// TODO: check if grid_dim matches compute shader defintion
	vkCmdDispatch(encoder->cmd_buffer.cmd_buffer, grid_dim.x, grid_dim.y, grid_dim.z);
	
	// all done here, end + submit
	VK_CALL_RET(vkEndCommandBuffer(encoder->cmd_buffer.cmd_buffer), "failed to end command buffer")
	// add completion handler if required
	if (completion_handler) {
		vk_queue.add_completion_handler(encoder->cmd_buffer, [handler = std::move(completion_handler)]() {
			handler();
		});
	}
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_dev.context)->vulkan_end_cmd_debug_label(encoder->cmd_buffer.cmd_buffer);
#endif
	// TODO: implement signaling for "signal_fences"
	(void)wait_until_completion;
	vk_queue.submit_command_buffer(encoder->cmd_buffer,
								   [encoder](const vulkan_command_buffer&) {
		// -> completion handler
		
		// kill constant buffers after the kernel has finished execution
		encoder->constant_buffers.clear();
	}, true /*|| wait_until_completion*/ /* TODO: don't always block, but do block if soft-printf is enabled */);
	
	// release all acquired descriptor sets/buffers and constant buffers again
	if (vk_dev.descriptor_buffer_support) {
		for (auto& desc_buf_instance : encoder->acquired_descriptor_buffers) {
			entry.desc_buffer.desc_buffer_container->release_descriptor_buffer(desc_buf_instance);
		}
		encoder->acquired_descriptor_buffers.clear();
	} else {
		for (auto& desc_set_instance : encoder->legacy.acquired_descriptor_sets) {
			entry.desc_legacy.desc_set_container->release_descriptor_set(desc_set_instance);
		}
		encoder->legacy.acquired_descriptor_sets.clear();
	}
	for (auto& acq_constant_buffer : encoder->acquired_constant_buffers) {
		entry.constant_buffers->release(acq_constant_buffer);
	}
	
	// if soft-printf is being used, read-back results
	if (is_soft_printf) {
		auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(cqueue, cpu_printf_buffer.get());
		handle_printf_buffer(cpu_printf_buffer);
	}
}

//! returns the entry for the current indices and makes sure that stage_input args are ignored
static inline const vulkan_kernel::vulkan_kernel_entry* arg_pre_handler(const vector<const vulkan_kernel::vulkan_kernel_entry*>& entries,
																		vulkan_kernel::idx_handler& idx) {
	// make sure we have a usable entry
	const vulkan_kernel::vulkan_kernel_entry* entry = nullptr;
	for (;;) {
		// get the next non-nullptr entry or use the current one if it's valid
		while (entries[idx.entry] == nullptr) {
			++idx.entry;
#if defined(FLOOR_DEBUG)
			if (idx.entry >= entries.size()) {
				log_error("shader/kernel entry out of bounds");
				return nullptr;
			}
#endif
		}
		entry = entries[idx.entry];
		
		// ignore any stage input args
		while (idx.arg < entry->info->args.size() &&
			   entry->info->args[idx.arg].special_type == SPECIAL_TYPE::STAGE_INPUT) {
			++idx.arg;
		}
		
		// have all args been specified for this entry?
		if (idx.arg >= entry->info->args.size()) {
			// implicit args at the end
			const auto implicit_arg_count = (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags) ? 1u : 0u);
			if (idx.arg < entry->info->args.size() + implicit_arg_count) {
				idx.is_implicit = true;
			} else { // actual end
				// get the next entry
				++idx.entry;
				// reset
				idx.arg = 0;
				idx.binding = 0;
				idx.legacy.iub = 0;
				idx.is_implicit = false;
				idx.implicit = 0;
				continue;
			}
		}
		break;
	}
	return entry;
}

static inline void arg_post_handler(const vulkan_kernel::vulkan_kernel_entry& entry,
									vulkan_kernel::idx_handler& idx) {
	// advance all indices
	if (!idx.is_implicit) {
		if (entry.info->args[idx.arg].special_type == SPECIAL_TYPE::IUB) {
			++idx.legacy.iub;
		}
		if (entry.info->args[idx.arg].image_access == ARG_IMAGE_ACCESS::READ_WRITE) {
			// read/write images are implemented as two args -> inc twice
			++idx.legacy.write_desc;
			++idx.binding;
		}
	} else {
		++idx.implicit;
	}
	++idx.arg;
	// argument buffer doesn't use a binding, it's a separate descriptor set
	if (idx.is_implicit || (!idx.is_implicit && entry.info->args[idx.arg].special_type != SPECIAL_TYPE::ARGUMENT_BUFFER)) {
		++idx.binding;
		++idx.legacy.write_desc;
	}
}

bool vulkan_kernel::set_and_handle_arguments(vulkan_encoder& encoder,
											 const vector<const vulkan_kernel_entry*>& shader_entries,
											 idx_handler& idx,
											 const vector<compute_kernel_arg>& args,
											 const vector<compute_kernel_arg>& implicit_args) const {
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
	
	const size_t arg_count = args.size() + implicit_args.size();
	size_t explicit_idx = 0, implicit_idx = 0;
	for (size_t i = 0; i < arg_count; ++i) {
		auto entry = arg_pre_handler(shader_entries, idx);
		const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
		
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_argument(encoder, *entry, idx, host_desc_data[idx.entry], *buf_ptr);
		} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Vulkan");
		} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Vulkan");
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_argument(encoder, *entry, idx, host_desc_data[idx.entry], *img_ptr);
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			set_argument(encoder, *entry, idx, host_desc_data[idx.entry], **vec_img_ptrs);
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			set_argument(encoder, *entry, idx, host_desc_data[idx.entry], **vec_img_sptrs);
		} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			// argument buffer will be set later (together with fixed descriptor buffers)
			encoder.argument_buffers.emplace_back(idx.entry, (const vulkan_buffer*)(*arg_buf_ptr)->get_storage_buffer());
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_argument(encoder, *entry, idx, host_desc_data[idx.entry], *generic_arg_ptr, arg.size);
		} else {
			log_error("encountered invalid arg");
			return false;
		}
		
		arg_post_handler(*entry, idx);
	}
	return true;
}

void vulkan_kernel::set_argument(vulkan_encoder& encoder,
								 const vulkan_kernel_entry& entry,
								 const idx_handler& idx,
								 const span<uint8_t>& host_desc_data,
								 const void* ptr, const size_t& size) const {
	const auto write_offset = entry.desc_buffer.argument_offsets[idx.binding];
	if (!idx.is_implicit && entry.info->args[idx.arg].special_type == SPECIAL_TYPE::IUB) {
		// -> inline uniform buffer (directly writes into the descriptor buffer memory)
#if defined(FLOOR_DEBUG)
		if (write_offset + size > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, ptr, size);
	} else {
		// -> plain old SSBO
#if defined(FLOOR_DEBUG)
		if (write_offset + vulkan_buffer::max_ssbo_descriptor_size > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor buffer write");
		}
#endif
		
		auto& const_buffer = encoder.acquired_constant_buffers[idx.entry].first;
		void* const_buffer_mapping = encoder.constant_buffer_mappings[idx.entry];
		assert(const_buffer_mapping != nullptr);
		const auto& const_buffer_info = encoder.entries[idx.entry]->constant_buffer_info.at(idx.arg);
		assert(const_buffer_info.size == size);
		memcpy((uint8_t*)const_buffer_mapping + const_buffer_info.offset, ptr, const_buffer_info.size);
		
		const VkDescriptorAddressInfoEXT addr_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
			.pNext = nullptr,
			.address = ((vulkan_buffer*)const_buffer)->get_vulkan_buffer_device_address() + const_buffer_info.offset,
			.range = const_buffer_info.size,
			.format = VK_FORMAT_UNDEFINED,
		};
		const VkDescriptorGetInfoEXT desc_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
			.pNext = nullptr,
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.data = {
				.pStorageBuffer = &addr_info,
			},
		};
		((vulkan_compute*)encoder.device.context)->vulkan_get_descriptor(encoder.device.device, &desc_info,
																		 encoder.device.desc_buffer_sizes.ssbo,
																		 host_desc_data.data() + write_offset);
	}
}

void vulkan_kernel::set_argument(vulkan_encoder& encoder,
								 const vulkan_kernel_entry& entry,
								 const idx_handler& idx,
								 const span<uint8_t>& host_desc_data,
								 const compute_buffer* arg) const {
	const auto vk_buffer = ((const vulkan_buffer*)arg)->get_underlying_vulkan_buffer_safe();
	const span<const uint8_t> desc_data { &vk_buffer->get_vulkan_descriptor_data()[0], encoder.device.desc_buffer_sizes.ssbo };
	const auto write_offset = entry.desc_buffer.argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (write_offset + desc_data.size_bytes() > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor buffer write");
	}
#endif
	memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size_bytes());
}

void vulkan_kernel::set_argument(vulkan_encoder& encoder,
								 const vulkan_kernel_entry& entry,
								 const idx_handler& idx,
								 const span<uint8_t>& host_desc_data,
								 const compute_image* arg) const {
	if (idx.is_implicit) {
		log_error("implicit image argument is not supported yet - should not be here");
		return;
	}
	
	auto vk_img = static_cast<vulkan_image*>(const_cast<compute_image*>(arg))->get_underlying_vulkan_image_safe();
	
	// transition image to appropriate layout
	const auto img_access = entry.info->args[idx.arg].image_access;
	if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		vk_img->transition_write(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
								 // also readable?
								 img_access == ARG_IMAGE_ACCESS::READ_WRITE,
								 // always direct-write, never attachment
								 true,
								 // allow general layout?
								 encoder.allow_generic_layout);
	} else { // READ
		vk_img->transition_read(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
								// allow general layout?
								encoder.allow_generic_layout);
	}
	
	// read image desc/obj
	if (img_access == ARG_IMAGE_ACCESS::READ ||
		img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		const auto& desc_data = vk_img->get_vulkan_descriptor_data_sampled();
		const auto write_offset = entry.desc_buffer.argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
		if (write_offset + desc_data.size_bytes() > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size());
	}
	
	// write image descs/objs
	if (img_access == ARG_IMAGE_ACCESS::WRITE ||
		img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		const auto& desc_data = vk_img->get_vulkan_descriptor_data_storage();
		const uint32_t rw_offset = (img_access == ARG_IMAGE_ACCESS::READ_WRITE ? 1u : 0u);
		const auto write_offset = entry.desc_buffer.argument_offsets[idx.binding + rw_offset];
#if defined(FLOOR_DEBUG)
		if (write_offset + desc_data.size_bytes() > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size());
	}
}

template <typename T, typename F>
floor_inline_always static void set_image_array_argument(vulkan_encoder& encoder,
														 const vulkan_kernel::vulkan_kernel_entry& entry,
														 const vulkan_kernel::idx_handler& idx,
														 const span<uint8_t>& host_desc_data,
														 const vector<T>& image_array, F&& image_accessor) {
	if (idx.is_implicit) {
		log_error("implicit image argument is not supported yet - should not be here");
		return;
	}
	
	// TODO: write/read-write array support
	
	// transition images to appropriate layout
	const auto img_access = entry.info->args[idx.arg].image_access;
	if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		for (auto& img : image_array) {
			image_accessor(img)->transition_write(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
												  // also readable?
												  img_access == ARG_IMAGE_ACCESS::READ_WRITE,
												  // always direct-write, never attachment
												  true,
												  // allow general layout?
												  encoder.allow_generic_layout);
		}
	} else { // READ
		for (auto& img : image_array) {
			image_accessor(img)->transition_read(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
												 // allow general layout?
												 encoder.allow_generic_layout);
		}
	}
	
	//
	const auto elem_count = entry.info->args[idx.arg].size;
	const auto desc_data_size = encoder.device.desc_buffer_sizes.sampled_image;
	const auto write_offset = entry.desc_buffer.argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (elem_count != image_array.size()) {
		log_error("invalid image array: expected $ elements, got $ elements", elem_count, image_array.size());
		return;
	}
	const auto desc_data_total_size = desc_data_size * elem_count;
	if (write_offset + desc_data_total_size > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor buffer write");
	}
#endif
	
	for (uint32_t i = 0; i < elem_count; ++i) {
		const auto& desc_data = image_accessor(image_array[i])->get_vulkan_descriptor_data_sampled();
		memcpy(host_desc_data.data() + write_offset + desc_data_size * i, desc_data.data(), desc_data_size);
	}
}

void vulkan_kernel::set_argument(vulkan_encoder& encoder,
								 const vulkan_kernel_entry& entry,
								 const idx_handler& idx,
								 const span<uint8_t>& host_desc_data,
								 const vector<shared_ptr<compute_image>>& arg) const {
	set_image_array_argument(encoder, entry, idx, host_desc_data, arg, [](const shared_ptr<compute_image>& img) {
		return static_cast<vulkan_image*>(const_cast<compute_image*>(img.get()));
	});
}

void vulkan_kernel::set_argument(vulkan_encoder& encoder,
								 const vulkan_kernel_entry& entry,
								 const idx_handler& idx,
								 const span<uint8_t>& host_desc_data,
								 const vector<compute_image*>& arg) const {
	set_image_array_argument(encoder, entry, idx, host_desc_data, arg, [](const compute_image* img) {
		return static_cast<vulkan_image*>(const_cast<compute_image*>(img));
	});
}

bool vulkan_kernel::set_and_handle_arguments_legacy(vulkan_encoder& encoder,
													const vector<const vulkan_kernel_entry*>& shader_entries,
													idx_handler& idx,
													const vector<compute_kernel_arg>& args,
													const vector<compute_kernel_arg>& implicit_args) const {
	const size_t arg_count = args.size() + implicit_args.size();
	size_t explicit_idx = 0, implicit_idx = 0;
	for (size_t i = 0; i < arg_count; ++i) {
		auto entry = arg_pre_handler(shader_entries, idx);
		const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
		
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_argument_legacy(encoder, *entry, idx, *buf_ptr);
		} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Vulkan");
		} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Vulkan");
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_argument_legacy(encoder, *entry, idx, *img_ptr);
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			set_argument_legacy(encoder, *entry, idx, **vec_img_ptrs);
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			set_argument_legacy(encoder, *entry, idx, **vec_img_sptrs);
		} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			const auto arg_storage_buf = (*arg_buf_ptr)->get_storage_buffer();
			set_argument_legacy(encoder, *entry, idx, arg_storage_buf);
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_argument_legacy(encoder, *entry, idx, *generic_arg_ptr, arg.size);
		} else {
			log_error("encountered invalid arg");
			return false;
		}
		
		arg_post_handler(*entry, idx);
	}
	return true;
}

void vulkan_kernel::set_argument_legacy(vulkan_encoder& encoder,
										const vulkan_kernel_entry& entry,
										const idx_handler& idx,
										const void* ptr, const size_t& size) const {
	// -> inline uniform buffer
	if (!idx.is_implicit && entry.info->args[idx.arg].special_type == SPECIAL_TYPE::IUB) {
		// TODO: size must be a multiple of 4
		auto& iub_write_desc = encoder.legacy.iub_descs[idx.legacy.iub];
		iub_write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
		iub_write_desc.pNext = nullptr;
		iub_write_desc.dataSize = uint32_t(size);
		iub_write_desc.pData = ptr;
		
		auto& write_desc = encoder.legacy.write_descs[idx.legacy.write_desc];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = &iub_write_desc;
		write_desc.dstSet = encoder.legacy.acquired_descriptor_sets[idx.entry].desc_set;
		write_desc.dstBinding = idx.binding;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = uint32_t(size);
		write_desc.descriptorType = entry.desc_legacy.desc_types[idx.binding];
		write_desc.pImageInfo = nullptr;
		write_desc.pBufferInfo = nullptr;
		write_desc.pTexelBufferView = nullptr;
	}
	// -> plain old SSBO
	else {
		auto& const_buffer = encoder.acquired_constant_buffers[idx.entry].first;
		void* const_buffer_mapping = encoder.constant_buffer_mappings[idx.entry];
		assert(const_buffer_mapping != nullptr);
		const auto& const_buffer_info = encoder.entries[idx.entry]->constant_buffer_info.at(idx.arg);
		assert(const_buffer_info.size == size);
		memcpy((uint8_t*)const_buffer_mapping + const_buffer_info.offset, ptr, const_buffer_info.size);
		
		auto buffer_info = make_unique<VkDescriptorBufferInfo>(VkDescriptorBufferInfo {
			.buffer = ((vulkan_buffer*)const_buffer)->get_vulkan_buffer(),
			.offset = const_buffer_info.offset,
			.range = const_buffer_info.size,
		});
		auto buffer_info_override = buffer_info.get();
		encoder.constant_buffer_desc_info.emplace_back(move(buffer_info));
		set_argument_legacy(encoder, entry, idx, const_buffer, buffer_info_override);
	}
}

void vulkan_kernel::set_argument_legacy(vulkan_encoder& encoder,
										const vulkan_kernel_entry& entry,
										const idx_handler& idx,
										const compute_buffer* arg,
										const VkDescriptorBufferInfo* buffer_info_override) const {
	const auto vk_buffer = ((const vulkan_buffer*)arg)->get_underlying_vulkan_buffer_safe();
	
	auto& write_desc = encoder.legacy.write_descs[idx.legacy.write_desc];
	write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_desc.pNext = nullptr;
	write_desc.dstSet = encoder.legacy.acquired_descriptor_sets[idx.entry].desc_set;
	write_desc.dstBinding = idx.binding;
	write_desc.dstArrayElement = 0;
	write_desc.descriptorCount = 1;
	write_desc.descriptorType = entry.desc_legacy.desc_types[idx.binding];
	write_desc.pImageInfo = nullptr;
	write_desc.pBufferInfo = (buffer_info_override == nullptr ? vk_buffer->get_vulkan_buffer_info() : buffer_info_override);
	write_desc.pTexelBufferView = nullptr;
	
	// TODO/NOTE: use dynamic offset if we ever need it
	//encoder.dyn_offsets.emplace_back(...);
}

void vulkan_kernel::set_argument_legacy(vulkan_encoder& encoder,
										const vulkan_kernel_entry& entry,
										const idx_handler& idx,
										const compute_image* arg) const {
	if (idx.is_implicit) {
		log_error("implicit image argument is not supported yet - should not be here");
		return;
	}
	
	auto vk_img = static_cast<vulkan_image*>(const_cast<compute_image*>(arg))->get_underlying_vulkan_image_safe();
	
	// transition image to appropriate layout
	const auto img_access = entry.info->args[idx.arg].image_access;
	if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		vk_img->transition_write(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
								 // also readable?
								 img_access == ARG_IMAGE_ACCESS::READ_WRITE,
								 // always direct-write, never attachment
								 true,
								 // allow general layout?
								 encoder.allow_generic_layout);
	} else { // READ
		vk_img->transition_read(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
								// allow general layout?
								encoder.allow_generic_layout);
	}
	
	// read image desc/obj
	if (img_access == ARG_IMAGE_ACCESS::READ ||
		img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		auto& write_desc = encoder.legacy.write_descs[idx.legacy.write_desc];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = encoder.legacy.acquired_descriptor_sets[idx.entry].desc_set;
		write_desc.dstBinding = idx.binding;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = 1;
		write_desc.descriptorType = entry.desc_legacy.desc_types[idx.binding];
		write_desc.pImageInfo = vk_img->get_vulkan_image_info();
		write_desc.pBufferInfo = nullptr;
		write_desc.pTexelBufferView = nullptr;
	}
	
	// write image descs/objs
	if (img_access == ARG_IMAGE_ACCESS::WRITE ||
		img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		const auto& mip_info = vk_img->get_vulkan_mip_map_image_info();
		const uint32_t rw_offset = (img_access == ARG_IMAGE_ACCESS::READ_WRITE ? 1u : 0u);
		
		auto& write_desc = encoder.legacy.write_descs[idx.legacy.write_desc + rw_offset];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = encoder.legacy.acquired_descriptor_sets[idx.entry].desc_set;
		write_desc.dstBinding = idx.binding + rw_offset;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = uint32_t(mip_info.size());
		write_desc.descriptorType = entry.desc_legacy.desc_types[idx.binding + rw_offset];
		write_desc.pImageInfo = mip_info.data();
		write_desc.pBufferInfo = nullptr;
		write_desc.pTexelBufferView = nullptr;
	}
}

template <typename T, typename F>
floor_inline_always static void set_image_array_argument_legacy(vulkan_encoder& encoder,
																const vulkan_kernel::vulkan_kernel_entry& entry,
																const vulkan_kernel::idx_handler& idx,
																const vector<T>& image_array, F&& image_accessor) {
	if (idx.is_implicit) {
		log_error("implicit image argument is not supported yet - should not be here");
		return;
	}
	
	// TODO: write/read-write array support
	
	// transition images to appropriate layout
	const auto img_access = entry.info->args[idx.arg].image_access;
	if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
		for (auto& img : image_array) {
			image_accessor(img)->transition_write(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
												  // also readable?
												  img_access == ARG_IMAGE_ACCESS::READ_WRITE,
												  // always direct-write, never attachment
												  true,
												  // allow general layout?
												  encoder.allow_generic_layout);
		}
	} else { // READ
		for (auto& img : image_array) {
			image_accessor(img)->transition_read(encoder.cqueue, encoder.cmd_buffer.cmd_buffer,
												 // allow general layout?
												 encoder.allow_generic_layout);
		}
	}
	
	//
	const auto elem_count = entry.info->args[idx.arg].size;
#if defined(FLOOR_DEBUG)
	if (elem_count != image_array.size()) {
		log_error("invalid image array: expected $ elements, got $ elements", elem_count, image_array.size());
		return;
	}
#endif
	
	// need to heap allocate this, because the actual write/update will happen later
	auto image_info = make_shared<vector<VkDescriptorImageInfo>>(elem_count);
	for (uint32_t i = 0; i < elem_count; ++i) {
		memcpy(&(*image_info)[i], image_accessor(image_array[i])->get_vulkan_image_info(), sizeof(VkDescriptorImageInfo));
	}
	encoder.legacy.image_array_info.emplace_back(image_info);
	
	auto& write_desc = encoder.legacy.write_descs[idx.legacy.write_desc];
	write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_desc.pNext = nullptr;
	write_desc.dstSet = encoder.legacy.acquired_descriptor_sets[idx.entry].desc_set;
	write_desc.dstBinding = idx.binding;
	write_desc.dstArrayElement = 0;
	write_desc.descriptorCount = elem_count;
	write_desc.descriptorType = entry.desc_legacy.desc_types[idx.binding];
	write_desc.pImageInfo = image_info->data();
	write_desc.pBufferInfo = nullptr;
	write_desc.pTexelBufferView = nullptr;
}

void vulkan_kernel::set_argument_legacy(vulkan_encoder& encoder,
										const vulkan_kernel_entry& entry,
										const idx_handler& idx,
										const vector<shared_ptr<compute_image>>& arg) const {
	set_image_array_argument_legacy(encoder, entry, idx, arg, [](const shared_ptr<compute_image>& img) {
		return static_cast<vulkan_image*>(const_cast<compute_image*>(img.get()));
	});
}

void vulkan_kernel::set_argument_legacy(vulkan_encoder& encoder,
										const vulkan_kernel_entry& entry,
										const idx_handler& idx,
										const vector<compute_image*>& arg) const {
	set_image_array_argument_legacy(encoder, entry, idx, arg, [](const compute_image* img) {
		return static_cast<vulkan_image*>(const_cast<compute_image*>(img));
	});
}

const compute_kernel::kernel_entry* vulkan_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const vulkan_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
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
	VkDeviceSize arg_buffer_size = 0;
	vk_ctx.vulkan_get_descriptor_set_layout_size(vk_dev.device, arg_buf_info.layout.desc_set_layout, &arg_buffer_size);
	arg_buffer_size = const_math::round_next_multiple(arg_buffer_size, uint64_t(256));
	
	// query offset for each binding/argument
	vector<VkDeviceSize> argument_offsets;
	argument_offsets.reserve(arg_buf_info.layout.bindings.size());
	for (const auto& binding : arg_buf_info.layout.bindings) {
		VkDeviceSize offset = 0;
		vk_ctx.vulkan_get_descriptor_set_layout_binding_offset(vk_dev.device, arg_buf_info.layout.desc_set_layout, binding.binding, &offset);
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
