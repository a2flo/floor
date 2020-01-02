/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_KERNEL_ARG_HPP__
#define __FLOOR_COMPUTE_KERNEL_ARG_HPP__

#include <variant>
#include <memory>
#include <vector>
#include <type_traits>
using namespace std;

class compute_buffer;
class compute_image;

struct compute_kernel_arg {
	constexpr compute_kernel_arg(const compute_buffer* buf) noexcept : var(buf) {}
	constexpr compute_kernel_arg(const compute_buffer& buf) noexcept : var(&buf) {}
	constexpr compute_kernel_arg(const shared_ptr<compute_buffer>& buf) noexcept : var(buf.get()) {}
	constexpr compute_kernel_arg(const unique_ptr<compute_buffer>& buf) noexcept : var(buf.get()) {}
	
	constexpr compute_kernel_arg(const compute_image* img) noexcept : var(img) {}
	constexpr compute_kernel_arg(const compute_image& img) noexcept : var(&img) {}
	constexpr compute_kernel_arg(const shared_ptr<compute_image>& img) noexcept : var(img.get()) {}
	constexpr compute_kernel_arg(const unique_ptr<compute_image>& img) noexcept : var(img.get()) {}
	
	constexpr compute_kernel_arg(const vector<compute_image*>* imgs) noexcept : var(imgs) {}
	constexpr compute_kernel_arg(const vector<compute_image*>& imgs) noexcept : var(&imgs) {}
	
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_image>>* imgs) noexcept : var(imgs) {}
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_image>>& imgs) noexcept : var(&imgs) {}
	
	// adapters for derived compute_buffer/compute_image
	template <typename derived_buffer_t, enable_if_t<is_convertible_v<derived_buffer_t*, compute_buffer*>>* = nullptr>
	constexpr compute_kernel_arg(const derived_buffer_t* buf) noexcept : var((const compute_buffer*)buf) {}
	template <typename derived_buffer_t, enable_if_t<is_convertible_v<derived_buffer_t*, compute_buffer*>>* = nullptr>
	constexpr compute_kernel_arg(const derived_buffer_t& buf) noexcept : var((const compute_buffer*)&buf) {}
	
	template <typename derived_image_t, enable_if_t<is_convertible_v<derived_image_t*, compute_image*>>* = nullptr>
	constexpr compute_kernel_arg(const derived_image_t* img) noexcept : var((const compute_image*)img) {}
	template <typename derived_image_t, enable_if_t<is_convertible_v<derived_image_t*, compute_image*>>* = nullptr>
	constexpr compute_kernel_arg(const derived_image_t& img) noexcept : var((const compute_image*)&img) {}
	
	// generic arg with CPU storage
	template <typename T, enable_if_t<(!is_convertible_v<decay_t<remove_pointer_t<T>>*, compute_buffer*> &&
									   !is_convertible_v<decay_t<remove_pointer_t<T>>*, compute_image*>)>* = nullptr>
	constexpr compute_kernel_arg(const T& generic_arg) noexcept : var((const void*)&generic_arg), size(sizeof(T)) {}
	
	// forward generic shader_ptrs to one of the constructors above
	template <typename T>
	constexpr compute_kernel_arg(const shared_ptr<T>& arg) noexcept : compute_kernel_arg(*arg) {}
	
	// forward generic unique_ptrs to one of the constructors above
	template <typename T>
	constexpr compute_kernel_arg(const unique_ptr<T>& arg) noexcept : compute_kernel_arg(*arg) {}
	
	// canonical arg storage
	variant<const void*, // generic arg
			const compute_buffer*, // single buffer
			const compute_image*, // single image
			const vector<compute_image*>*, // array of images
			const vector<shared_ptr<compute_image>>* // array of images
			> var;
	size_t size { 0 };
	
};

#endif
