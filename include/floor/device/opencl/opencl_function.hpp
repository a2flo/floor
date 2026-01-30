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

#include <floor/device/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/device/opencl/opencl_buffer.hpp>
#include <floor/device/opencl/opencl_image.hpp>
#include <floor/device/device_function.hpp>

namespace fl {

class opencl_device;

class opencl_function final : public device_function {
protected:
	struct arg_handler {
		bool needs_param_workaround { false };
		const device_queue* cqueue;
		const device* device;
		std::vector<std::shared_ptr<opencl_buffer>> args;
	};
	std::shared_ptr<arg_handler> create_arg_handler(const device_queue& cqueue) const;
	
public:
	struct opencl_function_entry : function_entry {
		cl_kernel function { nullptr };
	};
	using function_map_type = fl::flat_map<const opencl_device*, opencl_function_entry>;
	
	opencl_function(const std::string_view function_name_, function_map_type&& functions);
	~opencl_function() override = default;
	
	void execute(const device_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const std::vector<device_function_arg>& args,
				 const std::vector<const device_fence*>& wait_fences,
				 const std::vector<device_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const function_entry* get_function_entry(const device& dev) const override;
	
protected:
	const function_map_type functions;
	
	mutable atomic_spin_lock args_lock;
	
	typename function_map_type::const_iterator get_function(const device_queue& cqueue) const;
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::OPENCL; }
	
	//! actual function argument setters
	void set_const_function_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler,
								   const opencl_function_entry& entry,
								   void* arg, const size_t arg_size) const;
	
	void set_function_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler*,
							 const opencl_function_entry& entry,
							 const device_buffer* arg) const;
	
	floor_inline_always void set_function_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler,
												 const opencl_function_entry& entry,
												 const device_image* arg) const;
	
};

} // namespace fl

#endif
