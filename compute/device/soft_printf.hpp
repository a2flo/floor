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

#ifndef __FLOOR_COMPUTE_DEVICE_SOFT_PRINTF_HPP__
#define __FLOOR_COMPUTE_DEVICE_SOFT_PRINTF_HPP__

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)

namespace soft_printf {

// floating point types are always cast to double
template <typename T> requires(ext::is_floating_point_v<decay_as_t<T>>)
constexpr size_t printf_arg_size() { return 8; }
// integral types < 4 bytes are always cast to a 4 byte integral type
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) <= 4)
constexpr size_t printf_arg_size() { return 4; }
// remaining 8 byte integral types
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) == 8)
constexpr size_t printf_arg_size() { return 8; }
// pointers are always 8 bytes (64-bit support only), this includes any kind of char* or char[]
template <typename T> requires(is_pointer_v<decay_as_t<T>>)
constexpr size_t printf_arg_size() { return 8; }

// computes the total size of a printf argument pack (sum of sizeof of each type) + necessary alignment bytes/sizes
template <typename... Args>
constexpr size_t printf_args_total_size() {
	constexpr const size_t sizes[sizeof...(Args)] {
		printf_arg_size<Args>()...
	};
	size_t sum = 0;
	for(size_t i = 0, count = sizeof...(Args); i < count; ++i) {
		sum += sizes[i];
		
		// align if this type is 8 bytes large and the previous one was 4 bytes
		if(i > 0 && sizes[i] == 8 && sizes[i - 1] == 4) sum += 4;
	}
	return sum;
}

// dummy function needed to expand #Args... function calls
template <typename... Args> floor_inline_always constexpr static void printf_args_apply(const Args&...) {}

////////////////////////////////////////////////////////////////////////////////
// no address-space version (CUDA)
#if defined(FLOOR_COMPUTE_CUDA)
namespace no_as {
// casts and copies the printf argument to the correct "va_list"/buffer position + handles necessary alignment
template <typename T> requires(ext::is_floating_point_v<decay_as_t<T>>)
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(double*)*args_buf = (double)arg;
	*args_buf += 8;
	return 0;
}
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) <= 4)
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	typedef conditional_t<ext::is_signed_v<decay_as_t<T>>, int32_t, uint32_t> int_storage_type;
	*(int_storage_type*)*args_buf = (int_storage_type)arg;
	*args_buf += 4;
	return 0;
}
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) == 8)
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(decay_as_t<T>*)*args_buf = arg;
	*args_buf += 8;
	return 0;
}
template <typename T> requires(is_pointer_v<T>)
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(decay_as_t<T>*)*args_buf = arg;
	*args_buf += 8;
	return 0;
}
floor_inline_always static int printf_arg_copy(constant const char* arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(const char**)*args_buf = arg;
	*args_buf += 8;
	return 0;
}

// forwarder and handler of printf_arg_copy (we need to get and specialize for the actual storage type)
template <typename T> floor_inline_always static int printf_handle_arg(const T& arg, uint8_t** args_buf) {
	printf_arg_copy(arg, args_buf);
	return 0;
}

} // no_as
#endif

////////////////////////////////////////////////////////////////////////////////
// address-space version (Metal/Vulkan)
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
namespace as {

// casts and copies the printf argument to the correct "va_list"/buffer position + handles necessary alignment
template <typename T> requires(ext::is_floating_point_v<decay_as_t<T>>)
floor_inline_always static void printf_arg_copy(global uint32_t*& dst, const T& arg) {
	// NOTE: cast any floating point types to 32-bit float
#if defined(FLOOR_COMPUTE_VULKAN)
	// TODO/NOTE: can't cast dst to float* yet, so bitcast the arg instead
	*(global int32_t*)dst = floor_bitcast_f32_to_i32((float)arg);
#else
	*(global float*)dst = (float)arg;
#endif
	++dst;
}
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) <= 4)
floor_inline_always static void printf_arg_copy(global uint32_t*& dst, const T& arg) {
	typedef conditional_t<ext::is_signed_v<decay_as_t<T>>, int32_t, uint32_t> int_storage_type;
	*(global int_storage_type*)dst = (int_storage_type)arg;
	++dst;
}
template <typename T> requires(ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) == 8)
floor_inline_always static void printf_arg_copy(global uint32_t*& dst, const T& arg) {
	// NOTE: 64-bit integer types aren't supporte right now -> cast down to 32-bit
	typedef conditional_t<ext::is_signed_v<decay_as_t<T>>, int32_t, uint32_t> int_storage_type;
	*(global int_storage_type*)dst = (int_storage_type)arg;
	++dst;
}
template <typename T> requires(is_pointer_v<T>)
floor_inline_always static void printf_arg_copy(global uint32_t*& dst, const T& arg)
__attribute__((unavailable("pointer arguments in printf are currently not supported")));
// TODO: support string printing
//floor_inline_always static void printf_arg_copy(global uint8_t*& dst, constant const char* arg) {}

// forwarder and handler of printf_arg_copy (we need to get and specialize for the actual storage type)
template <typename T> floor_inline_always static int printf_handle_arg(global uint32_t*& dst, const T& arg) {
	printf_arg_copy(dst, arg);
	return 0; // dummy ret for expansion
}

// actual printf implementation
template <size_t format_N, typename... Args>
static void printf_impl(constant const char (&format)[format_N], const Args&... args) {
	// NOTE: we only support 32-bit args/values right now, so args size is always 4 bytes * #args
	static constexpr const auto args_size = sizeof...(args) * sizeof(uint32_t);
	
	// we know how many bytes need to be copied
	// -> always round up size to a multiple of 4
	const auto round_to_4 = [](const uint32_t& num) constexpr {
		switch (num % 4u) {
			case 1: return num + 3;
			case 2: return num + 2;
			case 3: return num + 1;
			default: break;
		}
		return num;
	};
	static constexpr const auto total_size = round_to_4(uint32_t(format_N)) + uint32_t(4 /* entry header */ + args_size);
	
	// the first 4 bytes of the printf buffer specify the current write offset
	// the next 4 bytes specify the max printf buffer size
	auto printf_buf = floor_get_printf_buffer();
	
	// short circuit overflow check
	if (*printf_buf >= *(printf_buf + 1)) {
		return; // already too large
	}
	
	// global atomic add to total size
	const auto offset = atomic_add(printf_buf, total_size);
	if (offset + total_size >= *(printf_buf + 1)) {
		return; // out of bounds
	}
	
	// store size of the entry, format string, then args
	auto dst_printf_buf = printf_buf + offset / sizeof(uint32_t);
	*dst_printf_buf = total_size;
	{
		auto dst_ptr = (dst_printf_buf + 1 /* entry header */);
		auto src_ptr = (constant const uint32_t*)format;
		constexpr const uint32_t even_copies = format_N / 4u;
#pragma unroll
		for (uint32_t i = 0; i < even_copies; ++i) {
			*dst_ptr++ = *src_ptr++;
		}
		constexpr const uint32_t rem_copies = format_N - (even_copies * 4u);
		if (rem_copies > 0) {
			const uint32_t val = *src_ptr & ((1u << (rem_copies * 8u)) - 1u);
			*dst_ptr++ = *src_ptr & ((1u << (rem_copies * 8u)) - 1u);
		}
	}
	
	if constexpr (sizeof...(args) > 0) {
		auto dst_ptr = dst_printf_buf + 1 /* entry header */ + (round_to_4(format_N) / 4u);
		soft_printf::printf_args_apply(soft_printf::as::printf_handle_arg(dst_ptr, args)...);
	}
}

} // as
#endif

} // soft_printf

#endif

#endif
