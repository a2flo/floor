/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <variant>
#include <memory>
#include <vector>
#include <type_traits>
#include <span>
using namespace std;

class compute_buffer;
class compute_image;
class argument_buffer;

struct compute_kernel_arg {
	constexpr compute_kernel_arg(const compute_buffer* buf) noexcept : var(buf) {}
	constexpr compute_kernel_arg(const compute_buffer& buf) noexcept : var(&buf) {}
	constexpr compute_kernel_arg(const shared_ptr<compute_buffer>& buf) noexcept : var(buf.get()) {}
	constexpr compute_kernel_arg(const unique_ptr<compute_buffer>& buf) noexcept : var(buf.get()) {}
	
	constexpr compute_kernel_arg(const vector<compute_buffer*>* bufs) noexcept : var(bufs) {}
	constexpr compute_kernel_arg(const vector<compute_buffer*>& bufs) noexcept : var(&bufs) {}
	
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_buffer>>* bufs) noexcept : var(bufs) {}
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_buffer>>& bufs) noexcept : var(&bufs) {}
	
	constexpr compute_kernel_arg(const compute_image* img) noexcept : var(img) {}
	constexpr compute_kernel_arg(const compute_image& img) noexcept : var(&img) {}
	constexpr compute_kernel_arg(const shared_ptr<compute_image>& img) noexcept : var(img.get()) {}
	constexpr compute_kernel_arg(const unique_ptr<compute_image>& img) noexcept : var(img.get()) {}
	
	constexpr compute_kernel_arg(const vector<compute_image*>* imgs) noexcept : var(imgs) {}
	constexpr compute_kernel_arg(const vector<compute_image*>& imgs) noexcept : var(&imgs) {}
	
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_image>>* imgs) noexcept : var(imgs) {}
	constexpr compute_kernel_arg(const vector<shared_ptr<compute_image>>& imgs) noexcept : var(&imgs) {}
	
	constexpr compute_kernel_arg(const argument_buffer* arg_buf) noexcept : var(arg_buf) {}
	constexpr compute_kernel_arg(const argument_buffer& arg_buf) noexcept : var(&arg_buf) {}
	constexpr compute_kernel_arg(const shared_ptr<argument_buffer>& arg_buf) noexcept : var(arg_buf.get()) {}
	constexpr compute_kernel_arg(const unique_ptr<argument_buffer>& arg_buf) noexcept : var(arg_buf.get()) {}
	
	// adapters for derived compute_buffer/compute_image/argument_buffer
	template <typename derived_buffer_t> requires is_convertible_v<derived_buffer_t*, compute_buffer*>
	constexpr compute_kernel_arg(const derived_buffer_t* buf) noexcept : var((const compute_buffer*)buf) {}
	template <typename derived_buffer_t> requires is_convertible_v<derived_buffer_t*, compute_buffer*>
	constexpr compute_kernel_arg(const derived_buffer_t& buf) noexcept : var((const compute_buffer*)&buf) {}
	
	template <typename derived_image_t> requires is_convertible_v<derived_image_t*, compute_image*>
	constexpr compute_kernel_arg(const derived_image_t* img) noexcept : var((const compute_image*)img) {}
	template <typename derived_image_t> requires is_convertible_v<derived_image_t*, compute_image*>
	constexpr compute_kernel_arg(const derived_image_t& img) noexcept : var((const compute_image*)&img) {}
	
	template <typename derived_arg_buffer_t> requires is_convertible_v<derived_arg_buffer_t*, argument_buffer*>
	constexpr compute_kernel_arg(const argument_buffer* arg_buf) noexcept : var((const argument_buffer*)arg_buf) {}
	template <typename derived_arg_buffer_t> requires is_convertible_v<derived_arg_buffer_t*, argument_buffer*>
	constexpr compute_kernel_arg(const argument_buffer& arg_buf) noexcept : var((const argument_buffer*)&arg_buf) {}

	// span arg with CPU storage
	template <typename data_type>
	constexpr compute_kernel_arg(const span<data_type>& span_arg) noexcept : var((const void*)span_arg.data()), size(span_arg.size_bytes()) {}
	
	// generic arg with CPU storage
	template <typename T> requires (!is_convertible_v<decay_t<remove_pointer_t<T>>*, compute_buffer*> &&
									!is_convertible_v<decay_t<remove_pointer_t<T>>*, compute_image*> &&
									!is_convertible_v<decay_t<remove_pointer_t<T>>*, argument_buffer*>)
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
			const vector<compute_buffer*>*, // array of buffers
			const vector<shared_ptr<compute_buffer>>*, // array of buffers
			const compute_image*, // single image
			const vector<compute_image*>*, // array of images
			const vector<shared_ptr<compute_image>>*, // array of images
			const argument_buffer* // single argument buffer
			> var;
	size_t size { 0 };
	
};
