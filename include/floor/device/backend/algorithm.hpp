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

//! misc compute algorithms, specialized for each backend/hardware
namespace fl::algorithm {
	// TODO: broadcast, radix/bitonic sort, histogram

	//! enforce correct work-group size types
	template <auto work_group_size>
	concept supported_work_group_size_type = (std::is_same_v<decltype(work_group_size), uint32_t> ||
											  std::is_same_v<decltype(work_group_size), uint2> ||
											  std::is_same_v<decltype(work_group_size), uint3>);

	//! computes the linear/1D work group size from the specified 1D, 2D or 3D work group size
	template <auto work_group_size>
	requires(supported_work_group_size_type<work_group_size>)
	static inline constexpr uint32_t compute_linear_work_group_size() {
		if constexpr (std::is_same_v<decltype(work_group_size), uint32_t>) {
			return work_group_size;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint2>) {
			return work_group_size.x * work_group_size.y;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint3>) {
			return work_group_size.x * work_group_size.y * work_group_size.z;
		} else {
			instantiation_trap("invalid work_group_size type");
		}
	}

	//! NOTE: generally true except for Host-Compute (can be enabled there for testing purposes)
	static constexpr const bool prefer_simd_operations {
	#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
		true
	#else
		false
	#endif
	};
	
	//! returns the minimum/lowest value representable by "data_type" (used to init _max algorithms)
	template <typename data_type> requires (ext::is_arithmetic_v<data_type>)
	static inline constexpr data_type min_value() {
		return std::numeric_limits<decay_as_t<data_type>>::lowest();
	}
	
	//! returns the minimum/lowest value representable by "data_type" (used to init _max algorithms)
	template <typename data_type> requires (is_floor_vector_v<data_type>)
	static inline constexpr data_type min_value() {
		return decay_as_t<data_type> { std::numeric_limits<typename data_type::decayed_scalar_type>::lowest() };
	}
	
	//! returns the maximum value representable by "data_type" (used to init _min algorithms)
	template <typename data_type> requires (ext::is_arithmetic_v<data_type>)
	static inline constexpr data_type max_value() {
		return std::numeric_limits<decay_as_t<data_type>>::max();
	}
	
	//! returns the maximum value representable by "data_type" (used to init _min algorithms)
	template <typename data_type> requires (is_floor_vector_v<data_type>)
	static inline constexpr data_type max_value() {
		return decay_as_t<data_type> { std::numeric_limits<typename data_type::decayed_scalar_type>::max() };
	}
	
