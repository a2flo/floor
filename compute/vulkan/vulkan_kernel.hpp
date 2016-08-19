/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

// the amount of macro voodoo is too damn high ...
#define FLOOR_VULKAN_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_VULKAN_KERNEL_IMPL

class vulkan_device;
class vulkan_kernel final : public compute_kernel {
protected:
	// don't want to include vulkan_queue here
	struct vulkan_encoder;
	
public:
	struct vulkan_kernel_entry : kernel_entry {
		VkPipeline pipeline { nullptr };
		VkDescriptorSetLayout desc_set_layout { nullptr };
		VkPipelineLayout pipeline_layout { nullptr };
		VkDescriptorPool desc_pool { nullptr };
		VkDescriptorSet desc_set { nullptr };
		vector<VkDescriptorType> desc_types;
	};
	typedef flat_map<vulkan_device*, vulkan_kernel_entry> kernel_map_type;
	
	vulkan_kernel(kernel_map_type&& kernels);
	~vulkan_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const uint3 global_work_size,
											 const uint3 local_work_size_,
											 Args&&... args) {
		// find entry for queue device
		const auto kernel_iter = get_kernel(queue);
		if(kernel_iter == kernels.cend()) {
			log_error("no kernel for this compute queue/device exists!");
			return;
		}
		
		// check work size
		const uint3 block_dim = check_local_work_size(kernel_iter->second, local_work_size_);
		
		// create command buffer ("encoder") for this kernel execution
		bool encoder_success = false;
		auto encoder = create_encoder(queue, kernel_iter->second, encoder_success);
		if(!encoder_success) {
			log_error("failed to create vulkan encoder / command buffer for kernel \"%s\"", kernel_iter->second.info->name);
			return;
		}
		
		// set and handle kernel arguments
		set_kernel_arguments<0>(encoder.get(), kernel_iter->second, forward<Args>(args)...);
		
		// run
		const uint3 grid_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
		};
		uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
		grid_dim.max(1u);
		
		execute_internal(encoder, queue, kernel_iter->second, work_dim, grid_dim, block_dim);
	}
	
	const kernel_entry* get_kernel_entry(shared_ptr<compute_device> dev) const override {
		const auto ret = kernels.get((vulkan_device*)dev.get());
		return !ret.first ? nullptr : &ret.second->second;
	}
	
protected:
	const kernel_map_type kernels;
	flat_map<const kernel_entry*, bool> warn_map;
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue* queue) const;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }

	shared_ptr<vulkan_encoder> create_encoder(compute_queue* queue, const vulkan_kernel_entry& entry, bool& success);
	
	void execute_internal(shared_ptr<vulkan_encoder> encoder,
						  compute_queue* queue,
						  const vulkan_kernel_entry& entry,
						  const uint32_t& work_dim,
						  const uint3& grid_dim,
						  const uint3& block_dim) const;
	
	//! handle kernel call terminator
	template <uint32_t num>
	floor_inline_always void set_kernel_arguments(vulkan_encoder*, const vulkan_kernel_entry&) const {}
	
	//! set kernel argument and recurse
	template <uint32_t num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
												  T&& arg, Args&&... args) const {
		set_kernel_argument(encoder, entry, num, forward<T>(arg));
		set_kernel_arguments<num + 1>(encoder, entry, forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	template <typename T, enable_if_t<!is_pointer<T>::value>* = nullptr>
	floor_inline_always void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
												 const uint32_t num, T&& arg) const {
		set_kernel_argument(encoder, entry, num, &arg, sizeof(T));
	}
	
	void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
							 const uint32_t num, const void* ptr, const size_t& size) const;
	
	floor_inline_always void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
												 const uint32_t num, shared_ptr<compute_buffer> arg) const {
		set_kernel_argument(encoder, entry, num, arg.get());
	}
	
	floor_inline_always void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
												 const uint32_t num, shared_ptr<compute_image> arg) const {
		set_kernel_argument(encoder, entry, num, arg.get());
	}
	
	void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
							 const uint32_t num, const compute_buffer* arg) const;
	
	void set_kernel_argument(vulkan_encoder* encoder, const vulkan_kernel_entry& entry,
							 const uint32_t num, const compute_image* arg) const;
	
};

#endif

#endif
