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

#include <floor/compute/argument_buffer.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/aligned_ptr.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <Metal/MTLArgumentEncoder.h>
#include <Metal/MTLComputeCommandEncoder.h>
#include <Metal/MTLRenderCommandEncoder.h>
#include <floor/compute/metal/metal_resource_tracking.hpp>

class metal_argument_buffer : public argument_buffer, public metal_resource_tracking {
public:
	metal_argument_buffer(const compute_kernel& func_, shared_ptr<compute_buffer> storage_buffer, aligned_ptr<uint8_t>&& storage_buffer_backing,
						  id <MTLArgumentEncoder> encoder, const llvm_toolchain::function_info& arg_info, vector<uint32_t>&& arg_indices);
	
	bool set_arguments(const compute_queue& dev_queue, const vector<compute_kernel_arg>& args) override;
	
	//! ensures all tracked resources are resident during the lifetime of the specified encoder
	void make_resident(id <MTLComputeCommandEncoder> enc) const;
	//! ensures all tracked resources are resident during the lifetime of the specified encoder
	void make_resident(id <MTLRenderCommandEncoder> enc, const llvm_toolchain::FUNCTION_TYPE& func_type) const;
	
protected:
	aligned_ptr<uint8_t> storage_buffer_backing;
	id <MTLArgumentEncoder> encoder;
	const llvm_toolchain::function_info& arg_info;
	const vector<uint32_t> arg_indices;
	
};

#endif