	using fl::algorithm::group::min_op;
	using fl::algorithm::group::max_op;
	
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
		if constexpr (std::is_same_v<decltype(work_group_size), uint32_t>) {
			lid = local_id.x;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint2>) {
			lid = local_id.x + local_id.y * local_size.x;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint3>) {
			lid = local_id.x + local_id.y * local_size.x + local_id.z * local_size.x * local_size.y;
		}
		
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
		auto value = lmem[lid];
		// butterfly reduce to [0]
#pragma unroll
		for (uint32_t i = linear_work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_DEVICE_INFO_VENDOR_APPLE)
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
#else // -> Host-Compute
		// make sure everyone has written to local memory
		local_barrier();
		// reduce in the first work-item only
		if(lid == 0) {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
			auto& arr = lmem.as_array();
#else
			auto& arr = lmem;
#endif
			for(uint32_t i = 1; i < linear_work_group_size; ++i) {
				arr[0] = op(arr[0], arr[i]);
			}
		}
		return data_type(lmem[0]);
#endif
	}
	
	//! generic work-group reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: the reduce function/op must be a binary function
	template <auto work_group_size, typename reduced_type, typename local_memory_type, typename F>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce(const reduced_type work_item_value,
												   local_memory_type& lmem,
												   F&& op) {
		// init/set all work-item values
		constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
		uint32_t lid = 0u;
		if constexpr (std::is_same_v<decltype(work_group_size), uint32_t>) {
			lid = local_id.x;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint2>) {
			lid = local_id.x + local_id.y * local_size.x;
		} else if constexpr (std::is_same_v<decltype(work_group_size), uint3>) {
			lid = local_id.x + local_id.y * local_size.x + local_id.z * local_size.x * local_size.y;
		}
		auto value = work_item_value;
		lmem[lid] = value;
		
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
		// butterfly reduce to [0]
#pragma unroll
		for(uint32_t i = linear_work_group_size / 2; i > 0; i >>= 1) {
			// sync local mem + work-item barrier
#if defined(FLOOR_DEVICE_INFO_VENDOR_APPLE)
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
#else // -> Host-Compute
		// make sure everyone has written to local memory
		local_barrier();
		// reduce in the first work-item only
		if(lid == 0) {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
			auto& arr = lmem.as_array();
#else
			auto& arr = lmem;
#endif
			for(uint32_t i = 1; i < linear_work_group_size; ++i) {
				arr[0] = reduced_type(op(arr[0], arr[i]));
			}
		}
		return reduced_type(lmem[0]);
#endif
	}
	
	//! work-group add/sum reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_add(const reduced_type work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::ADD, reduced_type>) {
			return group::work_group_reduce<group::OP::ADD>(work_item_value, lmem);
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::ADD, reduced_type> &&
						   prefer_simd_operations) {
			constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
			
			// first pass: inclusive scan in each sub-group
			const auto sub_block_red_val = group::sub_group_reduce<group::OP::ADD>(work_item_value);
			// first sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == 0u) {
				lmem[sub_group_id] = sub_block_red_val;
			}
			local_barrier();
			
			// second pass: reduction of the partial sums in each sub-group to compute the total sum, executed in the first sub-group
			reduced_type total_sum {};
			if (sub_group_id == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				const auto sg_in_val = (sub_group_local_id < (linear_work_group_size / sub_group_size) ? lmem[sub_group_local_id] : reduced_type(0));
				total_sum = group::sub_group_reduce<group::OP::ADD>(sg_in_val);
			}
			local_barrier();
			return total_sum;
		}
#endif
		return reduce<work_group_size>(work_item_value, lmem, std::plus<reduced_type> {});
	}
	
	//! work-group min reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_min(const reduced_type work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::MIN, reduced_type>) {
			return group::work_group_reduce<group::OP::MIN>(work_item_value, lmem);
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::MIN, reduced_type> &&
						   prefer_simd_operations) {
			constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
			
			// first pass: inclusive scan in each sub-group
			const auto sub_block_red_val = group::sub_group_reduce<group::OP::MIN>(work_item_value);
			// first sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == 0u) {
				lmem[sub_group_id] = sub_block_red_val;
			}
			local_barrier();
			
			// second pass: reduction of the partial minima in each sub-group to compute the total min, executed in the first sub-group
			reduced_type total_min {};
			if (sub_group_id == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				const auto sg_in_val = (sub_group_local_id < (linear_work_group_size / sub_group_size) ?
										reduced_type(lmem[sub_group_local_id]) : max_value<reduced_type>());
				total_min = group::sub_group_reduce<group::OP::MIN>(sg_in_val);
			}
			local_barrier();
			return total_min;
		}
#endif
		return reduce<work_group_size>(work_item_value, lmem, min_op<reduced_type> {});
	}
	
	//! work-group max reduce function
	//! NOTE: only work-item #0 (local_id == 0) is guaranteed to contain the final result
	//! NOTE: local memory must be allocated on the user side and passed into this function
	template <auto work_group_size, typename reduced_type, typename local_memory_type>
	requires(supported_work_group_size_type<work_group_size>)
	floor_inline_always static reduced_type reduce_max(const reduced_type work_item_value,
													   local_memory_type& lmem) {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, group::OP::MAX, reduced_type>) {
			return group::work_group_reduce<group::OP::MAX>(work_item_value, lmem);
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, group::OP::MAX, reduced_type> &&
						   prefer_simd_operations) {
			constexpr const uint32_t linear_work_group_size = compute_linear_work_group_size<work_group_size>();
			
			// first pass: inclusive scan in each sub-group
			const auto sub_block_red_val = group::sub_group_reduce<group::OP::MAX>(work_item_value);
			// first sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == 0u) {
				lmem[sub_group_id] = sub_block_red_val;
			}
			local_barrier();
			
			// second pass: reduction of the partial maxima in each sub-group to compute the total max, executed in the first sub-group
			reduced_type total_max {};
			if (sub_group_id == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				const auto sg_in_val = (sub_group_local_id < (linear_work_group_size / sub_group_size) ?
										reduced_type(lmem[sub_group_local_id]) : min_value<reduced_type>());
				total_max = group::sub_group_reduce<group::OP::MAX>(sg_in_val);
			}
			local_barrier();
			return total_max;
		}
