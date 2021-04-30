/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_METAL_ARGUMENT_BUFFER_HPP__
#define __FLOOR_METAL_ARGUMENT_BUFFER_HPP__

#include <floor/compute/argument_buffer.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/aligned_ptr.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <Metal/MTLArgumentEncoder.h>

class metal_argument_buffer : public argument_buffer {
public:
	metal_argument_buffer(const compute_kernel& func_, shared_ptr<compute_buffer> storage_buffer, aligned_ptr<uint8_t>&& storage_buffer_backing,
						  id <MTLArgumentEncoder> encoder, const llvm_toolchain::function_info& arg_info, vector<uint32_t>&& arg_indices);
	
	void set_arguments(const vector<compute_kernel_arg>& args) override;
	
protected:
	aligned_ptr<uint8_t> storage_buffer_backing;
	id <MTLArgumentEncoder> encoder;
	const llvm_toolchain::function_info& arg_info;
	const vector<uint32_t> arg_indices;
	
};

#endif

#endif
