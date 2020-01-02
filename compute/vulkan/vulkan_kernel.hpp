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

#ifndef __FLOOR_VULKAN_KERNEL_HPP__
#define __FLOOR_VULKAN_KERNEL_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
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
		VkDescriptorSet desc_set { nullptr };
		vector<VkDescriptorType> desc_types;
		
		struct spec_entry {
			VkPipeline pipeline { nullptr };
			VkSpecializationInfo info;
			vector<VkSpecializationMapEntry> map_entries;
			vector<uint32_t> data;
		};
		// work-group size -> spec entry
		flat_map<uint64_t, spec_entry> specializations;
		
		//! creates a 64-bit key out of the specified uint3 work-group size
		//! NOTE: components of the work-group size must fit into 16-bit
		static uint64_t make_spec_key(const uint3& work_group_size);
		
		//! specializes/builds a compute pipeline for the specified work-group size
		vulkan_kernel_entry::spec_entry* specialize(const vulkan_device& device,
													const uint3& work_group_size);
	};
	typedef flat_map<const vulkan_device&, vulkan_kernel_entry> kernel_map_type;
	
	struct idx_handler {
		// actual argument index (directly corresponding to the c++ source code)
		uint32_t arg { 0 };
		// index into the descriptor set that will be updated/written
		// NOTE: starts out at 1, because 0 is the fixed sampler set
		uint32_t write_desc { 1 };
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
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
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
											  bool& success) const;
	VkPipeline get_pipeline_spec(const vulkan_device& device,
								 vulkan_kernel_entry& entry,
								 const uint3& work_group_size) const;
	
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
					  const compute_buffer* arg) const;
	
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
	
};

#endif

#endif
