/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/metal/metal_argument_buffer.hpp>
#include <floor/compute/metal/metal_args.hpp>

metal_argument_buffer::metal_argument_buffer(const compute_kernel& func_, shared_ptr<compute_buffer> storage_buffer_,
											 aligned_ptr<uint8_t>&& storage_buffer_backing_,
											 id <MTLArgumentEncoder> encoder_, const llvm_toolchain::function_info& arg_info_) :
argument_buffer(func_, storage_buffer_), storage_buffer_backing(move(storage_buffer_backing_)), encoder(encoder_), arg_info(arg_info_) {}

void metal_argument_buffer::set_arguments(const vector<compute_kernel_arg>& args) {
	auto mtl_storage_buffer = (metal_buffer*)storage_buffer.get();
	auto mtl_buffer = mtl_storage_buffer->get_metal_buffer();
	
	[encoder setArgumentBuffer:mtl_buffer offset:0];
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::ARGUMENT>(encoder, { &arg_info }, args, {});
	
#if !defined(FLOOR_IOS)
	// signal buffer update if this is a managed buffer
	if ((mtl_storage_buffer->get_metal_resource_options() & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged) {
		[mtl_buffer didModifyRange:NSRange { 0, storage_buffer->get_size() }];
	}
#endif
}

#endif
