/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

//! interface and support/query helpers for parallel group operations
//! NOTE: the main intent of this is for backends to be able to provide specialized/optimized work-group/sub-group operations
//!       that will be used instead of the generic work-group/sub-group algorithms in compute_algorithm.hpp
//!       -> that means that these operations *may* be implemented by backends, not that they have to (rather use compute_algorithm)
namespace compute_algorithm::group {

//! group algorithm
enum class ALGORITHM {
	WORK_GROUP_REDUCE,
	WORK_GROUP_INCLUSIVE_SCAN,
	WORK_GROUP_EXCLUSIVE_SCAN,
	
	SUB_GROUP_REDUCE,
	SUB_GROUP_INCLUSIVE_SCAN,
	SUB_GROUP_EXCLUSIVE_SCAN,
};

//! group operation
enum class OP {
	NONE,
	ADD,
	MIN,
	MAX,
};

//! if the combination of algorithm, operation and data_type is supported, this inherits from true_type, otherwise false_type
template <ALGORITHM algo, OP op, typename data_type> struct supports : public std::false_type {};

//! if the combination of algorithm, operation and data_type is supported, this is true, otherwise false
template <ALGORITHM algo, OP op, typename data_type> constexpr bool supports_v = supports<algo, op, data_type>::value;

//! specifies the amount of required local memory that is needed for the specified combination of algorithm, operation and data_type
//! NOTE: sub-group level algorithms may not use any local memory
template <ALGORITHM algo, OP op, typename data_type>
requires(algo != ALGORITHM::SUB_GROUP_REDUCE &&
		 algo != ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN &&
		 algo != ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN)
struct required_local_memory_elements {
	static constexpr const size_t count = 0;
};

//! stub for reduce with a specific operation and data_type
template <OP op, typename data_type, typename lmem_type>
[[maybe_unused]] static auto work_group_reduce(const data_type& input_value, lmem_type& lmem);

//! stub for inclusive scan with a specific operation and data_type
template <OP op, typename data_type, typename lmem_type>
[[maybe_unused]] static auto work_group_inclusive_scan(const data_type& input_value, lmem_type& lmem);

//! stub for exclusive scan with a specific operation and data_type
template <OP op, typename data_type, typename lmem_type>
[[maybe_unused]] static auto work_group_exclusive_scan(const data_type& input_value, lmem_type& lmem);

//! stub for reduce with a specific operation and data_type
template <OP op, typename data_type>
[[maybe_unused]] static auto sub_group_reduce(const data_type& input_value);

//! stub for inclusive scan with a specific operation and data_type
template <OP op, typename data_type>
[[maybe_unused]] static auto sub_group_inclusive_scan(const data_type& input_value);

//! stub for exclusive scan with a specific operation and data_type
template <OP op, typename data_type>
[[maybe_unused]] static auto sub_group_exclusive_scan(const data_type& input_value);

} // compute_algorithm::group
