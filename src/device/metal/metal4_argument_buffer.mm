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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/device/metal/metal4_argument_buffer.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/device/metal/metal4_buffer.hpp>

namespace fl {

metal4_argument_buffer::metal4_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer_,
											   id <MTLArgumentEncoder> encoder_, const toolchain::function_info& arg_info_,
											   std::vector<uint32_t>&& arg_indices_, const char* debug_label_) :
argument_buffer(func_, storage_buffer_, debug_label_), metal4_resource_tracking((const metal_device&)storage_buffer->get_device()),
encoder(encoder_), arg_info(arg_info_), arg_indices(std::move(arg_indices_)) {
	// NOTE: not updating the Metal debug label on the storage buffer here, as this will already have been done in metal4_function
}

metal4_argument_buffer::~metal4_argument_buffer() {
	@autoreleasepool {
		encoder = nil;
	}
}

bool metal4_argument_buffer::set_arguments(const device_queue& dev_queue [[maybe_unused]], const std::vector<device_function_arg>& args) {
	@autoreleasepool {
		auto mtl_storage_buffer = (metal4_buffer*)storage_buffer.get();
		auto mtl_buffer = mtl_storage_buffer->get_metal_buffer();
		
		[encoder setArgumentBuffer:mtl_buffer offset:0];
		assert(&dev_queue.get_device() == &mtl_storage_buffer->get_device());
		clear_resources();
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::ARGUMENT_BUFFER>(mtl_storage_buffer->get_device(), encoder,
																							   { &arg_info }, args, {}, resource_container,
																							   (!arg_indices.empty() ? &arg_indices : nullptr))) {
			log_error("failed to set argument buffer$ arguments", debug_label.empty() ? "" : " (" + debug_label + ")");
			return false;
		}
		sort_and_unique_all_resources();
		
		// bump generation + mark as clean
		++generation;
		resource_container.is_dirty = false;
	}
	
	return true;
}

void metal4_argument_buffer::set_debug_label(const std::string& label) {
	argument_buffer::set_debug_label(label);
#if defined(FLOOR_DEBUG)
	if (storage_buffer) {
		@autoreleasepool {
			if (auto mtl_buf = storage_buffer->get_underlying_metal4_buffer_safe(); mtl_buf) {
				const_cast<metal4_buffer*>(mtl_buf)->set_debug_label(label);
			}
		}
	}
#endif
}

} // namespace fl

#endif
