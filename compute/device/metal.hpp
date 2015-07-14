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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_HPP__

#if defined(FLOOR_COMPUTE_METAL)

namespace std {
	// straightforward wrapping, use the fast_* version when possible
	metal_func float sqrt(float) asm("air.fast_sqrt.f32");
	metal_func float rsqrt(float) asm("air.fast_rsqrt.f32");
	metal_func float fabs(float) asm("air.fast_fabs.f32");
	metal_func float fmin(float, float) asm("air.fast_fmin.f32");
	metal_func float fmax(float, float) asm("air.fast_fmax.f32");
	metal_func float floor(float) asm("air.fast_floor.f32");
	metal_func float ceil(float) asm("air.fast_ceil.f32");
	metal_func float round(float) asm("air.fast_round.f32");
	metal_func float trunc(float) asm("air.fast_trunc.f32");
	metal_func float rint(float) asm("air.fast_rint.f32");
	metal_func float sin(float) asm("air.fast_sin.f32");
	metal_func float cos(float) asm("air.fast_cos.f32");
	metal_func float tan(float) asm("air.fast_tan.f32");
	metal_func float asin(float) asm("air.fast_asin.f32");
	metal_func float acos(float) asm("air.fast_acos.f32");
	metal_func float atan(float) asm("air.fast_atan.f32");
	metal_func float atan2(float, float) asm("air.fast_atan2.f32");
	// metal for os x doesn't define an fma function or intrinsic (although mentioned in the docs!),
	// however it still works for the intel backend (TODO: amd?), but it doesn't for the nvidia backend
#if !defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA) // -> use intrinsic for ios metal and osx metal if intel
	metal_func float fma(float, float, float) asm("air.fma.f32");
#else // -> use nvidia intrinsic on osx
	metal_func float fma(float, float, float) asm("llvm.nvvm.fma.rz.ftz.f");
#endif
	metal_func float exp(float) asm("air.fast_exp.f32");
	metal_func float exp2(float) asm("air.fast_exp2.f32");
	metal_func float log(float) asm("air.fast_log.f32");
	metal_func float log2(float) asm("air.fast_log2.f32");
	metal_func float pow(float, float) asm("air.fast_pow.f32");
	metal_func float fmod(float, float) asm("air.fast_fmod.f32");
	
	metal_func int16_t abs(int16_t) asm("air.abs.s.i16");
	metal_func int32_t abs(int32_t) asm("air.abs.s.i32");
	metal_func int64_t abs(int64_t) asm("air.abs.s.i64");
	
	metal_func int32_t mulhi(int32_t x, int32_t y) asm("air.mul_hi.i32");
	metal_func uint32_t mulhi(uint32_t x, uint32_t y) asm("air.mul_hi.u.i32");
	metal_func int64_t mulhi(int64_t x, int64_t y) asm("air.mul_hi.i64");
	metal_func uint64_t mulhi(uint64_t x, uint64_t y) asm("air.mul_hi.u.i64");
	
	metal_func uint32_t madsat(uint32_t, uint32_t, uint32_t) asm("air.mad_sat.u.i32");
}

// would usually have to provide these as kernel arguments in metal, but this works as well
// (thx for providing these apple, interesting cl_kernel_air64.h and cl_kernel.h you have there ;))
#if defined(FLOOR_COMPUTE_INFO_OS_IOS)
// on iOS, these all do and have to return 32-bit values, otherwise bad things(tm) will happen
#define FLOOR_GET_ID_RET_TYPE uint32_t
#else
// on OS X, these actually have to return size_t as they're supposed to
#define FLOOR_GET_ID_RET_TYPE size_t
#endif

// these functions are defined for metal on ios and for intel on osx (TODO: amd?), but not for nvidia on osx!
#if !defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA)
FLOOR_GET_ID_RET_TYPE metal_get_global_id(uint32_t dimindx) asm("air.get_global_id.i32");
FLOOR_GET_ID_RET_TYPE metal_get_local_id (uint32_t dimindx) asm("air.get_local_id.i32");
FLOOR_GET_ID_RET_TYPE metal_get_local_size(uint32_t dimindx) asm("air.get_local_size.i32");
FLOOR_GET_ID_RET_TYPE metal_get_group_id(uint32_t dimindx) asm("air.get_group_id.i32");
FLOOR_GET_ID_RET_TYPE metal_get_num_groups(uint32_t dimindx) asm("air.get_num_groups.i32");
uint32_t metal_get_work_dim() asm("air.get_work_dim.i32");

// NOTE: this is broken on intel gpus -> compute it manually
#if !defined(FLOOR_COMPUTE_INFO_VENDOR_INTEL)
FLOOR_GET_ID_RET_TYPE metal_get_global_size(uint32_t dimindx) asm("air.get_global_size.i32");
#else
FLOOR_GET_ID_RET_TYPE metal_get_global_size(uint32_t dimindx) {
	return get_local_size(dimindx) * get_num_groups(dimindx);
}
#endif

// wrap metal id handling functions so that uint32_t is always returned
floor_inline_always uint32_t get_global_id(uint32_t dim) { return uint32_t(metal_get_global_id(dim)); }
floor_inline_always uint32_t get_global_size(uint32_t dim) { return uint32_t(metal_get_global_size(dim)); }
floor_inline_always uint32_t get_local_id(uint32_t dim) { return uint32_t(metal_get_local_id(dim)); }
floor_inline_always uint32_t get_local_size(uint32_t dim) { return uint32_t(metal_get_local_size(dim)); }
floor_inline_always uint32_t get_group_id(uint32_t dim) { return uint32_t(metal_get_group_id(dim)); }
floor_inline_always uint32_t get_num_groups(uint32_t dim) { return uint32_t(metal_get_num_groups(dim)); }
floor_inline_always uint32_t get_work_dim(uint32_t dim) { return metal_get_work_dim(); }

#else // -> use cuda style get_*_id functions if nvidia on osx
#include <floor/compute/device/cuda_id.hpp>
#endif

#undef FLOOR_GET_ID_RET_TYPE

// barrier and mem_fence functionality
// (note that there is also a air.mem_barrier function, but it seems non-functional/broken and isn't used by apples code)
void air_wg_barrier(uint32_t mem_scope, int32_t sync_scope) __attribute__((noduplicate)) asm("air.wg.barrier");

static floor_inline_always void global_barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void global_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void global_read_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void global_write_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

static floor_inline_always void local_barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void local_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void local_read_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
static floor_inline_always void local_write_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

static floor_inline_always void barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_ALL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// not supported (neither __printf_cl nor __builtin_printf work)
#define printf(...)

#endif

#endif
