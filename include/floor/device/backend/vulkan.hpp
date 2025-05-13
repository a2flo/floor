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

#if defined(FLOOR_DEVICE_VULKAN)

// similar to Metal, Vulkan id handling functions will be handled on the compiler side and replaced with builtin variables
FLOOR_GLOBAL_ID_RANGE_ATTR const_func uint32_t get_global_id(uint32_t dim) asm("floor.builtin.global_id.i32");
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func uint32_t get_global_size(uint32_t dim) asm("floor.builtin.global_size.i32");
FLOOR_LOCAL_ID_RANGE_ATTR const_func uint32_t get_local_id(uint32_t dim) asm("floor.builtin.local_id.i32");
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func uint32_t get_local_size(uint32_t dim) asm("floor.builtin.local_size.i32");
FLOOR_GROUP_ID_RANGE_ATTR const_func uint32_t get_group_id(uint32_t dim) asm("floor.builtin.group_id.i32");
FLOOR_GROUP_SIZE_RANGE_ATTR const_func uint32_t get_group_size(uint32_t dim) asm("floor.builtin.group_size.i32");
[[range(1u, 3u)]] const_func uint32_t get_work_dim() asm("floor.builtin.work_dim.i32");
FLOOR_SUB_GROUP_ID_RANGE_ATTR uint32_t get_sub_group_id() asm("floor.builtin.sub_group_id.i32");
FLOOR_SUB_GROUP_LOCAL_ID_RANGE_ATTR uint32_t get_sub_group_local_id() asm("floor.builtin.sub_group_local_id.i32");
FLOOR_SUB_GROUP_SIZE_RANGE_ATTR uint32_t get_sub_group_size() asm("floor.builtin.sub_group_size.i32");
FLOOR_NUM_SUB_GROUPS_RANGE_ATTR uint32_t get_num_sub_groups() asm("floor.builtin.num_sub_groups.i32");

#include <floor/device/backend/opencl_common.hpp>

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

namespace std {
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

// 32-bit popcount is natively supported, 16-bit and 64-bit can be easily emulated
const_func uint32_t floor_rt_popcount(uint32_t x) asm("floor.bit_count.u32");
inline const_func uint16_t floor_rt_popcount(uint16_t x) {
	return uint16_t(floor_rt_popcount(uint32_t(x)));
}
inline const_func uint64_t floor_rt_popcount(uint64_t x) {
	return floor_rt_popcount(uint32_t(x)) + floor_rt_popcount(uint32_t(x >> 32ull));
}

} // namespace std

// NOTE: builtin printf is not supported with vulkan
// -> software printf implementation
global uint32_t* floor_get_printf_buffer() asm("floor.builtin.get_printf_buffer");
#include <floor/device/backend/soft_printf.hpp>

template <size_t format_N, typename... Args>
static void printf(constant const char (&format)[format_N], const Args&... args) {
	fl::soft_printf::as::printf_impl(format, args...);
}

// barrier and mem_fence functionality
// NOTE: local = 1, global = 2, image = 4
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z9mem_fencej");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");

void global_barrier() __attribute__((noduplicate, convergent)) asm("floor.barrier.global");
floor_inline_always static void global_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(2u);
}
floor_inline_always static void global_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(2u);
}
floor_inline_always static void global_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(2u);
}

void local_barrier() __attribute__((noduplicate, convergent)) asm("floor.barrier.local");
floor_inline_always static void local_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(1u);
}
floor_inline_always static void local_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(1u);
}
floor_inline_always static void local_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(1u);
}

void barrier() __attribute__((noduplicate, convergent)) asm("floor.barrier.full");

//! NOTE: not guaranteed to be available everywhere
void image_barrier() __attribute__((noduplicate, convergent)) asm("floor.barrier.image");
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(4u);
}

namespace fl {

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

} // namespace fl

// sub-group functionality
#define SUB_GROUP_TYPES(F, P) \
	F(int32_t, "s32", P) F(uint32_t, "u32", P) F(float, "f32", P) \
	F(int16_t, "s16", P) F(uint16_t, "u16", P) F(half, "f16", P)
#define SUB_GROUP_FUNC(type, type_str, func) type func(type, uint32_t lane_idx_delta_or_mask) __attribute__((noduplicate, convergent)) asm("floor.sub_group." #func "." type_str);
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_down)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_up)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_TYPES
#undef SUB_GROUP_FUNC

