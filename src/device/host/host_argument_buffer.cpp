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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/device/host/host_argument_buffer.hpp>
#include <floor/device/host/host_buffer.hpp>
#include <floor/device/host/host_image.hpp>

namespace fl {

host_argument_buffer::host_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer_,
										   const toolchain::function_info& arg_info_) :
argument_buffer(func_, storage_buffer_), arg_info(arg_info_) {}

bool host_argument_buffer::set_arguments(const device_queue& dev_queue floor_unused, const std::vector<device_function_arg>& args) {
	auto host_storage_buffer = (host_buffer*)storage_buffer.get();
	
	auto copy_buffer_ptr = host_storage_buffer->get_host_buffer_ptr();
	size_t copy_size = 0;
	const auto buffer_size = host_storage_buffer->get_size();
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(void*);
			copy_size += arg_size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for buffer pointer in argument buffer");
				return false;
			}
			const auto ptr = ((const host_buffer*)(*buf_ptr))->get_host_buffer_ptr_with_sync();
			memcpy(copy_buffer_ptr, &ptr, arg_size);
			copy_buffer_ptr += arg_size;
		} else if (auto vec_buf_ptrs = get_if<const std::vector<device_buffer*>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(void*);
			for (const auto& entry : **vec_buf_ptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return false;
				}
				
				const auto ptr = (entry ? ((const host_buffer*)entry)->get_host_buffer_ptr_with_sync() : nullptr);
				memcpy(copy_buffer_ptr, &ptr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto vec_buf_sptrs = get_if<const std::vector<std::shared_ptr<device_buffer>>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(void*);
			for (const auto& entry : **vec_buf_sptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return false;
				}
				
				const auto ptr = (entry ? ((const host_buffer*)entry.get())->get_host_buffer_ptr_with_sync() : nullptr);
				memcpy(copy_buffer_ptr, &ptr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(void*);
			copy_size += arg_size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for image pointer in argument buffer");
				return false;
			}
			const auto ptr = ((const host_image*)(*img_ptr))->get_host_image_program_info_with_sync();
			memcpy(copy_buffer_ptr, &ptr, arg_size);
			copy_buffer_ptr += arg_size;
		} else if ([[maybe_unused]] auto vec_img_ptrs = get_if<const std::vector<device_image*>*>(&arg.var)) {
			log_error("array of images is not supported for Host-Compute");
			return false;
		} else if ([[maybe_unused]] auto vec_img_sptrs = get_if<const std::vector<std::shared_ptr<device_image>>*>(&arg.var)) {
			log_error("array of images is not supported for Host-Compute");
			return false;
		} else if ([[maybe_unused]] auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			log_error("nested argument buffers are not supported for Host-Compute");
			return false;
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			// NOTE: contrary to param<> arguments in functions, these are copied as objects, not pointers
			if (arg.size == 0) {
				log_error("generic argument of size 0 can't be set in argument buffer");
				return false;
			}
			copy_size += arg.size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for generic argument in argument buffer");
				return false;
			}
			memcpy(copy_buffer_ptr, *generic_arg_ptr, arg.size);
			copy_buffer_ptr += arg.size;
		} else {
			log_error("encountered invalid arg");
			return false;
		}
	}
	
	return true;
}

} // namespace fl

#endif