#endif
		return reduce<work_group_size>(work_item_value, lmem, max_op<reduced_type> {});
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size, typename data_type, group::OP op = group::OP::NONE>
	static constexpr uint32_t reduce_local_memory_elements() {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_REDUCE, op, data_type>) {
			return group::required_local_memory_elements<group::ALGORITHM::WORK_GROUP_REDUCE, op, data_type>::count;
		} else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_REDUCE, op, data_type> &&
							 device_info::simd_width_min() > 1 && device_info::simd_width_max() >= device_info::simd_width_min() &&
							 prefer_simd_operations) {
			return work_group_size / device_info::simd_width_min();
		}
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
				prefer_simd_operations &&
				pot_simd);
	}
	
	//! generic work-group scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, bool inclusive, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto scan(const data_type work_item_value,
										 op_func_type&& op,
										 lmem_type& lmem,
										 const decay_as_t<data_type> init_val) {
		using dec_data_type = decay_as_t<data_type>;
		const auto lid = local_id.x;
		
		if constexpr (has_sub_group_scan() && device_info::simd_width() > 0u) {
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
			constexpr const auto simd_width = device_info::simd_width();
			constexpr const auto group_count = work_group_size / simd_width;
			static_assert((work_group_size % simd_width) == 0u, "work-group size must be a multiple of SIMD-width");
			const auto lane = sub_group_local_id;
			const auto group = sub_group_id;
			
			// scan in sub-group
			dec_data_type shfled_var;
			dec_data_type scan_value = work_item_value;
#pragma unroll
			for (uint32_t lane_idx = 1u; lane_idx != simd_width; lane_idx <<= 1u) {
				shfled_var = simd_shuffle_up(scan_value, lane_idx);
				if (lane >= lane_idx) {
					scan_value = dec_data_type(op(shfled_var, scan_value));
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
				dec_data_type group_scan_value {};
				if constexpr (group_count == simd_width) {
					group_scan_value = lmem[lane];
				} else {
					group_scan_value = (lane < group_count ? lmem[lane] : init_val);
				}
				
#pragma unroll
				for (uint32_t lane_idx = 1u; lane_idx != simd_width; lane_idx <<= 1u) {
					shfled_var = simd_shuffle_up(group_scan_value, lane_idx);
					if (lane >= lane_idx) {
						group_scan_value = dec_data_type(op(shfled_var, group_scan_value));
					}
				}
				
				if constexpr (group_count == simd_width) {
					lmem[lane] = group_scan_value;
				} else {
					if (lane < group_count) {
						lmem[lane] = group_scan_value;
					}
				}
			}
			local_barrier();
			
			// broadcast final per-sub-group values to all sub-groups
			const auto group_offset = (group > 0u ? lmem[group - 1u] : init_val);
			if constexpr (!inclusive) {
				// exclusive: shift one up, #0 in each group returns the offset of the previous group (+ 0)
				shfled_var = simd_shuffle_up(scan_value, 1u);
				scan_value = (lane == 0u ? init_val : shfled_var);
			}
			// force barrier for consistency with other scan implementations + we can safely overwrite lmem
			local_barrier();
			return dec_data_type(op(group_offset, scan_value));
#endif
		} else {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
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
			const auto ret = (lid == 0u ? init_val : lmem[side_idx + lid - 1u]);
			local_barrier();
			return dec_data_type(ret);
#else // -> Host-Compute
			lmem[lid] = work_item_value;
			local_barrier();
			
			if (lid == 0) {
				// just forward scan
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
				auto& arr = lmem.as_array();
#else
				auto& arr = lmem;
#endif
				for (uint32_t i = 1u; i < work_group_size; ++i) {
					arr[i] = dec_data_type(op(dec_data_type(arr[i - 1]), dec_data_type(arr[i])));
				}
			}
			
			// sync once so that lmem can safely be used again outside of this function
			// for exclusive: local id #0 always returns the init/zero val, all others the value from the previous local id
			const auto ret = (inclusive ? lmem[lid] : (lid == 0 ? init_val : lmem[lid - 1u]));
			local_barrier();
			return dec_data_type(ret);
#endif
		}
	}
	
	//! generic work-group inclusive-scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan(const data_type work_item_value,
												   op_func_type&& op,
												   lmem_type& lmem,
												   const decay_as_t<data_type> init_val) {
		return scan<work_group_size, true>(work_item_value, std::forward<op_func_type>(op), lmem, init_val);
	}

	//! work-group inclusive-scan-$op function (aka prefix sum/min/max)
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, group::OP op, typename data_type, typename lmem_type> requires (op != group::OP::NONE)
	floor_inline_always static auto inclusive_scan_op(const data_type work_item_value, lmem_type& lmem,
													  const decay_as_t<data_type> init_val) {
		using dec_data_type = decay_as_t<data_type>;
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_INCLUSIVE_SCAN, op, dec_data_type>) {
			return group::work_group_inclusive_scan<op>(work_item_value, lmem);
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, op, dec_data_type> &&
						   device_info::simd_width() > 0u && prefer_simd_operations) {
			constexpr const auto simd_width = device_info::simd_width();
			constexpr const auto group_count = work_group_size / simd_width;
			
			// first pass: inclusive scan in each sub-group
			const auto sub_block_val = group::sub_group_inclusive_scan<op>(work_item_value);
			// last sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == sub_group_size - 1u) {
				lmem[sub_group_id] = sub_block_val;
			}
			local_barrier();
			
			// second pass: inclusive scan of the last values in each sub-group to compute the per-sub-group/block offset, executed in the first sub-group
			if (sub_group_id == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				dec_data_type sg_in_val {};
				if constexpr (group_count == simd_width) {
					sg_in_val = dec_data_type(lmem[sub_group_local_id]);
				} else {
					sg_in_val = (sub_group_local_id < (work_group_size / sub_group_size) ? dec_data_type(lmem[sub_group_local_id]) : init_val);
				}
				const auto wg_offset = group::sub_group_inclusive_scan<op>(sg_in_val);
				if constexpr (group_count == simd_width) {
					lmem[sub_group_local_id] = wg_offset;
				} else {
					if (sub_group_local_id < (work_group_size / sub_group_size)) {
						lmem[sub_group_local_id] = wg_offset;
					}
				}
			}
			local_barrier();
			
			// finally: broadcast final per-sub-group/block values to all sub-groups
			const auto sub_block_offset = dec_data_type(sub_group_id == 0u ? init_val : lmem[sub_group_id - 1u]);
			// force barrier for consistency with other scan implementations + we can safely overwrite lmem
			local_barrier();
			if constexpr (op == group::OP::ADD) {
				return dec_data_type(sub_block_offset + sub_block_val);
			} else if constexpr (op == group::OP::MIN) {
				return dec_data_type(min_op<dec_data_type>()(sub_block_offset, sub_block_val));
			} else if constexpr (op == group::OP::MAX) {
				return dec_data_type(max_op<dec_data_type>()(sub_block_offset, sub_block_val));
			} else {
				instantiation_trap_dependent_type(dec_data_type, "unhandled op");
			}
		}
