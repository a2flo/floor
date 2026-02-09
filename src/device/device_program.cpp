/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/device_program.hpp>
#include <floor/core/logger.hpp>

namespace fl {

device_program::~device_program() {}

std::shared_ptr<device_function> device_program::get_function(const std::string_view& func_name) const {
	const auto iter = std::find(function_names.cbegin(), function_names.cend(), func_name);
	if (iter == function_names.cend()) {
		return {};
	}
	return functions[size_t(std::distance(function_names.cbegin(), iter))];
}

const device_function* device_program::get_function_ptr(const std::string_view& func_name) const {
	const auto iter = std::find(function_names.cbegin(), function_names.cend(), func_name);
	if (iter == function_names.cend()) {
		return nullptr;
	}
	const auto idx = size_t(std::distance(function_names.cbegin(), iter));
	return functions.at(idx).get();
}

bool device_program::should_ignore_function_for_device(const device& dev, const toolchain::function_info& func_info) const {
	if (func_info.has_valid_required_simd_width()) {
		if (func_info.required_simd_width < dev.simd_range.x ||
			func_info.required_simd_width > dev.simd_range.y) {
#if defined(FLOOR_DEBUG)
			log_warn("ignoring function \"$\" that requires an unsupported SIMD width $ for device $ (SIMD range [$, $])",
					 func_info.name, func_info.required_simd_width, dev.name, dev.simd_range.x, dev.simd_range.y);
#endif
			return true;
		}
	}
	if (func_info.has_valid_required_local_size()) {
		if (func_info.required_local_size.extent() > dev.max_total_local_size ||
			(func_info.required_local_size > dev.max_local_size).any()) {
#if defined(FLOOR_DEBUG)
			log_warn("ignoring function \"$\" that requires an unsupported local size $ for device $ (max local size $, max total $)",
					 func_info.name, func_info.required_local_size, dev.name, dev.max_local_size, dev.max_total_local_size);
#endif
			return true;
		}
	}
	return false;
}

} // namespace fl
