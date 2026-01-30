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

#pragma once

#include <floor/core/essentials.hpp>
#include <floor/device/device_queue.hpp>

namespace fl {

class device_buffer;
class device_function;

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class argument_buffer {
public:
	//! creates an argument buffer using the specified storage buffer
	argument_buffer(const device_function& func, std::shared_ptr<device_buffer> storage_buffer);
	
	virtual ~argument_buffer() = default;
	
	//! returns the backing storage buffer
	const device_buffer* get_storage_buffer() const {
		return storage_buffer.get();
	}
	
	//! sets/encodes the specified arguments in this buffer
	template <typename... Args>
	bool set_arguments(const device_queue& dev_queue, const Args&... args)
	__attribute__((enable_if(device_queue::check_arg_types<Args...>(), "valid args"))) {
		return set_arguments(dev_queue, { args... });
	}
	
	//! sets/encodes the specified arguments in this buffer
	template <typename... Args>
	bool set_arguments(const device_queue& dev_queue, const Args&...)
	__attribute__((enable_if(!device_queue::check_arg_types<Args...>(), "invalid args"), unavailable("invalid argument(s)!")));
	
	//! sets/encodes the specified arguments in this buffer
	virtual bool set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) = 0;
	
	//! sets the debug label for this argument buffer (e.g. for display in a debugger)
	virtual void set_debug_label(const std::string& label);
	//! returns the current debug label
	virtual const std::string& get_debug_label() const;
	
protected:
	const device_function& func;
	std::shared_ptr<device_buffer> storage_buffer;
	std::string debug_label;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
