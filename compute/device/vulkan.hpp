/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_VULKAN_HPP__
#define __FLOOR_COMPUTE_DEVICE_VULKAN_HPP__

#if defined(FLOOR_COMPUTE_VULKAN)

// similar to metal, vulkan id handling functions will be handled on the compiler side and replaced with builtin variables
FLOOR_GLOBAL_ID_RANGE_ATTR const_func uint32_t get_global_id(uint32_t dim) asm("floor.builtin.global_id.i32");
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func uint32_t get_global_size(uint32_t dim) asm("floor.builtin.global_size.i32");
FLOOR_LOCAL_ID_RANGE_ATTR const_func uint32_t get_local_id(uint32_t dim) asm("floor.builtin.local_id.i32");
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func uint32_t get_local_size(uint32_t dim) asm("floor.builtin.local_size.i32");
FLOOR_GROUP_ID_RANGE_ATTR const_func uint32_t get_group_id(uint32_t dim) asm("floor.builtin.group_id.i32");
FLOOR_GROUP_SIZE_RANGE_ATTR const_func uint32_t get_group_size(uint32_t dim) asm("floor.builtin.group_size.i32");
[[range(1u, 3u)]] const_func uint32_t get_work_dim() asm("floor.builtin.work_dim.i32");

#include <floor/compute/device/opencl_common.hpp>

// non-standard bit counting functions (don't use these directly, use math::func instead)
// we don't have direct clz/ctz support
const_func int16_t floor_vulkan_find_int_lsb(uint16_t x) asm("floor.find_int_lsb.u16");
const_func int16_t floor_vulkan_find_int_lsb(int16_t x) asm("floor.find_int_lsb.s16");
const_func int32_t floor_vulkan_find_int_lsb(uint32_t x) asm("floor.find_int_lsb.u32");
const_func int32_t floor_vulkan_find_int_lsb(int32_t x) asm("floor.find_int_lsb.s32");
const_func int64_t floor_vulkan_find_int_lsb(uint64_t x) asm("floor.find_int_lsb.u64");
const_func int64_t floor_vulkan_find_int_lsb(int64_t x) asm("floor.find_int_lsb.s64");

const_func int32_t floor_vulkan_find_int_msb(uint32_t x) asm("floor.find_int_msb.u32"); // 32-bit only
const_func int32_t floor_vulkan_find_int_msb(int32_t x) asm("floor.find_int_msb.s32"); // 32-bit only

const_func uint16_t floor_rt_reverse_bits(uint16_t x) asm("floor.bit_reverse.u16");
const_func uint32_t floor_rt_reverse_bits(uint32_t x) asm("floor.bit_reverse.u32");
const_func uint64_t floor_rt_reverse_bits(uint64_t x) asm("floor.bit_reverse.u64");

// -> forward to lsb/msb functions
const_func uint16_t floor_rt_clz(uint16_t x) {
	// same if we do or do not have 16-bit support
	const auto msb_bit_idx = floor_vulkan_find_int_msb(uint32_t(x));
	return uint16_t(msb_bit_idx < 0 ? 16 : (15 - msb_bit_idx));
}
const_func uint32_t floor_rt_clz(uint32_t x) {
	const auto msb_bit_idx = floor_vulkan_find_int_msb(x);
	return (msb_bit_idx < 0 ? 32 : (31 - msb_bit_idx));
}
const_func uint64_t floor_rt_clz(uint64_t x) {
	// can't use "find_int_msb", b/c it's 32-bit only
	// -> reverse the bits and find the lsb instead
	const auto rev_lsb_bit_idx = floor_vulkan_find_int_lsb(floor_rt_reverse_bits(x));
	return (rev_lsb_bit_idx < 0 ? 64 : rev_lsb_bit_idx);
}

const_func uint16_t floor_rt_ctz(uint16_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 16 : lsb_bit_idx);
}
const_func uint32_t floor_rt_ctz(uint32_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 32 : lsb_bit_idx);
}
const_func uint64_t floor_rt_ctz(uint64_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 64 : lsb_bit_idx);
}

// we have direct support for these
const_func uint16_t floor_rt_popcount(uint16_t x) asm("floor.bit_count.u16");
const_func uint32_t floor_rt_popcount(uint32_t x) asm("floor.bit_count.u32");
const_func uint64_t floor_rt_popcount(uint64_t x) asm("floor.bit_count.u64");

// NOTE: builtin printf is not supported with vulkan
// -> software printf implementation
global uint32_t* floor_get_printf_buffer() asm("floor.builtin.get_printf_buffer");
#include <floor/compute/device/soft_printf.hpp>

template <size_t format_N, typename... Args>
static void printf(constant const char (&format)[format_N], const Args&... args) {
	soft_printf::as::printf_impl(format, args...);
}

// barrier and mem_fence functionality
// NOTE: local = 1, global = 2, image = 4
void cl_barrier(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z7barrierj");
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z9mem_fencej");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");

floor_inline_always static void global_barrier() {
	cl_barrier(2u);
}
floor_inline_always static void global_mem_fence() {
	cl_mem_fence(2u);
}
floor_inline_always static void global_read_mem_fence() {
	cl_read_mem_fence(2u);
}
floor_inline_always static void global_write_mem_fence() {
	cl_write_mem_fence(2u);
}

floor_inline_always static void local_barrier() {
	cl_barrier(1u);
}
floor_inline_always static void local_mem_fence() {
	cl_mem_fence(1u);
}
floor_inline_always static void local_read_mem_fence() {
	cl_read_mem_fence(1u);
}
floor_inline_always static void local_write_mem_fence() {
	cl_write_mem_fence(1u);
}

floor_inline_always static void barrier() {
	cl_barrier(3u);
}

//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_barrier() {
	cl_barrier(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_mem_fence() {
	cl_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_read_mem_fence() {
	cl_read_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_write_mem_fence() {
	cl_write_mem_fence(4u);
}

// tessellation
template <typename point_data_t>
class vulkan_patch_control_point {
public:
	size_t size() const {
		// TODO: implement this
		return 0u;
	}
	
	auto operator[](const size_t idx) const {
		// TODO: implement this
		//return __libfloor_access_patch_control_point(uint32_t(idx), p, point_data_t {});
		return point_data_t {};
	}
	
protected:
	//! compiler-internal opaque type to deal with generic control point types
	__patch_control_point_t p;
	
};

#endif

#endif
