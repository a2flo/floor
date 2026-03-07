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

#include <floor/device/argument_buffer.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/aligned_ptr.hpp>
#include <floor/device/metal/metal4_function.hpp>
#include <floor/device/toolchain.hpp>
#include <Metal/MTLArgumentEncoder.h>
#include <Metal/MTL4ComputeCommandEncoder.h>
#include <Metal/MTL4RenderCommandEncoder.h>
#include <floor/device/metal/metal4_resource_tracking.hpp>

namespace fl {

class metal4_argument_buffer : public argument_buffer, public metal4_resource_tracking<false> {
public:
	metal4_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer,
						   id <MTLArgumentEncoder> encoder, const toolchain::function_info& arg_info,
						   std::vector<uint32_t>&& arg_indices, const char* debug_label_ = nullptr);
	~metal4_argument_buffer() override;
	
	bool set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) override;
	
	void set_debug_label(const std::string& label) override;
	
	//! returns the current generation of this argument buffer
	//! NOTE: we use this to determine if any indirect resource updates are required (e.g. in indirect pipelines)
	uint64_t get_generation() const {
		return generation;
	}
	
protected:
	id <MTLArgumentEncoder> encoder;
	const toolchain::function_info& arg_info;
	const std::vector<uint32_t> arg_indices;
	uint64_t generation { 0u };
	
};

} // namespace fl

#endif