#endif
		else {
			if constexpr (op == group::OP::ADD) {
				return scan<work_group_size, true>(work_item_value, std::plus<dec_data_type> {}, lmem, init_val);
			} else if constexpr (op == group::OP::MIN) {
				return scan<work_group_size, true>(work_item_value, min_op<dec_data_type> {}, lmem, init_val);
			} else if constexpr (op == group::OP::MAX) {
				return scan<work_group_size, true>(work_item_value, max_op<dec_data_type> {}, lmem, init_val);
			} else {
				instantiation_trap_dependent_type(dec_data_type, "unhandled op");
			}
		}
	}

	//! work-group inclusive-scan-add/sum function (aka prefix sum)
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan_add(const data_type work_item_value, lmem_type& lmem) {
		return inclusive_scan_op<work_group_size, group::OP::ADD>(work_item_value, lmem, decay_as_t<data_type> {});
	}

	//! work-group inclusive-scan-min function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan_min(const data_type work_item_value, lmem_type& lmem) {
		return inclusive_scan_op<work_group_size, group::OP::MIN>(work_item_value, lmem, max_value<data_type>());
	}

	//! work-group inclusive-scan-max function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto inclusive_scan_max(const data_type work_item_value, lmem_type& lmem) {
		return inclusive_scan_op<work_group_size, group::OP::MAX>(work_item_value, lmem, min_value<data_type>());
	}
	
	//! generic work-group exclusive-scan function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	//! NOTE: the reduce function/op must be a binary function
	template <uint32_t work_group_size, typename data_type, typename op_func_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan(const data_type work_item_value,
												   op_func_type&& op,
												   lmem_type& lmem,
												   const decay_as_t<data_type> init_val) {
		return scan<work_group_size, false>(work_item_value, std::forward<op_func_type>(op), lmem, init_val);
	}
	
	//! work-group exclusive-scan-$op function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, group::OP op, typename data_type, typename lmem_type> requires (op != group::OP::NONE)
	floor_inline_always static auto exclusive_scan_op(const data_type work_item_value, lmem_type& lmem,
													  const decay_as_t<data_type> init_val) {
		using dec_data_type = decay_as_t<data_type>;
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_EXCLUSIVE_SCAN, op, dec_data_type>) {
			return group::work_group_exclusive_scan<op>(work_item_value, lmem);
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		// can we fallback to a sub-group level implementation?
		else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, op, dec_data_type> &&
						   device_info::simd_width() > 0u && prefer_simd_operations) {
			constexpr const auto simd_width = device_info::simd_width();
			constexpr const auto group_count = work_group_size / simd_width;
			
			// first pass: inclusive scan in each sub-group
			const auto sub_block_val = group::sub_group_inclusive_scan<op>(work_item_value);
			// last sub-group item writes its result into local memory for the second pass
			if (sub_group_local_id == sub_group_size - 1u) {
				lmem[sub_group_id] = sub_block_val;
			}
			local_barrier();
			
			// second pass: inclusive scan of the last values in each sub-group to compute the per-sub-group/block offset, executed in the first sub-group
			if (sub_group_id == 0u) {
				// NOTE: we need to consider that the executing work-group size may be smaller than "sub_group_size * sub_group_size"
				dec_data_type sg_in_val {};
				if constexpr (group_count == simd_width) {
					sg_in_val = dec_data_type(lmem[sub_group_local_id]);
				} else {
					sg_in_val = (sub_group_local_id < (work_group_size / sub_group_size) ? dec_data_type(lmem[sub_group_local_id]) : init_val);
				}
				const auto wg_offset = group::sub_group_inclusive_scan<op>(sg_in_val);
				if constexpr (group_count == simd_width) {
					lmem[sub_group_local_id] = wg_offset;
				} else {
					if (sub_group_local_id < (work_group_size / sub_group_size)) {
						lmem[sub_group_local_id] = wg_offset;
					}
				}
			}
			local_barrier();
			
			// finally: broadcast final per-sub-group/block values to all sub-groups
			const auto sub_block_offset = dec_data_type(sub_group_id == 0u ? init_val : lmem[sub_group_id - 1u]);
			// shift one up, #0 in each group returns the value of the previous group (+ 0)
			const auto excl_sub_block_val = simd_shuffle_up(sub_block_val, 1u);
			// force barrier for consistency with other scan implementations + we can safely overwrite lmem
			local_barrier();
			if constexpr (op == group::OP::ADD) {
				return dec_data_type(sub_block_offset + (sub_group_local_id == 0u ? init_val : excl_sub_block_val));
			} else if constexpr (op == group::OP::MIN) {
				return dec_data_type(min_op<dec_data_type>()(sub_block_offset, (sub_group_local_id == 0u ? init_val : excl_sub_block_val)));
			} else if constexpr (op == group::OP::MAX) {
				return dec_data_type(max_op<dec_data_type>()(sub_block_offset, (sub_group_local_id == 0u ? init_val : excl_sub_block_val)));
			} else {
				instantiation_trap_dependent_type(dec_data_type, "unhandled op");
			}
		}
