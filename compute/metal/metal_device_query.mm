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

#include <floor/compute/metal/metal_device_query.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/logger.hpp>

namespace metal_device_query {

//! a dummy kernel we'll compile when doing a device query
static NSString* dummy_kernel_source {
	@"kernel void query() {}"
};

std::optional<device_info_t> query(id <MTLDevice> device) {
	NSError* err { nil };
	id <MTLLibrary> query_lib = [device newLibraryWithSource:dummy_kernel_source
													 options:nil
													   error:&err];
	if (!query_lib || err != nil) {
		log_error("failed to create Metal library for device query$",
				 (err != nil ?  ": "s + [[err localizedDescription] UTF8String] : ""));
		return {};
	}
	
	device_info_t info {};
	id <MTLFunction> query_func = [query_lib newFunctionWithName:@"query"];
	if (!query_func) {
		log_error("failed to retrieve query kernel function");
		return {};
	}
	
	id <MTLComputePipelineState> kernel_state = [device newComputePipelineStateWithFunction:query_func error:&err];
	if (!kernel_state || err != nil) {
		log_error("failed to create kernel state for device query$",
				 (err != nil ?  ": "s + [[err localizedDescription] UTF8String] : ""));
		return {};
	}
	
	info.simd_width = (uint32_t)[kernel_state threadExecutionWidth];
	
	return info;
}

} // namespace metal_device_query

#endif
