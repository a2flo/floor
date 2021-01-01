/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_ID_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_ID_HPP__

#if defined(FLOOR_COMPUTE_HOST)

////////////////////////////////////////////////////////////////////////////////////////////////////
// host-compute
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
// this is used to compute the offset into local memory depending on the worker thread id.
// this must be declared extern, so that it is properly visible to host compute code, so that
// no "opaque" function has to called, which would be detrimental to vectorization.
#if !defined(__WINDOWS__)
extern thread_local uint32_t floor_thread_idx;
extern thread_local uint32_t floor_thread_local_memory_offset;
#else // Windows workarounds for dllexport of TLS vars
FLOOR_DLL_API inline auto& floor_thread_idx_get() {
	static thread_local uint32_t floor_thread_idx_tls;
	return floor_thread_idx_tls;
}
FLOOR_DLL_API inline auto& floor_thread_local_memory_offset_get() {
	static thread_local uint32_t floor_thread_local_memory_offset_tls;
	return floor_thread_local_memory_offset_tls;
}
#define floor_thread_idx floor_thread_idx_get()
#define floor_thread_local_memory_offset floor_thread_local_memory_offset_get()
#endif

// id handling vars, as above, this is externally visible to aid vectorization
extern FLOOR_DLL_API uint32_t floor_work_dim;
extern FLOOR_DLL_API uint3 floor_global_work_size;
extern FLOOR_DLL_API uint3 floor_local_work_size;
extern FLOOR_DLL_API uint3 floor_group_size;

#if !defined(__WINDOWS__)
extern thread_local uint3 floor_global_idx;
extern thread_local uint3 floor_local_idx;
extern thread_local uint3 floor_group_idx;
#else // Windows workarounds for dllexport of TLS vars
FLOOR_DLL_API inline auto& floor_global_idx_get() {
	static thread_local uint3 floor_global_idx_tls;
	return floor_global_idx_tls;
}
FLOOR_DLL_API inline auto& floor_local_idx_get() {
	static thread_local uint3 floor_local_idx_tls;
	return floor_local_idx_tls;
}
FLOOR_DLL_API inline auto& floor_group_idx_get() {
	static thread_local uint3 floor_group_idx_tls;
	return floor_group_idx_tls;
}
#define floor_global_idx floor_global_idx_get()
#define floor_local_idx floor_local_idx_get()
#define floor_group_idx floor_group_idx_get()
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// host-compute device
#else

// for host-compute device execution, each execution thread has its own memory space (initializes the binary + memory separately),
// which allows use to avoid TLS (-> faster, better code gen) and simply put all IDs/size symbols in per-execution thread memory
extern uint3 floor_global_idx;
extern uint3 floor_global_work_size;
extern uint3 floor_local_idx;
extern uint3 floor_local_work_size;
extern uint3 floor_group_idx;
extern uint3 floor_group_size;
extern uint32_t floor_work_dim;
// we don't need to handle an offset per thread, this is always 0
static constexpr const uint32_t floor_thread_local_memory_offset { 0u };

#endif // !FLOOR_COMPUTE_HOST_DEVICE

#endif // FLOOR_COMPUTE_HOST

#endif
