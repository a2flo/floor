/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#if defined(FLOOR_COMPUTE_CUDA)

// make this a little easier to use
#define FLOOR_CUDA_DIM0 __attribute__((always_inline, flatten, pure, enable_if(dim == 0, "const 0")))
#define FLOOR_CUDA_DIM1 __attribute__((always_inline, flatten, pure, enable_if(dim == 1, "const 1")))
#define FLOOR_CUDA_DIM2 __attribute__((always_inline, flatten, pure, enable_if(dim == 2, "const 2")))
#define FLOOR_CUDA_INVALID __attribute__((always_inline, flatten, pure, enable_if(dim > 2, "invalid dim"), unavailable("invalid dim")))
#define FLOOR_CUDA_DIM_RT __attribute__((always_inline, flatten, pure))

#define global_id uint3 { \
	__nvvm_read_ptx_sreg_ctaid_x() * __nvvm_read_ptx_sreg_ntid_x() + __nvvm_read_ptx_sreg_tid_x(), \
	__nvvm_read_ptx_sreg_ctaid_y() * __nvvm_read_ptx_sreg_ntid_y() + __nvvm_read_ptx_sreg_tid_y(), \
	__nvvm_read_ptx_sreg_ctaid_z() * __nvvm_read_ptx_sreg_ntid_z() + __nvvm_read_ptx_sreg_tid_z() \
}
#define global_size uint3 { \
	__nvvm_read_ptx_sreg_nctaid_x() * __nvvm_read_ptx_sreg_ntid_x(), \
	__nvvm_read_ptx_sreg_nctaid_y() * __nvvm_read_ptx_sreg_ntid_y(), \
	__nvvm_read_ptx_sreg_nctaid_z() * __nvvm_read_ptx_sreg_ntid_z() \
}
#define local_id uint3 { \
	__nvvm_read_ptx_sreg_tid_x(), \
	__nvvm_read_ptx_sreg_tid_y(), \
	__nvvm_read_ptx_sreg_tid_z() \
}
#define local_size uint3 { \
	__nvvm_read_ptx_sreg_ntid_x(), \
	__nvvm_read_ptx_sreg_ntid_y(), \
	__nvvm_read_ptx_sreg_ntid_z() \
}
#define group_id uint3 { \
	__nvvm_read_ptx_sreg_ctaid_x(), \
	__nvvm_read_ptx_sreg_ctaid_y(), \
	__nvvm_read_ptx_sreg_ctaid_z() \
}
#define group_size uint3 { \
	__nvvm_read_ptx_sreg_nctaid_x(), \
	__nvvm_read_ptx_sreg_nctaid_y(), \
	__nvvm_read_ptx_sreg_nctaid_z() \
}
#define sub_group_id ((uint32_t(__nvvm_read_ptx_sreg_tid_x()) + \
					   uint32_t(__nvvm_read_ptx_sreg_tid_y()) * uint32_t(__nvvm_read_ptx_sreg_nctaid_x()) + \
					   uint32_t(__nvvm_read_ptx_sreg_tid_z()) * uint32_t(__nvvm_read_ptx_sreg_nctaid_y()) * uint32_t(__nvvm_read_ptx_sreg_nctaid_x())) \
					  / FLOOR_COMPUTE_INFO_SIMD_WIDTH)
// faster alternatives to sub_group_id if kernel execution dim is known
#define sub_group_id_1d (uint32_t(__nvvm_read_ptx_sreg_tid_x()) / FLOOR_COMPUTE_INFO_SIMD_WIDTH)
#define sub_group_id_2d ((uint32_t(__nvvm_read_ptx_sreg_tid_x()) + uint32_t(__nvvm_read_ptx_sreg_tid_y()) * uint32_t(__nvvm_read_ptx_sreg_nctaid_x())) \
						 / FLOOR_COMPUTE_INFO_SIMD_WIDTH)
#define sub_group_id_3d sub_group_id
#define sub_group_local_id __nvvm_read_ptx_sreg_laneid()
#define sub_group_size FLOOR_COMPUTE_INFO_SIMD_WIDTH
#define sub_group_count __nvvm_read_ptx_sreg_nwarpid()

