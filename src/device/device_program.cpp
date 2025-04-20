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

#include <floor/device/device_program.hpp>

namespace fl {

device_program::~device_program() {}

std::shared_ptr<device_function> device_program::get_function(const std::string_view& func_name) const {
	const auto iter = find(cbegin(function_names), cend(function_names), func_name);
	if(iter == cend(function_names)) return {};
	return functions[(size_t)distance(cbegin(function_names), iter)];
}

} // namespace fl
