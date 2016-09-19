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

#ifndef __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__
#define __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__

//! misc compute algorithms, specialized for each backend/hardware
namespace compute_algorithm {
	// TODO: sg incl/excl scan, broadcast, radix/bitonic sort, histogram
	
	//////////////////////////////////////////
	// sub-group reduce functions
	
#if defined(FLOOR_COMPUTE_CUDA)
	//! performs a butterfly reduction inside the sub-group using the specific operation/function
	//! NOTE: sm_30 or higher only
	template <typename T, typename F, enable_if_t<device_info::cuda_sm() >= 30 && sizeof(T) == 4>* = nullptr>
	floor_inline_always static T sub_group_reduce(T lane_var, F&& op) {
		T shfled_var;
#pragma unroll
		for(uint32_t lane = device_info::simd_width() / 2; lane > 0; lane >>= 1) {
			if constexpr(is_floating_point<T>::value) {
				asm volatile("shfl.bfly.b32 %0, %1, %2, %3;"
							 : "=f"(shfled_var) : "f"(lane_var), "i"(lane), "i"(device_info::simd_width() - 1));
			}
			else asm volatile("shfl.bfly.b32 %0, %1, %2, %3;"
							  : "=r"(shfled_var) : "r"(lane_var), "i"(lane), "i"(device_info::simd_width() - 1));
			lane_var = op(lane_var, shfled_var);
		}
		return lane_var;
	}
	template <typename T> floor_inline_always static T sub_group_reduce_add(T lane_var) {
		return sub_group_reduce(lane_var, plus<> {});
	}
	template <typename T> floor_inline_always static T sub_group_reduce_min(T lane_var) {
		return sub_group_reduce(lane_var, [](const auto& lhs, const auto& rhs) { return ::min(lhs, rhs); });
	}
	template <typename T> floor_inline_always static T sub_group_reduce_max(T lane_var) {
		return sub_group_reduce(lane_var, [](const auto& lhs, const auto& rhs) { return ::max(lhs, rhs); });
	}
	
#elif defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
#if defined(FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS)
	// just forward to global functions for opencl
	template <typename T> floor_inline_always static T sub_group_reduce_add(T lane_var) {
		return ::sub_group_reduce_add(lane_var);
	}
	template <typename T> floor_inline_always static T sub_group_reduce_min(T lane_var) {
		return ::sub_group_reduce_min(lane_var);
	}
	template <typename T> floor_inline_always static T sub_group_reduce_max(T lane_var) {
		return ::sub_group_reduce_max(lane_var);
	}
#endif
#endif
	
	//////////////////////////////////////////
	// work-group reduce functions
	// TODO: add specialized reduce (sub-group/warp, shuffle, spir/spir-v built-in)
	
	//! generic work-group reduce function, without initializing local memory with a work-item specific value
	//! NOTE: only work-item #0 (local_id.x == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename reduced_type, typename local_memory_type, typename F>
	floor_inline_always static reduced_type reduce_no_init(local_memory_type& lmem,
														   F&& op) {
		const auto lid = local_id.x;
		
#if !defined(FLOOR_COMPUTE_HOST)
		auto value = lmem[lid];
		// butterfly reduce to [0]
#pragma unroll
		for(uint32_t i = work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_COMPUTE_CUDA)
			if(i >= device_info::simd_width())
#endif
			{
				local_barrier();
			}
			if(lid < i) {
				value = op(value, lmem[lid + i]);
				if(i > 1) {
					lmem[lid] = value;
				}
			}
		}
#else // -> host-compute
		// make sure everyone has written to local memory
		local_barrier();
		// reduce in the first work-item only
		if(lid == 0) {
			auto& arr = lmem.as_array();
			for(uint32_t i = 1; i < work_group_size; ++i) {
				arr[0] = op(arr[0], arr[i]);
			}
		}
#endif
		return lmem[0];
	}
	
	//! generic work-group reduce function
	//! NOTE: only work-item #0 (local_id.x == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename reduced_type, typename local_memory_type, typename F>
	floor_inline_always static reduced_type reduce(const reduced_type& work_item_value,
												   local_memory_type& lmem,
												   F&& op) {
		// init/set all work-item values
		const auto lid = local_id.x;
		auto value = work_item_value;
		lmem[lid] = value;
		
#if !defined(FLOOR_COMPUTE_HOST)
		// butterfly reduce to [0]
#pragma unroll
		for(uint32_t i = work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_COMPUTE_CUDA)
			if(i >= device_info::simd_width())
#endif
			{
				local_barrier();
			}
			if(lid < i) {
				value = op(value, lmem[lid + i]);
				if(i > 1) {
					lmem[lid] = value;
				}
			}
		}
		return value;
