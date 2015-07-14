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
uint32_t get_global_id(uint32_t dim);
uint32_t get_global_size(uint32_t dim);
uint32_t get_local_id(uint32_t dim);
uint32_t get_local_size(uint32_t dim);
uint32_t get_group_id(uint32_t dim);
uint32_t get_num_groups(uint32_t dim);
uint32_t get_work_dim();

// math functions
#include <cmath>
namespace std {
	static floor_inline_always float rsqrt(float x) { return 1.0f / sqrt(x); }
	static floor_inline_always double rsqrt(double x) { return 1.0 / sqrt(x); }
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

#endif

#endif
