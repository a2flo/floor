/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
	const_func metal_func float sqrt(float) asm("air.fast_sqrt.f32");
	const_func metal_func float rsqrt(float) asm("air.fast_rsqrt.f32");
	const_func metal_func float fabs(float) asm("air.fast_fabs.f32");
	const_func metal_func float abs(float) asm("air.fast_fabs.f32");
	const_func metal_func float fmin(float, float) asm("air.fast_fmin.f32");
	const_func metal_func float fmax(float, float) asm("air.fast_fmax.f32");
	const_func metal_func float floor(float) asm("air.fast_floor.f32");
	const_func metal_func float ceil(float) asm("air.fast_ceil.f32");
	const_func metal_func float round(float) asm("air.fast_round.f32");
	const_func metal_func float trunc(float) asm("air.fast_trunc.f32");
	const_func metal_func float rint(float) asm("air.fast_rint.f32");
	const_func metal_func float sin(float) asm("air.fast_sin.f32");
	const_func metal_func float cos(float) asm("air.fast_cos.f32");
	const_func metal_func float tan(float) asm("air.fast_tan.f32");
	const_func metal_func float asin(float) asm("air.fast_asin.f32");
	const_func metal_func float acos(float) asm("air.fast_acos.f32");
	const_func metal_func float atan(float) asm("air.fast_atan.f32");
	const_func metal_func float atan2(float, float) asm("air.fast_atan2.f32");
	// metal for os x doesn't define an fma function or intrinsic (although mentioned in the docs!),
	// however it still works for the intel backend, but it doesn't for the nvidia backend
#if !defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA) // -> use intrinsic for ios metal and osx metal if intel
	const_func metal_func float fma(float, float, float) asm("air.fma.f32");
#else // -> use nvidia intrinsic on osx
	const_func metal_func float fma(float, float, float) asm("llvm.nvvm.fma.rz.ftz.f");
#endif
	const_func metal_func float exp(float) asm("air.fast_exp.f32");
	const_func metal_func float exp2(float) asm("air.fast_exp2.f32");
	const_func metal_func float log(float) asm("air.fast_log.f32");
	const_func metal_func float log2(float) asm("air.fast_log2.f32");
	const_func metal_func float pow(float, float) asm("air.fast_pow.f32");
	const_func metal_func float fmod(float, float) asm("air.fast_fmod.f32");
	
	const_func floor_inline_always metal_func float copysign(float a, float b) {
		// metal/air doesn't have a builtin function/intrinsic for this and does bit ops instead -> do the same
		uint32_t ret = (*(uint32_t*)&a) & 0x7FFFFFFFu;
		ret |= (*(uint32_t*)&b) & 0x80000000u;
		return *(float*)&ret;
	}
	
	const_func metal_func half abs(half) asm("air.fabs.f16");
	
	const_func metal_func int8_t abs(int8_t) asm("air.abs.s.i8");
	const_func metal_func int16_t abs(int16_t) asm("air.abs.s.i16");
	const_func metal_func int32_t abs(int32_t) asm("air.abs.s.i32");
	const_func metal_func int64_t abs(int64_t val) { return val < int64_t(0) ? -val : val; }
	const_func metal_func uint8_t abs(uint8_t) asm("air.abs.u.i8");
	const_func metal_func uint16_t abs(uint16_t) asm("air.abs.u.i16");
	const_func metal_func uint32_t abs(uint32_t) asm("air.abs.u.i32");
	const_func metal_func uint64_t abs(uint64_t val) { return val; }
	
	const_func metal_func int16_t floor_rt_min(int16_t, int16_t) asm("air.min.s.i16");
	const_func metal_func uint16_t floor_rt_min(uint16_t, uint16_t) asm("air.min.u.i16");
	const_func metal_func int32_t floor_rt_min(int32_t, int32_t) asm("air.min.s.i32");
	const_func metal_func uint32_t floor_rt_min(uint32_t, uint32_t) asm("air.min.u.i32");
	const_func metal_func int64_t floor_rt_min(int64_t x, int64_t y) { return x <= y ? x : y; }
	const_func metal_func uint64_t floor_rt_min(uint64_t x, uint64_t y) { return x <= y ? x : y; }
	const_func metal_func int16_t floor_rt_max(int16_t, int16_t) asm("air.max.s.i16");
	const_func metal_func uint16_t floor_rt_max(uint16_t, uint16_t) asm("air.max.u.i16");
	const_func metal_func int32_t floor_rt_max(int32_t, int32_t) asm("air.max.s.i32");
	const_func metal_func uint32_t floor_rt_max(uint32_t, uint32_t) asm("air.max.u.i32");
	const_func metal_func int64_t floor_rt_max(int64_t x, int64_t y) { return x >= y ? x : y; }
	const_func metal_func uint64_t floor_rt_max(uint64_t x, uint64_t y) { return x >= y ? x : y; }
	
	const_func metal_func int32_t mulhi(int32_t x, int32_t y) asm("air.mul_hi.i32");
	const_func metal_func uint32_t mulhi(uint32_t x, uint32_t y) asm("air.mul_hi.u.i32");
	
	const_func metal_func uint32_t madsat(uint32_t, uint32_t, uint32_t) asm("air.mad_sat.u.i32");
}

// metal itself does not provide get_*_id/get_*_size functions, but rather handles this by using additional kernel arguments
// that must be tagged with a specific attribute. personally, I think this is a bad design decision that adds unnecessary work
// to both the user and the backend/frontend developer (me), and makes everything more complex than it has to be. furthermore,
// the way of doing it like this makes it incompatible to the way opencl and cuda handle this.
// note that "air.get_*_id.i32" intrinsics do exist, but are only partially available or not available at all to all backends,
// and for those that are supported, the return type is sometimes a 32-bit and sometimes a 64-bit integer -> unusable.
// solution: add compiler voodoo that automatically adds the special kernel arguments and loads these arguments in places
// where the following "intrinsics" are used. thus, compatible to the opencl/cuda way of doing it (and sane ...).
const_func uint32_t get_global_id(uint32_t dim) asm("floor.get_global_id.i32");
const_func uint32_t get_global_size(uint32_t dim) asm("floor.get_global_size.i32");
const_func uint32_t get_local_id(uint32_t dim) asm("floor.get_local_id.i32");
const_func uint32_t get_local_size(uint32_t dim) asm("floor.get_local_size.i32");
const_func uint32_t get_group_id(uint32_t dim) asm("floor.get_group_id.i32");
const_func uint32_t get_group_size(uint32_t dim) asm("floor.get_group_size.i32");
const_func uint32_t get_work_dim() asm("floor.get_work_dim.i32");

// barrier and mem_fence functionality
// (note that there is also a air.mem_barrier function, but it seems non-functional/broken and isn't used by apples code)
void air_wg_barrier(uint32_t mem_scope, int32_t sync_scope) __attribute__((noduplicate)) asm("air.wg.barrier");

floor_inline_always static void global_barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_read_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_write_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void local_barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_read_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_write_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_ALL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void image_barrier() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_read_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_write_mem_fence() {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// not supported (neither __printf_cl nor __builtin_printf work)
#define printf(...)

#endif

#endif