FLOOR_GLOBAL_ID_RANGE_ATTR const_func static uint32_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_ctaid_x() * __nvvm_read_ptx_sreg_ntid_x() + __nvvm_read_ptx_sreg_tid_x();
}
FLOOR_GLOBAL_ID_RANGE_ATTR const_func static uint32_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_ctaid_y() * __nvvm_read_ptx_sreg_ntid_y() + __nvvm_read_ptx_sreg_tid_y();
}
FLOOR_GLOBAL_ID_RANGE_ATTR const_func static uint32_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_ctaid_z() * __nvvm_read_ptx_sreg_ntid_z() + __nvvm_read_ptx_sreg_tid_z();
}
FLOOR_GLOBAL_ID_RANGE_ATTR static uint32_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_GLOBAL_ID_RANGE_ATTR const_func static uint32_t get_global_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_ctaid_x() * __nvvm_read_ptx_sreg_ntid_x() + __nvvm_read_ptx_sreg_tid_x();
		case 1: return __nvvm_read_ptx_sreg_ctaid_y() * __nvvm_read_ptx_sreg_ntid_y() + __nvvm_read_ptx_sreg_tid_y();
		case 2: return __nvvm_read_ptx_sreg_ctaid_z() * __nvvm_read_ptx_sreg_ntid_z() + __nvvm_read_ptx_sreg_tid_z();
		default: return 0;
	}
}
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func static uint32_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_nctaid_x() * __nvvm_read_ptx_sreg_ntid_x();
}
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func static uint32_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_nctaid_y() * __nvvm_read_ptx_sreg_ntid_y();
}
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func static uint32_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_nctaid_z() * __nvvm_read_ptx_sreg_ntid_z();
}
FLOOR_GLOBAL_SIZE_RANGE_ATTR static uint32_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func static uint32_t get_global_size(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_nctaid_x() * __nvvm_read_ptx_sreg_ntid_x();
		case 1: return __nvvm_read_ptx_sreg_nctaid_y() * __nvvm_read_ptx_sreg_ntid_y();
		case 2: return __nvvm_read_ptx_sreg_nctaid_z() * __nvvm_read_ptx_sreg_ntid_z();
		default: return 1;
	}
}

FLOOR_LOCAL_ID_RANGE_ATTR const_func static uint32_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_tid_x();
}
FLOOR_LOCAL_ID_RANGE_ATTR const_func static uint32_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_tid_y();
}
FLOOR_LOCAL_ID_RANGE_ATTR const_func static uint32_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_tid_z();
}
FLOOR_LOCAL_ID_RANGE_ATTR static uint32_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_LOCAL_ID_RANGE_ATTR const_func static uint32_t get_local_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_tid_x();
		case 1: return __nvvm_read_ptx_sreg_tid_y();
		case 2: return __nvvm_read_ptx_sreg_tid_z();
		default: return 0;
	}
}

FLOOR_LOCAL_SIZE_RANGE_ATTR const_func static uint32_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_ntid_x();
}
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func static uint32_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_ntid_y();
}
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func static uint32_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_ntid_z();
}
FLOOR_LOCAL_SIZE_RANGE_ATTR static uint32_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func static uint32_t get_local_size(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_ntid_x();
		case 1: return __nvvm_read_ptx_sreg_ntid_y();
		case 2: return __nvvm_read_ptx_sreg_ntid_z();
		default: return 1;
	}
}

FLOOR_GROUP_ID_RANGE_ATTR const_func static uint32_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_ctaid_x();
}
FLOOR_GROUP_ID_RANGE_ATTR const_func static uint32_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_ctaid_y();
}
FLOOR_GROUP_ID_RANGE_ATTR const_func static uint32_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_ctaid_z();
}
FLOOR_GROUP_ID_RANGE_ATTR static uint32_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_GROUP_ID_RANGE_ATTR const_func static uint32_t get_group_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_ctaid_x();
		case 1: return __nvvm_read_ptx_sreg_ctaid_y();
		case 2: return __nvvm_read_ptx_sreg_ctaid_z();
		default: return 0;
	}
}

FLOOR_GROUP_SIZE_RANGE_ATTR const_func static uint32_t get_group_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __nvvm_read_ptx_sreg_nctaid_x();
}
FLOOR_GROUP_SIZE_RANGE_ATTR const_func static uint32_t get_group_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __nvvm_read_ptx_sreg_nctaid_y();
}
FLOOR_GROUP_SIZE_RANGE_ATTR const_func static uint32_t get_group_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __nvvm_read_ptx_sreg_nctaid_z();
}
FLOOR_GROUP_SIZE_RANGE_ATTR static uint32_t get_group_size(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
FLOOR_GROUP_SIZE_RANGE_ATTR const_func static uint32_t get_group_size(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __nvvm_read_ptx_sreg_nctaid_x();
		case 1: return __nvvm_read_ptx_sreg_nctaid_y();
		case 2: return __nvvm_read_ptx_sreg_nctaid_z();
		default: return 1;
	}
}

[[range(1u, 3u)]] const_func static uint32_t get_work_dim() {
	// grid dim (X, Y, Z)
	// -> if Z is 1, must either be 1D or 2D
	if(__nvvm_read_ptx_sreg_nctaid_z() == 1) {
		// -> if Y is 1 as well, it is 1D, else 2D
		return (__nvvm_read_ptx_sreg_nctaid_y() == 1 ? 1 : 2);
	}
	// else: -> Z is not 1, must always be 3D
	return 3;
}

const_func static uint32_t get_sub_group_id() {
	// NOTE: %warpid should not be used as per ptx isa spec
	return sub_group_id;
}

const_func static uint32_t get_sub_group_local_id() {
	return __nvvm_read_ptx_sreg_laneid();
}

const_func constexpr static uint32_t get_sub_group_size() {
	return FLOOR_COMPUTE_INFO_SIMD_WIDTH;
}

const_func static uint32_t get_num_sub_groups() {
	return __nvvm_read_ptx_sreg_nwarpid();
}

#endif
