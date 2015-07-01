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

#include <floor/compute/device/host.hpp>

// TODO: locking! (or in host_program -> execute ?)

static uint32_t floor_work_dim { 1u };
static size3 floor_global_work_size;
static size3 floor_local_work_size;
static size3 floor_group_size;
static size3 floor_global_idx;
static size3 floor_local_idx;
static size3 floor_group_idx;

size_t get_global_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_global_idx[dimindx];
}
size_t get_global_size(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_global_work_size[dimindx];
}
size_t get_local_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_local_idx[dimindx];
}
size_t get_local_size(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_local_work_size[dimindx];
}
size_t get_group_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_group_idx[dimindx];
}
size_t get_num_groups(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_group_size[dimindx];
}
uint32_t get_work_dim() {
	return floor_work_dim;
}

void floor_setup_host_exec(const uint32_t& dim,
						   const size3& global_work_size,
						   const size3& local_work_size) {
	floor_work_dim = dim;
	floor_global_work_size = global_work_size;
	floor_local_work_size = local_work_size;
	
	const auto mod_groups = floor_global_work_size % floor_local_work_size;
	floor_group_size = floor_global_work_size / floor_local_work_size;
	if(mod_groups.x > 0) ++floor_group_size.x;
	if(mod_groups.y > 0) ++floor_group_size.y;
	if(mod_groups.z > 0) ++floor_group_size.z;
	
	floor_global_idx = { 0, 0, 0 };
	floor_local_idx = { 0, 0, 0 };
	floor_group_idx = { 0, 0, 0 };
}
