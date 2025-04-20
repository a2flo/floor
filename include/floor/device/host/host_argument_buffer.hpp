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

#include <floor/device/argument_buffer.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/core/aligned_ptr.hpp>
#include <floor/device/host/host_function.hpp>

namespace fl {

class host_argument_buffer : public argument_buffer {
public:
	host_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer, const toolchain::function_info& arg_info);
	
	bool set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) override;
	
protected:
	const toolchain::function_info& arg_info;
	
};

} // namespace fl

#endif