#else // -> host-compute
		// make sure everyone has written to local memory
		local_barrier();
		// reduce in the first work-item only
		if(lid == 0) {
			auto& arr = lmem.as_array();
			for(uint32_t i = 1; i < work_group_size; ++i) {
				arr[0] = op(arr[0], arr[i]);
			}
		}
		return lmem[0];
#endif
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size>
	static constexpr uint32_t reduce_local_memory_elements() {
		return work_group_size;
	}
	
	//////////////////////////////////////////
	// work-group scan functions
	// TODO: add specialized scans (sub-group/warp, shuffle, spir/spir-v built-in)
	
	//! generic work-group scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, bool inclusive, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto scan(const data_type& work_item_value,
										 op_func_type&& op,
										 lmem_type& lmem,
										 const data_type zero_val = (data_type)0) {
		const auto lid = local_id.x;
		auto value = work_item_value;
		
#if !defined(FLOOR_COMPUTE_HOST)
		lmem[lid] = value;
		local_barrier();
		
		uint32_t side_idx = 0;
#pragma unroll
		for(uint32_t offset = 1; offset < work_group_size; offset <<= 1) {
			if(lid >= offset) {
				value = op(lmem[side_idx + lid - offset], value);
			}
			side_idx = work_group_size - side_idx; // swap side
			lmem[side_idx + lid] = value;
			local_barrier();
		}
		
#if defined(FLOOR_CXX17)
		// inclusive
		if constexpr(inclusive) {
			return value; // value == lmem[side_idx + lid] at this point
		}
		// exclusive
		else {
			const auto ret = (lid == 0 ? zero_val : lmem[side_idx + lid - 1]);
			local_barrier();
			return ret;
		}
#else
		return __builtin_choose_expr(inclusive,
									 value,
									 (value = (lid == 0 ? zero_val : lmem[side_idx + lid - 1]), local_barrier(), value));
#endif
#else // -> host-compute
		lmem[inclusive ? lid : lid + 1] = value;
		local_barrier();
		
		if(lid == 0) {
			// exclusive: #0 has not been set yet -> init with zero
			if(!inclusive) {
				lmem[0] = zero_val;
			}
			
			// just forward scan
			auto& arr = lmem.as_array();
			for(uint32_t i = 1; i < work_group_size; ++i) {
				arr[i] = op(arr[i - 1], arr[i]);
			}
		}
		
		// sync once so that lmem can safely be used again outside of this function
		const auto ret = lmem[lid];
		local_barrier();
		return ret;
#endif
	}
	
	//! generic work-group inclusive-scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan(const data_type& work_item_value,
												   op_func_type&& op,
												   lmem_type& lmem,
												   const data_type zero_val = (data_type)0) {
		return scan<work_group_size, true>(work_item_value, std::forward<op_func_type>(op), lmem, zero_val);
	}
	
	//! generic work-group exclusive-scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan(const data_type& work_item_value,
												   op_func_type&& op,
												   lmem_type& lmem,
												   const data_type zero_val = (data_type)0) {
		return scan<work_group_size, false>(work_item_value, std::forward<op_func_type>(op), lmem, zero_val);
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size>
	static constexpr uint32_t scan_local_memory_elements() {
		return work_group_size * 2u - 1u;
	}
	
}

#endif
