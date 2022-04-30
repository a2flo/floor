/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_KERNEL_HPP__
#define __FLOOR_VULKAN_KERNEL_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_descriptor_set.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/compute_kernel_arg.hpp>

class vulkan_device;
class vulkan_queue;
struct vulkan_encoder;
struct vulkan_command_buffer;

class vulkan_kernel : public compute_kernel {
public:
	struct vulkan_kernel_entry : kernel_entry {
		VkPipelineLayout pipeline_layout { nullptr };
		VkPipelineShaderStageCreateInfo stage_info;
		VkDescriptorSetLayout desc_set_layout { nullptr };
		VkDescriptorPool desc_pool { nullptr };
		unique_ptr<vulkan_descriptor_set_container> desc_set_container;
		vector<VkDescriptorType> desc_types;
		
		//! buffers/storage for constant data
		//! NOTE: must be the same amount as the number of descriptors in "desc_set_container"
		array<shared_ptr<compute_buffer>, vulkan_descriptor_set_container::descriptor_count> constant_buffers_storage;
		array<void*, vulkan_descriptor_set_container::descriptor_count> constant_buffer_mappings;
		unique_ptr<safe_resource_container<compute_buffer*, vulkan_descriptor_set_container::descriptor_count>> constant_buffers;
		//! argument index -> constant buffer info
		struct constant_buffer_info_t {
			uint32_t offset;
			uint32_t size;
		};
		flat_map<uint32_t, constant_buffer_info_t> constant_buffer_info;
		
		struct spec_entry {
			VkPipeline pipeline { nullptr };
			VkSpecializationInfo info;
			vector<VkSpecializationMapEntry> map_entries;
			vector<uint32_t> data;
		};
		// must sync access to specializations
		atomic_spin_lock specializations_lock;
		// work-group size -> spec entry
		flat_map<uint64_t, spec_entry> specializations GUARDED_BY(specializations_lock);
		
		//! creates a 64-bit key out of the specified uint3 work-group size
		//! NOTE: components of the work-group size must fit into 16-bit
		static uint64_t make_spec_key(const uint3& work_group_size);
		
		//! specializes/builds a compute pipeline for the specified work-group size
		vulkan_kernel_entry::spec_entry* specialize(const vulkan_device& device,
													const uint3& work_group_size) REQUIRES(specializations_lock);
	};
	typedef flat_map<const vulkan_device&, vulkan_kernel_entry> kernel_map_type;
	
	struct idx_handler {
		// actual argument index (directly corresponding to the c++ source code)
		uint32_t arg { 0 };
		// index into the descriptor set that will be updated/written
		uint32_t write_desc { 0 };
		// binding index in the resp. descriptor set
		uint32_t binding { 0 };
		// IUB index in the resp. descriptor set
		uint32_t iub { 0 };
		// flag if this is an implicit arg
		bool is_implicit { false };
		// current implicit argument index
		uint32_t implicit { 0 };
		// current kernel/shader entry
		uint32_t entry { 0 };
	};
	
	vulkan_kernel(kernel_map_type&& kernels);
	~vulkan_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args,
				 const vector<const compute_fence*>& wait_fences,
				 const vector<const compute_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device& dev) const override;
	
protected:
	mutable kernel_map_type kernels;
	
	typename kernel_map_type::iterator get_kernel(const compute_queue& queue) const;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }
	
	shared_ptr<vulkan_encoder> create_encoder(const compute_queue& queue,
											  const vulkan_command_buffer* cmd_buffer,
											  const VkPipeline pipeline,
											  const VkPipelineLayout pipeline_layout,
											  const vector<const vulkan_kernel_entry*>& entries,
											  const char* debug_label,
											  bool& success) const;
	VkPipeline get_pipeline_spec(const vulkan_device& device,
								 vulkan_kernel_entry& entry,
								 const uint3& work_group_size) const REQUIRES(!entry.specializations_lock);
	
	bool set_and_handle_arguments(vulkan_encoder& encoder,
								  const vector<const vulkan_kernel_entry*>& shader_entries,
								  idx_handler& idx,
								  const vector<compute_kernel_arg>& args,
								  const vector<compute_kernel_arg>& implicit_args) const;
	
	// actual argument setters
	void set_argument(vulkan_encoder& encoder,
					  const vulkan_kernel_entry& entry,
					  const idx_handler& idx,
					  const void* ptr, const size_t& size) const;
	
	void set_argument(vulkan_encoder& encoder,
					  const vulkan_kernel_entry& entry,
					  const idx_handler& idx,
					  const compute_buffer* arg,
					  const VkDescriptorBufferInfo* buffer_info_override = nullptr) const;
	
	void set_argument(vulkan_encoder& encoder,
					  const vulkan_kernel_entry& entry,
					  const idx_handler& idx,
					  const compute_image* arg) const;
	
	void set_argument(vulkan_encoder& encoder,
					  const vulkan_kernel_entry& entry,
					  const idx_handler& idx,
					  const vector<shared_ptr<compute_image>>& arg) const;
	void set_argument(vulkan_encoder& encoder,
					  const vulkan_kernel_entry& entry,
					  const idx_handler& idx,
					  const vector<compute_image*>& arg) const;
	
	unique_ptr<argument_buffer> create_argument_buffer_internal(const compute_queue& cqueue,
																const kernel_entry& entry,
																const llvm_toolchain::arg_info& arg,
																const uint32_t& user_arg_index,
																const uint32_t& ll_arg_index,
																const COMPUTE_MEMORY_FLAG& add_mem_flags) const override;
	
};

#endif

#endif