//! Vulkan parallel group operation implementations / support
namespace fl::algorithm::group {

// compiler side functions
int16_t sub_group_reduce_add(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.s16");
uint16_t sub_group_reduce_add(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.u16");
half sub_group_reduce_add(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.f16");
int32_t sub_group_reduce_add(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.s32");
uint32_t sub_group_reduce_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.u32");
float sub_group_reduce_add(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add.f32");

int16_t sub_group_reduce_min(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.s16");
uint16_t sub_group_reduce_min(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.u16");
half sub_group_reduce_min(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.f16");
int32_t sub_group_reduce_min(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.s32");
uint32_t sub_group_reduce_min(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.u32");
float sub_group_reduce_min(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min.f32");

int16_t sub_group_reduce_max(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.s16");
uint16_t sub_group_reduce_max(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.u16");
half sub_group_reduce_max(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.f16");
int32_t sub_group_reduce_max(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.s32");
uint32_t sub_group_reduce_max(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.u32");
float sub_group_reduce_max(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max.f32");

int16_t sub_group_inclusive_scan_add(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.s16");
uint16_t sub_group_inclusive_scan_add(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.u16");
half sub_group_inclusive_scan_add(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.f16");
int32_t sub_group_inclusive_scan_add(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.s32");
uint32_t sub_group_inclusive_scan_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.u32");
float sub_group_inclusive_scan_add(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add.f32");

int16_t sub_group_inclusive_scan_min(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.s16");
uint16_t sub_group_inclusive_scan_min(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.u16");
half sub_group_inclusive_scan_min(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.f16");
int32_t sub_group_inclusive_scan_min(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.s32");
uint32_t sub_group_inclusive_scan_min(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.u32");
float sub_group_inclusive_scan_min(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min.f32");

int16_t sub_group_inclusive_scan_max(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.s16");
uint16_t sub_group_inclusive_scan_max(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.u16");
half sub_group_inclusive_scan_max(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.f16");
int32_t sub_group_inclusive_scan_max(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.s32");
uint32_t sub_group_inclusive_scan_max(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.u32");
float sub_group_inclusive_scan_max(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max.f32");

int16_t sub_group_exclusive_scan_add(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.s16");
uint16_t sub_group_exclusive_scan_add(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.u16");
half sub_group_exclusive_scan_add(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.f16");
int32_t sub_group_exclusive_scan_add(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.s32");
uint32_t sub_group_exclusive_scan_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.u32");
float sub_group_exclusive_scan_add(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add.f32");

int16_t sub_group_exclusive_scan_min(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.s16");
uint16_t sub_group_exclusive_scan_min(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.u16");
half sub_group_exclusive_scan_min(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.f16");
int32_t sub_group_exclusive_scan_min(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.s32");
uint32_t sub_group_exclusive_scan_min(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.u32");
float sub_group_exclusive_scan_min(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min.f32");

int16_t sub_group_exclusive_scan_max(int16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.s16");
uint16_t sub_group_exclusive_scan_max(uint16_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.u16");
half sub_group_exclusive_scan_max(half value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.f16");
int32_t sub_group_exclusive_scan_max(int32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.s32");
uint32_t sub_group_exclusive_scan_max(uint32_t value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.u32");
float sub_group_exclusive_scan_max(float value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max.f32");

// specialize for all supported operations
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_add(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MIN)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_min(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MAX)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_max(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_inclusive_scan(const data_type& input_value) {
	return sub_group_inclusive_scan_add(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MIN)
static auto sub_group_inclusive_scan(const data_type& input_value) {
	return sub_group_inclusive_scan_min(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MAX)
static auto sub_group_inclusive_scan(const data_type& input_value) {
	return sub_group_inclusive_scan_max(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_exclusive_scan(const data_type& input_value) {
	return sub_group_exclusive_scan_add(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MIN)
static auto sub_group_exclusive_scan(const data_type& input_value) {
	return sub_group_exclusive_scan_min(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, uint16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, int16_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, half> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MAX)
static auto sub_group_exclusive_scan(const data_type& input_value) {
	return sub_group_exclusive_scan_max(input_value);
}

} // namespace fl::algorithm::group

#endif
