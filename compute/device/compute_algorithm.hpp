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
	// TODO: work-group reduce, sg/wg incl/excl scan, broadcast, radix/bitonic sort, histogram
	
#if defined(FLOOR_COMPUTE_CUDA)
	//! performs a butterfly reduction inside the sub-group using the specific operation/function
	//! NOTE: sm_30 or higher only
	template <typename T, typename F, enable_if_t<device_info::cuda_sm() >= 30 && sizeof(T) == 4>* = nullptr>
	floor_inline_always static T sub_group_reduce(T lane_var, F op) {
		T shfled_var;
#pragma unroll
		for(uint32_t lane = device_info::simd_width() / 2; lane > 0; lane >>= 1) {
			if(is_floating_point<T>::value) {
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
	
#elif defined(FLOOR_COMPUTE_OPENCL)
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
}

#endif
