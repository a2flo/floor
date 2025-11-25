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

#include <variant>
#include <memory>
#include <vector>
#include <type_traits>
#include <span>
#include <ranges>
#include <floor/constexpr/ext_traits.hpp>

namespace fl {

class device_buffer;
class device_image;
class argument_buffer;

struct device_function_arg {
	constexpr device_function_arg(const device_buffer* buf) noexcept : var(buf) {}
	constexpr device_function_arg(const device_buffer& buf) noexcept : var(&buf) {}
	constexpr device_function_arg(const std::shared_ptr<device_buffer>& buf) noexcept : var(buf.get()) {}
	constexpr device_function_arg(const std::unique_ptr<device_buffer>& buf) noexcept : var(buf.get()) {}
	
	constexpr device_function_arg(const std::vector<const device_buffer*>& bufs) noexcept : var(std::span { bufs.data(), bufs.size() }) {}
	constexpr device_function_arg(const std::vector<device_buffer*>& bufs) noexcept : var(std::span { bufs.data(), bufs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<const device_buffer*, extent>& bufs) noexcept :
	var(std::span { bufs.data(), bufs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<device_buffer*, extent>& bufs) noexcept :
	var(std::span { bufs.data(), bufs.size() }) {}
	constexpr device_function_arg(const std::span<const device_buffer* const> bufs) noexcept : var(bufs) {}
	constexpr device_function_arg(const std::span<const device_buffer*> bufs) noexcept : var(bufs) {}
	constexpr device_function_arg(const std::span<device_buffer*> bufs) noexcept : var(bufs) {}
	
	constexpr device_function_arg(const std::vector<std::shared_ptr<device_buffer>>& bufs) noexcept :
	var(std::span { bufs.data(), bufs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<std::shared_ptr<device_buffer>, extent>& bufs) noexcept :
	var(std::span { bufs.data(), bufs.size() }) {}
	constexpr device_function_arg(const std::span<const std::shared_ptr<device_buffer>> bufs) noexcept : var(bufs) {}
	
	constexpr device_function_arg(const device_image* img) noexcept : var(img) {}
	constexpr device_function_arg(const device_image& img) noexcept : var(&img) {}
	constexpr device_function_arg(const std::shared_ptr<device_image>& img) noexcept : var(img.get()) {}
	constexpr device_function_arg(const std::unique_ptr<device_image>& img) noexcept : var(img.get()) {}
	
	constexpr device_function_arg(const std::vector<const device_image*>& imgs) noexcept : var(std::span { imgs.data(), imgs.size() }) {}
	constexpr device_function_arg(const std::vector<device_image*>& imgs) noexcept : var(std::span { imgs.data(), imgs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<const device_image*, extent>& imgs) noexcept :
	var(std::span { imgs.data(), imgs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<device_image*, extent>& imgs) noexcept :
	var(std::span { imgs.data(), imgs.size() }) {}
	constexpr device_function_arg(const std::span<const device_image* const> imgs) noexcept : var(imgs) {}
	constexpr device_function_arg(const std::span<const device_image*> imgs) noexcept : var(imgs) {}
	constexpr device_function_arg(const std::span<device_image*> imgs) noexcept : var(imgs) {}
	
	constexpr device_function_arg(const std::vector<std::shared_ptr<device_image>>& imgs) noexcept :
	var(std::span { imgs.data(), imgs.size() }) {}
	template <size_t extent> constexpr device_function_arg(const std::array<std::shared_ptr<device_image>, extent>& imgs) noexcept :
	var(std::span { imgs.data(), imgs.size() }) {}
	constexpr device_function_arg(const std::span<const std::shared_ptr<device_image>> imgs) noexcept : var(imgs) {}
	
	constexpr device_function_arg(const argument_buffer* arg_buf) noexcept : var(arg_buf) {}
	constexpr device_function_arg(const argument_buffer& arg_buf) noexcept : var(&arg_buf) {}
	constexpr device_function_arg(const std::shared_ptr<argument_buffer>& arg_buf) noexcept : var(arg_buf.get()) {}
	constexpr device_function_arg(const std::unique_ptr<argument_buffer>& arg_buf) noexcept : var(arg_buf.get()) {}
	
	// adapters for derived device_buffer/device_image/argument_buffer
	template <typename derived_buffer_t> requires std::is_convertible_v<derived_buffer_t*, const device_buffer*>
	constexpr device_function_arg(const derived_buffer_t* buf) noexcept : var((const device_buffer*)buf) {}
	template <typename derived_buffer_t> requires std::is_convertible_v<derived_buffer_t*, const device_buffer*>
	constexpr device_function_arg(const derived_buffer_t& buf) noexcept : var((const device_buffer*)&buf) {}
	
	template <typename derived_image_t> requires std::is_convertible_v<derived_image_t*, const device_image*>
	constexpr device_function_arg(const derived_image_t* img) noexcept : var((const device_image*)img) {}
	template <typename derived_image_t> requires std::is_convertible_v<derived_image_t*, const device_image*>
	constexpr device_function_arg(const derived_image_t& img) noexcept : var((const device_image*)&img) {}
	
	template <typename derived_arg_buffer_t> requires std::is_convertible_v<derived_arg_buffer_t*, const argument_buffer*>
	constexpr device_function_arg(const argument_buffer* arg_buf) noexcept : var((const argument_buffer*)arg_buf) {}
	template <typename derived_arg_buffer_t> requires std::is_convertible_v<derived_arg_buffer_t*, const argument_buffer*>
	constexpr device_function_arg(const argument_buffer& arg_buf) noexcept : var((const argument_buffer*)&arg_buf) {}
	
	//! evaluates to true_type if T is a type pointing to device memory
	template <typename T>
	struct is_device_memory : public std::conditional_t<(std::is_convertible_v<std::decay_t<std::remove_pointer_t<T>>*, const device_buffer*> ||
														 std::is_convertible_v<std::decay_t<std::remove_pointer_t<T>>*, const device_image*> ||
														 std::is_convertible_v<std::decay_t<std::remove_pointer_t<T>>*, const argument_buffer*>),
														std::true_type, std::false_type> {};
	template <typename T> static constexpr bool is_device_memory_v = is_device_memory<T>::value;

	// span arg with CPU storage
	template <typename data_type>
	constexpr device_function_arg(const std::span<data_type>& span_arg) noexcept : var((const void*)span_arg.data()), size(span_arg.size_bytes()) {}
	template <typename data_type, size_t count>
	constexpr device_function_arg(const std::span<data_type, count>& span_arg) noexcept : var((const void*)span_arg.data()), size(span_arg.size_bytes()) {}
	template <typename data_type>
	constexpr device_function_arg(std::span<data_type>&& span_arg) noexcept : var((const void*)span_arg.data()), size(span_arg.size_bytes()) {}
	template <typename data_type, size_t count>
	constexpr device_function_arg(std::span<data_type, count>&& span_arg) noexcept : var((const void*)span_arg.data()), size(span_arg.size_bytes()) {}
	
	// any contiguous sized range with CPU storage (e.g. std::vector), but no smart pointers
	template <std::ranges::contiguous_range cr_type>
	requires (std::ranges::sized_range<cr_type> &&
			  !is_device_memory_v<std::ranges::range_value_t<cr_type>> &&
			  !ext::is_unique_ptr_v<std::ranges::range_value_t<cr_type>> &&
			  !ext::is_shared_ptr_v<std::ranges::range_value_t<cr_type>>)
	constexpr device_function_arg(cr_type&& cr) noexcept : device_function_arg(std::span<const std::ranges::range_value_t<cr_type>>(cr)) {}
	
	// generic arg with CPU storage
	template <typename T> requires (!is_device_memory_v<T> && !ext::is_vector_v<T> && !ext::is_array_v<T> && !ext::is_span_v<T>)
	constexpr device_function_arg(const T& generic_arg) noexcept : var((const void*)&generic_arg), size(sizeof(T)) {}
	
	// forward generic shader_ptrs to one of the constructors above
	template <typename T>
	constexpr device_function_arg(const std::shared_ptr<T>& arg) noexcept : device_function_arg(*arg) {}
	
	// forward generic unique_ptrs to one of the constructors above
	template <typename T>
	constexpr device_function_arg(const std::unique_ptr<T>& arg) noexcept : device_function_arg(*arg) {}
	
	// canonical arg storage
	std::variant<const void*, // generic arg
				 const device_buffer*, // single buffer
				 std::span<const device_buffer* const>, // array of buffers
				 std::span<const std::shared_ptr<device_buffer>>, // array of buffers
				 const device_image*, // single image
				 std::span<const device_image* const>, // array of images
				 std::span<const std::shared_ptr<device_image>>, // array of images
				 const argument_buffer* // single argument buffer
				 > var;
	size_t size { 0 };
	
};

} // namespace fl
