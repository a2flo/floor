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

#ifndef __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__
#define __FLOOR_COMPUTE_DEVICE_COMPUTE_ALGORITHM_HPP__

//! misc compute algorithms, specialized for each backend/hardware
namespace compute_algorithm {
	// TODO: broadcast, radix/bitonic sort, histogram

	//! enforce correct work-group size types
	#if defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE) && __clang_major__ < 15
	template <auto work_group_size>
	concept supported_work_group_size_type = is_same_v<decltype(work_group_size), uint32_t>;
	#else
	template <auto work_group_size>
	concept supported_work_group_size_type = (is_same_v<decltype(work_group_size), uint32_t> ||
											  is_same_v<decltype(work_group_size), uint2> ||
											  is_same_v<decltype(work_group_size), uint3>);
	#endif

	//! computes the linear/1D work group size from the specified 1D, 2D or 3D work group size
	template <auto work_group_size>
	requires(supported_work_group_size_type<work_group_size>)
	static inline constexpr uint32_t compute_linear_work_group_size() {
		if constexpr (is_same_v<decltype(work_group_size), uint32_t>) {
			return work_group_size;
		} else if constexpr (is_same_v<decltype(work_group_size), uint2>) {
			return work_group_size.x * work_group_size.y;
		} else if constexpr (is_same_v<decltype(work_group_size), uint3>) {
			return work_group_size.x * work_group_size.y * work_group_size.z;
		} else {
			instantiation_trap("invalid work_group_size type");
		}
	}
	
	//////////////////////////////////////////
	// sub-group reduce functions
	
	template <typename data_type>
	requires(group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::ADD, data_type>)
	floor_inline_always static data_type sub_group_reduce_add(data_type lane_var) {
		return group::sub_group_reduce<group::OP::ADD>(lane_var);
	}
	template <typename data_type>
	requires(group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::MIN, data_type>)
	floor_inline_always static data_type sub_group_reduce_min(data_type lane_var) {
		return group::sub_group_reduce<group::OP::MIN>(lane_var);
	}
	template <typename data_type>
	requires(group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::MAX, data_type>)
	floor_inline_always static data_type sub_group_reduce_max(data_type lane_var) {
		return group::sub_group_reduce<group::OP::MAX>(lane_var);
	}
	
	//////////////////////////////////////////
	// work-group reduce functions
	
	//! generic work-group reduce function, without initializing local memory with a work-item specific value
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: the reduce function/op must be a binary function
	template <auto work_group_size, typename reduced_type, typename local_memory_type, typename F>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_no_init(local_memory_type& lmem,
														   F&& op) {
		constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
		uint32_t lid = 0u;
		if constexpr (is_same_v<decltype(work_group_size), uint32_t>) {
			lid = local_id.x;
		} else if constexpr (is_same_v<decltype(work_group_size), uint2>) {
			lid = local_id.x + local_id.y * local_size.x;
		} else if constexpr (is_same_v<decltype(work_group_size), uint3>) {
			lid = local_id.x + local_id.y * local_size.x + local_id.z * local_size.x * local_size.y;
		}
		
#if !defined(FLOOR_COMPUTE_HOST)
		auto value = lmem[lid];
		// butterfly reduce to [0]
#pragma unroll
		for (uint32_t i = linear_work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_COMPUTE_INFO_VENDOR_APPLE)
			if(i >= device_info::simd_width())
#endif
			{
				local_barrier();
			}
			if (lid < i) {
				value = op(value, lmem[lid + i]);
				if (i > 1) {
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
			for(uint32_t i = 1; i < linear_work_group_size; ++i) {
				arr[0] = op(arr[0], arr[i]);
			}
		}
		return lmem[0];
#endif
	}
	
	//! generic work-group reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: the reduce function/op must be a binary function
	template <auto work_group_size, typename reduced_type, typename local_memory_type, typename F>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce(const reduced_type& work_item_value,
												   local_memory_type& lmem,
												   F&& op) {
		// init/set all work-item values
		constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
		uint32_t lid = 0u;
		if constexpr (is_same_v<decltype(work_group_size), uint32_t>) {
			lid = local_id.x;
		} else if constexpr (is_same_v<decltype(work_group_size), uint2>) {
			lid = local_id.x + local_id.y * local_size.x;
		} else if constexpr (is_same_v<decltype(work_group_size), uint3>) {
			lid = local_id.x + local_id.y * local_size.x + local_id.z * local_size.x * local_size.y;
		}
		auto value = work_item_value;
		lmem[lid] = value;
		
#if !defined(FLOOR_COMPUTE_HOST)
		// butterfly reduce to [0]
#pragma unroll
		for(uint32_t i = linear_work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_COMPUTE_INFO_VENDOR_APPLE)
			if (i >= device_info::simd_width())
#endif
			{
				local_barrier();
			}
			if (lid < i) {
				value = op(value, lmem[lid + i]);
				if (i > 1) {
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
			for(uint32_t i = 1; i < linear_work_group_size; ++i) {
				arr[0] = op(arr[0], arr[i]);
			}
		}
		return lmem[0];
#endif
	}
	
	//! work-group add/sum reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_add(const reduced_type& work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::ADD, reduced_type>) {
			return group::work_group_reduce<group::OP::ADD>(work_item_value, lmem);
		}
		// TODO: sub-group algo fallback
		return reduce<work_group_size>(work_item_value, lmem, plus<reduced_type> {});
	}
	
