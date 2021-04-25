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
#include <floor/core/core.hpp>

namespace metal_device_query {

//! a dummy kernel we'll compile when doing a device query
static NSString* dummy_kernel_source {
	@"kernel void query() {}"
};

std::optional<device_info_t> query(id <MTLDevice> device) {
	// figure out the SIMD width by compiling a dummy kernel and querying the "threadExecutionWidth"
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
	
#if !defined(FLOOR_IOS)
	// we can figure out the compute unit count for AMD devices on macOS
	const string dev_name = [[device name] UTF8String];
	if (dev_name.find("AMD") != string::npos) {
		string ioreg_query;
		core::system("ioreg -r -k \"GPUConfigurationVariable\" | grep \"GPUConfigurationVariable\"", ioreg_query);
		if (!ioreg_query.empty() && ioreg_query.find("GPUConfigurationVariable") != string::npos) {
			auto props_start = ioreg_query.find("= {");
			auto props_end = ioreg_query.rfind('}');
			if (props_start != string::npos && props_end != string::npos && props_start + 3 < props_end) {
				auto props_tokens = core::tokenize(ioreg_query.substr(props_start + 3, (props_end - props_start) - 3), ',');
				uint32_t NumCUPerSH = 1, NumSH = 1, NumSE = 1;
				const auto parse_prop_value = [](const string& prop) -> uint32_t {
					const auto eq_pos = prop.find('=');
					if (eq_pos == string::npos) {
						return 0u;
					}
					return (uint32_t)strtoull(prop.c_str() + eq_pos + 1, nullptr, 10);
				};
				for (const auto& prop_token : props_tokens) {
					if (prop_token.find("\"NumCUPerSH\"") == 0) {
						NumCUPerSH = parse_prop_value(prop_token);
					} else if (prop_token.find("\"NumSH\"") == 0) {
						NumSH = parse_prop_value(prop_token);
					} else if (prop_token.find("\"NumSE\"") == 0) {
						NumSE = parse_prop_value(prop_token);
					}
				}
				info.units = NumCUPerSH * NumSH * NumSE;
			}
		}
	}
#endif
	
	return info;
}

} // namespace metal_device_query

#endif
