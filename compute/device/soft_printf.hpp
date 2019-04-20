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

#ifndef __FLOOR_COMPUTE_DEVICE_SOFT_PRINTF_HPP__
#define __FLOOR_COMPUTE_DEVICE_SOFT_PRINTF_HPP__

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_METAL)

namespace soft_printf {

// floating point types are always cast to double
template <typename T, enable_if_t<ext::is_floating_point_v<decay_as_t<T>>>* = nullptr>
constexpr size_t printf_arg_size() { return 8; }
// integral types < 4 bytes are always cast to a 4 byte integral type
template <typename T, enable_if_t<ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) <= 4>* = nullptr>
constexpr size_t printf_arg_size() { return 4; }
// remaining 8 byte integral types
template <typename T, enable_if_t<ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) == 8>* = nullptr>
constexpr size_t printf_arg_size() { return 8; }
// pointers are always 8 bytes (64-bit support only), this includes any kind of char* or char[]
template <typename T, enable_if_t<is_pointer<decay_as_t<T>>::value>* = nullptr>
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

// casts and copies the printf argument to the correct "va_list"/buffer position + handles necessary alignment
template <typename T, enable_if_t<ext::is_floating_point_v<decay_as_t<T>>>* = nullptr>
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(double*)*args_buf = (double)arg;
	*args_buf += 8;
	return 0;
}
template <typename T, enable_if_t<ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) <= 4>* = nullptr>
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	typedef conditional_t<ext::is_signed_v<decay_as_t<T>>, int32_t, uint32_t> int_storage_type;
	*(int_storage_type*)*args_buf = (int_storage_type)arg;
	*args_buf += 4;
	return 0;
}
template <typename T, enable_if_t<ext::is_integral_v<decay_as_t<T>> && sizeof(decay_as_t<T>) == 8>* = nullptr>
floor_inline_always static int printf_arg_copy(const T& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(decay_as_t<T>*)*args_buf = arg;
	*args_buf += 8;
	return 0;
}
template <typename T, enable_if_t<is_pointer<T>::value>* = nullptr>
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

} // soft_printf

#endif

#endif
