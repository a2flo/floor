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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_ID_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_ID_HPP__

#if defined(FLOOR_COMPUTE_HOST)

// this is used to compute the offset into local memory depending on the worker thread id.
// this must be declared extern, so that it is properly visible to host compute code, so that
// no "opaque" function has to called, which would be detrimental to vectorization.
#if !defined(__WINDOWS__)
extern _Thread_local uint32_t floor_thread_idx;
extern _Thread_local uint32_t floor_thread_local_memory_offset;
#else // Windows workarounds for dllexport of TLS vars
FLOOR_DLL_API inline auto& floor_thread_idx_get() {
	static _Thread_local uint32_t floor_thread_idx_tls;
	return floor_thread_idx_tls;
}
FLOOR_DLL_API inline auto& floor_thread_local_memory_offset_get() {
	static _Thread_local uint32_t floor_thread_local_memory_offset_tls;
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
extern _Thread_local uint3 floor_global_idx;
extern _Thread_local uint3 floor_local_idx;
extern _Thread_local uint3 floor_group_idx;
#else // Windows workarounds for dllexport of TLS vars
FLOOR_DLL_API inline auto& floor_global_idx_get() {
	static _Thread_local uint3 floor_global_idx_tls;
	return floor_global_idx_tls;
}
FLOOR_DLL_API inline auto& floor_local_idx_get() {
	static _Thread_local uint3 floor_local_idx_tls;
	return floor_local_idx_tls;
}
FLOOR_DLL_API inline auto& floor_group_idx_get() {
	static _Thread_local uint3 floor_group_idx_tls;
	return floor_group_idx_tls;
}
#define floor_global_idx floor_global_idx_get()
#define floor_local_idx floor_local_idx_get()
#define floor_group_idx floor_group_idx_get()
#endif

#endif

#endif
