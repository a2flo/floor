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

#include <floor/device/argument_buffer.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/device_context.hpp>

namespace fl {

argument_buffer::argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer_) : func(func_), storage_buffer(storage_buffer_) {}

void argument_buffer::set_debug_label(const std::string& label) {
	debug_label = label;
}

const std::string& argument_buffer::get_debug_label() const {
	return debug_label;
}

} // namespace fl