	//! work-group min reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_min(const reduced_type& work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::MIN, reduced_type>) {
			return group::work_group_reduce<group::OP::MIN>(work_item_value, lmem);
		}
		// TODO: sub-group algo fallback
		return reduce<work_group_size>(work_item_value, lmem, [](auto& lhs, auto& rhs) { return min(lhs, rhs); });
	}
	
	//! work-group max reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_max(const reduced_type& work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::MAX, reduced_type>) {
			return group::work_group_reduce<group::OP::MAX>(work_item_value, lmem);
		}
		// TODO: sub-group algo fallback
		return reduce<work_group_size>(work_item_value, lmem, [](auto& lhs, auto& rhs) { return max(lhs, rhs); });
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size>
	static constexpr uint32_t reduce_local_memory_elements() {
		// TODO: update requirements when there is sub-group support
		return work_group_size;
	}
	
	//////////////////////////////////////////
	// work-group scan functions

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
			// force barrier for consistency with other scan implementations + we can safely overwrite lmem
			local_barrier();
			return op(group_offset, scan_value);
#endif
		} else {
			// old-school scan
			auto value = work_item_value;
			lmem[lid] = value;
			local_barrier();
			
			uint32_t side_idx = 0u;
#pragma nounroll
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
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
			auto& arr = lmem.as_array();
#else
			auto& arr = lmem;
#endif
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

	//! work-group inclusive-scan-add/sum function (aka prefix sum)
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan_add(const data_type& work_item_value, lmem_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_INCLUSIVE_SCAN, group::OP::ADD, data_type>) {
			return group::work_group_inclusive_scan<group::OP::ADD>(work_item_value, lmem);
		}
		// TODO: sub-group algo fallback
		return scan<work_group_size, true>(work_item_value, plus<data_type> {}, lmem, data_type(0));
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
	
	//! work-group exclusive-scan-add/sum function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan_add(const data_type& work_item_value, lmem_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_EXCLUSIVE_SCAN, group::OP::ADD, data_type>) {
			return group::work_group_exclusive_scan<group::OP::ADD>(work_item_value, lmem);
		}
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, group::OP::ADD, data_type>) {
			// first pass: inclusive scan in each sub-group
			const auto sub_block_val = group::sub_group_inclusive_scan<group::OP::ADD>(work_item_value);
			// last sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == sub_group_size - 1u) {
				lmem[sub_group_id_1d] = sub_block_val;
			}
			local_barrier();
			
			// second pass: inclusive scan of the last values in each sub-group to compute the per-sub-group/block offset, executed in the first sub-group
			if (sub_group_id_1d == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				const auto sg_in_val = (sub_group_local_id < sub_group_size ? lmem[sub_group_local_id] : data_type(0));
				const auto wg_offset = group::sub_group_inclusive_scan<group::OP::ADD>(sg_in_val);
				lmem[sub_group_local_id] = wg_offset;
			}
			local_barrier();
			
			// finally: broadcast final per-sub-group/block offsets to all sub-groups
			const auto sub_block_offset = (sub_group_id_1d == 0u ? data_type(0) : lmem[sub_group_id_1d - 1u]);
			// shift one up, #0 in each group returns the offset of the previous group (+ 0)
			const auto excl_sub_block_val = simd_shuffle_up(sub_block_val, 1u);
			// force barrier for consistency with other scan implementations + we can safely overwrite lmem
			local_barrier();
			return sub_block_offset + (sub_group_local_id == 0u ? data_type(0) : excl_sub_block_val);
		}
#endif
		return scan<work_group_size, false>(work_item_value, plus<data_type> {}, lmem, data_type(0));
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size>
	static constexpr uint32_t scan_local_memory_elements() {
		// TODO: update requirements when there is sub-group algo support
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
		if constexpr (has_sub_group_scan()) {
			static_assert(device_info::simd_width() * device_info::simd_width() >= work_group_size,
						  "unexpected SIMD-width / max work-group size");
#if defined(FLOOR_COMPUTE_METAL) && defined(FLOOR_COMPUTE_INFO_VENDOR_AMD)
			// workaround an AMD Metal issue where we seem to have a weird alignment/allocation issue, where 32/64 is not enough (even if we align it to 16)
			// NOTE: this is the case for both SIMD32 and SIMD64
			return 128u;
#else
			return max(device_info::simd_width(), work_group_size / device_info::simd_width());
#endif
		}
#endif
		return work_group_size * 2u - 1u;
	}
	
}

#endif
