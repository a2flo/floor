/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

// id handling (NOTE: implemented in host_kernel.cpp)
size_t get_global_id(uint32_t dimindx);
size_t get_global_size(uint32_t dimindx);
size_t get_local_id(uint32_t dimindx);
size_t get_local_size(uint32_t dimindx);
size_t get_group_id(uint32_t dimindx);
size_t get_num_groups(uint32_t dimindx);
uint32_t get_work_dim();

#define global_id size3 { get_global_id(0), get_global_id(1), get_global_id(2) }
#define global_size size3 { get_global_size(0), get_global_size(1), get_global_size(2) }
#define local_id size3 { get_local_id(0), get_local_id(1), get_local_id(2) }
#define local_size size3 { get_local_size(0), get_local_size(1), get_local_size(2) }
#define group_id size3 { get_group_id(0), get_group_id(1), get_group_id(2) }
#define group_size size3 { get_num_groups(0), get_num_groups(1), get_num_groups(2) }

// math functions
#include <cmath>
namespace std {
	static floor_inline_always float rsqrt(float x) { return 1.0f / sqrt(x); }
	static floor_inline_always double rsqrt(double x) { return 1.0 / sqrt(x); }
}

// printf
#include <cstdio>

// barrier and mem_fence functionality
static floor_inline_always void global_barrier() {
	// TODO: !
}
static floor_inline_always void global_mem_fence() {
	// TODO: !
}
static floor_inline_always void global_read_mem_fence() {
	// TODO: !
}
static floor_inline_always void global_write_mem_fence() {
	// TODO: !
}
static floor_inline_always void local_barrier() {
	// TODO: !
}
static floor_inline_always void local_mem_fence() {
	// TODO: !
}
static floor_inline_always void local_read_mem_fence() {
	// TODO: !
}
static floor_inline_always void local_write_mem_fence() {
	// TODO: !
}

#endif

#endif