#endif
		else {
			if constexpr (op == group::OP::ADD) {
				return scan<work_group_size, false>(work_item_value, std::plus<dec_data_type> {}, lmem, init_val);
			} else if constexpr (op == group::OP::MIN) {
				return scan<work_group_size, false>(work_item_value, min_op<dec_data_type> {}, lmem, init_val);
			} else if constexpr (op == group::OP::MAX) {
				return scan<work_group_size, false>(work_item_value, max_op<dec_data_type> {}, lmem, init_val);
			} else {
				instantiation_trap_dependent_type(dec_data_type, "unhandled op");
			}
		}
	}
	
	//! work-group exclusive-scan-add/sum function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan_add(const data_type work_item_value, lmem_type& lmem) {
		return exclusive_scan_op<work_group_size, group::OP::ADD>(work_item_value, lmem, decay_as_t<data_type> {});
	}
	
	//! work-group exclusive-scan-min function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan_min(const data_type work_item_value, lmem_type& lmem) {
		return exclusive_scan_op<work_group_size, group::OP::MIN>(work_item_value, lmem, max_value<data_type>());
	}
	
	//! work-group exclusive-scan-max function
	//! NOTE: local memory must be allocated on the user side and passed into this function
	//! NOTE: this function can only be called for 1D kernels
	template <uint32_t work_group_size, typename data_type, typename lmem_type>
	floor_inline_always static auto exclusive_scan_max(const data_type work_item_value, lmem_type& lmem) {
		return exclusive_scan_op<work_group_size, group::OP::MAX>(work_item_value, lmem, min_value<data_type>());
	}
	
	//! returns the amount of local memory elements that must be allocated by the caller
	template <uint32_t work_group_size, typename data_type, group::OP op = group::OP::NONE>
	static constexpr uint32_t scan_local_memory_elements() {
		if constexpr (group::supports_v<group::ALGORITHM::WORK_GROUP_INCLUSIVE_SCAN, op, data_type>) {
			return group::required_local_memory_elements<group::ALGORITHM::WORK_GROUP_INCLUSIVE_SCAN, op, data_type>::count;
		} else if constexpr (group::supports_v<group::ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, op, uint32_t> &&
							 device_info::simd_width_min() > 1 && device_info::simd_width_max() >= device_info::simd_width_min() &&
							 prefer_simd_operations) {
			return work_group_size / device_info::simd_width_min();
		}
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS != 0
		else if constexpr (has_sub_group_scan() && device_info::simd_width() > 0u &&
						   device_info::vendor() != device_info::VENDOR::HOST /* Host-Compute scan uses oldschool scan */) {
			static_assert(device_info::simd_width() * device_info::simd_width() >= work_group_size,
						  "unexpected SIMD-width / max work-group size");
#if defined(FLOOR_DEVICE_METAL) && defined(FLOOR_DEVICE_INFO_VENDOR_AMD)
			// workaround an AMD Metal issue where we seem to have a weird alignment/allocation issue, where 32/64 is not enough (even if we align it to 16)
			// NOTE: this is the case for both SIMD32 and SIMD64
			return 128u;
#else
			return const_math::max(device_info::simd_width(), work_group_size / device_info::simd_width());
#endif
		}
#endif
		return work_group_size * 2u;
	}
	
} // namespace fl::algorithm
