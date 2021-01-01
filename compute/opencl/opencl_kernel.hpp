/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_KERNEL_HPP__
#define __FLOOR_OPENCL_KERNEL_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_image.hpp>
#include <floor/compute/compute_kernel.hpp>

class opencl_device;

class opencl_kernel final : public compute_kernel {
protected:
	struct arg_handler {
		bool needs_param_workaround { false };
		const compute_queue* cqueue;
		const compute_device* device;
		vector<shared_ptr<opencl_buffer>> args;
	};
	shared_ptr<arg_handler> create_arg_handler(const compute_queue& cqueue) const;
	
public:
	struct opencl_kernel_entry : kernel_entry {
		cl_kernel kernel { nullptr };
	};
	typedef flat_map<const opencl_device&, opencl_kernel_entry> kernel_map_type;
	
	opencl_kernel(kernel_map_type&& kernels);
	~opencl_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device& dev) const override;
	
protected:
	const kernel_map_type kernels;
	
	mutable atomic_spin_lock args_lock;
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue& cqueue) const;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	//! actual kernel argument setters
	void set_const_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler,
								   const opencl_kernel_entry& entry,
								   void* arg, const size_t arg_size) const;
	
	void set_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler*,
							 const opencl_kernel_entry& entry,
							 const compute_buffer* arg) const;
	
	floor_inline_always void set_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler,
												 const opencl_kernel_entry& entry,
												 const compute_image* arg) const;
	
};

#endif

#endif
