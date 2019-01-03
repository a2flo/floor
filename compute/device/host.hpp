/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_HPP__

#if defined(FLOOR_COMPUTE_HOST)

// already need this here
#include <floor/math/vector_lib.hpp>

// id handling
extern uint32_t floor_work_dim;
extern uint3 floor_global_work_size;
extern uint3 floor_local_work_size;
extern uint3 floor_group_size;
extern _Thread_local uint3 floor_global_idx;
extern _Thread_local uint3 floor_local_idx;
extern _Thread_local uint3 floor_group_idx;

floor_inline_always __attribute__((const)) static uint32_t get_global_id(uint32_t dim) {
	if(dim >= floor_work_dim) return 0;
	return floor_global_idx[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_global_size(uint32_t dim) {
	if(dim >= floor_work_dim) return 1;
	return floor_global_work_size[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_local_id(uint32_t dim) {
	if(dim >= floor_work_dim) return 0;
	return floor_local_idx[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_local_size(uint32_t dim) {
	if(dim >= floor_work_dim) return 1;
	return floor_local_work_size[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_group_id(uint32_t dim) {
	if(dim >= floor_work_dim) return 0;
	return floor_group_idx[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_group_size(uint32_t dim) {
	if(dim >= floor_work_dim) return 1;
	return floor_group_size[dim];
}
floor_inline_always __attribute__((const)) static uint32_t get_work_dim() {
	return floor_work_dim;
}

// math functions
#include <cmath>
namespace std {
	floor_inline_always __attribute__((const)) static float rsqrt(float x) { return 1.0f / sqrt(x); }
	floor_inline_always __attribute__((const)) static double rsqrt(double x) { return 1.0 / sqrt(x); }
}

// printf
#include <cstdio>

// barrier and mem_fence functionality (NOTE: implemented in host_kernel.cpp)
void global_barrier();
void global_mem_fence();
void global_read_mem_fence();
void global_write_mem_fence();
void local_barrier();
void local_mem_fence();
void local_read_mem_fence();
void local_write_mem_fence();
void barrier();

void image_barrier();
void image_mem_fence();
void image_read_mem_fence();
void image_write_mem_fence();

// local memory management (NOTE: implemented in host_kernel.cpp)
uint8_t* __attribute__((aligned(1024))) floor_requisition_local_memory(const size_t size, uint32_t& offset);

#endif

#endif
