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

#if !defined(FLOOR_NO_METAL)
#include <floor/core/aligned_ptr.hpp>
#include <floor/device/metal/metal_function.hpp>
#include <floor/device/toolchain.hpp>
#include <Metal/MTLArgumentEncoder.h>
#include <Metal/MTLComputeCommandEncoder.h>
#include <Metal/MTLRenderCommandEncoder.h>
#include <floor/device/metal/metal_resource_tracking.hpp>

namespace fl {

class metal_argument_buffer : public argument_buffer, public metal_resource_tracking {
public:
	metal_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer, aligned_ptr<uint8_t>&& storage_buffer_backing,
						  id <MTLArgumentEncoder> encoder, const toolchain::function_info& arg_info, std::vector<uint32_t>&& arg_indices);
	~metal_argument_buffer() override;
	
	bool set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) override;
	
	//! ensures all tracked resources are resident during the lifetime of the specified encoder
	void make_resident(id <MTLComputeCommandEncoder> enc) const;
	//! ensures all tracked resources are resident during the lifetime of the specified encoder
	void make_resident(id <MTLRenderCommandEncoder> enc, const toolchain::FUNCTION_TYPE& func_type) const;
	
	void set_debug_label(const std::string& label) override;
	
protected:
	aligned_ptr<uint8_t> storage_buffer_backing;
	id <MTLArgumentEncoder> encoder;
	const toolchain::function_info& arg_info;
	const std::vector<uint32_t> arg_indices;
	
};

} // namespace fl

#endif
