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

#if defined(FLOOR_DEVICE_HOST_COMPUTE)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Host-Compute
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)

// dynamic vars/functions
extern uint32_t floor_host_compute_thread_local_memory_offset_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_global_idx_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_local_idx_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_group_idx_get() FLOOR_HOST_COMPUTE_CC;
extern uint32_t floor_host_compute_sub_group_id_get() FLOOR_HOST_COMPUTE_CC;
extern uint32_t floor_host_compute_sub_group_local_id_get() FLOOR_HOST_COMPUTE_CC;

#define floor_thread_local_memory_offset floor_host_compute_thread_local_memory_offset_get()
#define floor_global_idx floor_host_compute_global_idx_get()
#define floor_local_idx floor_host_compute_local_idx_get()
#define floor_group_idx floor_host_compute_group_idx_get()
#define floor_sub_group_id floor_host_compute_sub_group_id_get()
#define floor_sub_group_local_id floor_host_compute_sub_group_local_id_get()

// globally constant vars/functions
extern uint32_t floor_host_compute_work_dim_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_global_work_size_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_local_work_size_get() FLOOR_HOST_COMPUTE_CC;
extern fl::uint3 floor_host_compute_group_size_get() FLOOR_HOST_COMPUTE_CC;
extern uint32_t floor_host_compute_sub_group_size_get() FLOOR_HOST_COMPUTE_CC;
extern uint32_t floor_host_compute_num_sub_groups_get() FLOOR_HOST_COMPUTE_CC;
#define floor_work_dim floor_host_compute_work_dim_get()
#define floor_global_work_size floor_host_compute_global_work_size_get()
#define floor_local_work_size floor_host_compute_local_work_size_get()
#define floor_group_size floor_host_compute_group_size_get()
#define floor_sub_group_size floor_host_compute_sub_group_size_get()
#define floor_num_sub_groups floor_host_compute_num_sub_groups_get()

////////////////////////////////////////////////////////////////////////////////////////////////////
// Host-Compute device
#else

// for Host-Compute device execution, each execution thread has its own memory space (initializes the binary + memory separately),
// which allows us to avoid TLS (-> faster, better code gen) and simply put all IDs/size symbols in per-execution thread memory
extern fl::uint3 floor_global_idx;
extern fl::uint3 floor_global_work_size;
extern fl::uint3 floor_local_idx;
extern fl::uint3 floor_local_work_size;
extern fl::uint3 floor_group_idx;
extern fl::uint3 floor_group_size;
extern uint32_t floor_work_dim;
extern uint32_t floor_sub_group_id;
extern uint32_t floor_sub_group_local_id;
extern uint32_t floor_sub_group_size;
extern uint32_t floor_num_sub_groups;
// we don't need to handle an offset per thread, this is always 0
static constexpr const uint32_t floor_thread_local_memory_offset { 0u };

#endif // !FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE

#endif // FLOOR_DEVICE_HOST_COMPUTE
