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

#ifndef __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__
#define __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__

//! misc compute algorithms, specialized for each backend/hardware
namespace compute_algorithm {
	// TODO: broadcast, radix/bitonic sort, histogram
	
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
			if constexpr(is_floating_point_v<T>) {
				asm volatile(FLOOR_CUDA_SHFL ".bfly.b32 %0, %1, %2, %3" FLOOR_CUDA_SHFL_MASK ";"
							 : "=f"(shfled_var) : "f"(lane_var), "i"(lane), "i"(device_info::simd_width() - 1));
			}
			else {
				asm volatile(FLOOR_CUDA_SHFL ".bfly.b32 %0, %1, %2, %3" FLOOR_CUDA_SHFL_MASK ";"
							 : "=r"(shfled_var) : "r"(lane_var), "i"(lane), "i"(device_info::simd_width() - 1));
			}
			lane_var = op(lane_var, shfled_var);
		}
		return lane_var;
	}
	template <typename T, typename F, enable_if_t<device_info::cuda_sm() >= 30 && sizeof(T) == 8>* = nullptr>
	floor_inline_always static T sub_group_reduce(T lane_var, F&& op) {
		T shfled_var;
#pragma unroll
		for(uint32_t lane = device_info::simd_width() / 2; lane > 0; lane >>= 1) {
			uint32_t shfled_hi, shfled_lo, hi, lo;
			if constexpr(is_floating_point_v<T>) {
				asm volatile("mov.b64 { %0, %1 }, %2;" : "=r"(lo), "=r"(hi) : "d"(lane_var));
			}
			else {
				asm volatile("mov.b64 { %0, %1 }, %2;" : "=r"(lo), "=r"(hi) : "l"(lane_var));
			}
			asm volatile(FLOOR_CUDA_SHFL ".bfly.b32 %0, %2, %4, %5" FLOOR_CUDA_SHFL_MASK ";\n"
						 "\t" FLOOR_CUDA_SHFL ".bfly.b32 %1, %3, %4, %5" FLOOR_CUDA_SHFL_MASK ";"
						 : "=r"(shfled_lo), "=r"(shfled_hi)
						 : "r"(lo), "r"(hi), "i"(lane), "i"(device_info::simd_width() - 1));
			if constexpr(is_floating_point_v<T>) {
				asm volatile("mov.b64 %0, { %1, %2 };" : "=d"(shfled_var) : "r"(shfled_lo), "r"(shfled_hi));
			}
			else {
				asm volatile("mov.b64 %0, { %1, %2 };" : "=l"(shfled_var) : "r"(shfled_lo), "r"(shfled_hi));
			}
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
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
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
	
#elif defined(FLOOR_COMPUTE_METAL)
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
	//! performs a butterfly reduction inside the sub-group using the specific operation/function
	template <typename T, typename F, enable_if_t<is_same_v<T, int32_t> || is_same_v<T, uint32_t> || is_same_v<T, float>>* = nullptr>
	floor_inline_always static T sub_group_reduce(T lane_var, F&& op) {
		// on Metal we only have a fixed+known SIMD-width at compile-time when we're specifically compiling for a device
		if constexpr (device_info::has_fixed_known_simd_width()) {
			T shfled_var;
#pragma unroll
			for(uint32_t lane = device_info::simd_width() / 2; lane > 0; lane >>= 1) {
				shfled_var = simd_shuffle_xor(lane_var, lane);
				lane_var = op(lane_var, shfled_var);
			}
			return lane_var;
		} else {
			// dynamic version
			T shfled_var;
			for(uint32_t lane = sub_group_size / 2; lane > 0; lane >>= 1) {
				shfled_var = simd_shuffle_xor(lane_var, lane);
				lane_var = op(lane_var, shfled_var);
			}
			return lane_var;
		}
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
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_INFO_VENDOR_APPLE) || defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA)
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
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_INFO_VENDOR_APPLE) || defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA)
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
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
			auto& arr = lmem.as_array();
#else
			auto& arr = lmem;
#endif
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
	// TODO: add specialized scans (spir/spir-v built-in)

	//! returns true if the inclusive/exclusive scan implementation uses the sub-group path
	static constexpr bool has_sub_group_scan() {
		constexpr const bool pot_simd { const_math::popcount(device_info::simd_width()) == 1 };
		return (device_info::has_sub_group_shuffle() &&
				device_info::has_fixed_known_simd_width() &&
				device_info::simd_width() > 0u &&
				pot_simd);
	}
	
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
		
#if !defined(FLOOR_COMPUTE_HOST)
		if constexpr (has_sub_group_scan()) {
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
			constexpr const auto simd_width = device_info::simd_width();
			constexpr const auto group_count = work_group_size / simd_width;
			static_assert((work_group_size % simd_width) == 0u, "work-group size must be a multiple of SIMD-width");
			const auto lane = sub_group_local_id;
			const auto group = sub_group_id_1d;
			
			// scan in sub-group
			data_type shfled_var;
			data_type scan_value = work_item_value;
#pragma unroll
			for (uint32_t lane_idx = 1u; lane_idx != simd_width; lane_idx <<= 1u) {
				shfled_var = simd_shuffle_up(scan_value, lane_idx);
				if (lane >= lane_idx) {
					scan_value = op(shfled_var, scan_value);
				}
			}
			
			// broadcast last value in each scan (for other sub-groups)
			if (lane == (simd_width - 1u)) {
				lmem[group] = scan_value;
			}
			local_barrier();
			
			// scan per-sub-group results in the first sub-group
			if (group == 0u) {
				// init each lane with the result (most right) value of each sub-group
				data_type group_scan_value {};
				if constexpr (group_count == simd_width) {
					group_scan_value = lmem[lane];
				} else {
					group_scan_value = (lane < group_count ? lmem[lane] : zero_val);
				}
				
#pragma unroll
				for (uint32_t lane_idx = 1u; lane_idx != simd_width; lane_idx <<= 1u) {
					shfled_var = simd_shuffle_up(group_scan_value, lane_idx);
					if (lane >= lane_idx) {
						group_scan_value = op(shfled_var, group_scan_value);
					}
				}
				
				lmem[lane] = group_scan_value;
			}
			local_barrier();
			
			// broadcast final per-sub-group offsets to all sub-groups
			const auto group_offset = (group > 0u ? lmem[group - 1u] : data_type(0));
			if constexpr (!inclusive) {
				// exclusive: shift one up, #0 in each group returns the offset of the previous group (+ 0)
				shfled_var = simd_shuffle_up(scan_value, 1u);
				scan_value = (lane == 0u ? zero_val : shfled_var);
			}
			return op(group_offset, scan_value);
#endif
		} else {
			// old-school scan
			auto value = work_item_value;
			lmem[lid] = value;
			local_barrier();
			
			uint32_t side_idx = 0u;
#pragma unroll
			for (uint32_t offset = 1u; offset < work_group_size; offset <<= 1u) {
				if (lid >= offset) {
					value = op(lmem[side_idx + lid - offset], value);
				}
				side_idx = work_group_size - side_idx; // swap side
				lmem[side_idx + lid] = value;
				local_barrier();
			}
			
			// inclusive
			if constexpr (inclusive) {
				return value; // value == lmem[side_idx + lid] at this point
			}
			// exclusive
			const auto ret = (lid == 0u ? zero_val : lmem[side_idx + lid - 1u]);
			local_barrier();
			return ret;
		}
#else // -> host-compute
		lmem[inclusive ? lid : lid + 1] = work_item_value;
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
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
		if constexpr (has_sub_group_scan()) {
			static_assert(device_info::simd_width() * device_info::simd_width() >= device_info::local_id_range_max(),
						  "unexpected SIMD-width / max work-group size");
#if !defined(FLOOR_COMPUTE_METAL) && !defined(FLOOR_COMPUTE_INFO_VENDOR_AMD)
			return device_info::simd_width();
#else
			// more padding on Metal/AMD
			return device_info::simd_width() * 2u;
#endif
		}
#endif
#if !defined(FLOOR_COMPUTE_METAL) && !defined(FLOOR_COMPUTE_INFO_VENDOR_AMD)
		return work_group_size * 2u - 1u;
#else
		// need the padding on Metal/AMD
		return work_group_size * 2u;
#endif
	}
	
}

#endif
