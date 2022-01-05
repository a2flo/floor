/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_argument_buffer.hpp>
#include <floor/compute/compute_context.hpp>

vulkan_argument_buffer::vulkan_argument_buffer(const compute_kernel& func_, shared_ptr<compute_buffer> storage_buffer_,
											   const llvm_toolchain::function_info& arg_info_) :
argument_buffer(func_, storage_buffer_), arg_info(arg_info_) {}

static uint64_t get_buffer_device_address(const compute_buffer& buffer) {
	uint64_t device_address = 0u;
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(buffer.get_flags())) {
		auto vk_buffer = buffer.get_shared_vulkan_buffer();
		if (vk_buffer == nullptr) {
			vk_buffer = (const vulkan_buffer*)&buffer;
#if defined(FLOOR_DEBUG)
			if (auto test_cast_vk_buffer = dynamic_cast<const vulkan_buffer*>(&buffer); !test_cast_vk_buffer) {
				log_error("specified buffer \"$\" is neither a Vulkan buffer nor a shared Vulkan buffer", buffer.get_debug_label());
				return 0u;
			}
#endif
		}
		device_address = vk_buffer->get_vulkan_buffer_device_address();
	} else {
		device_address = ((const vulkan_buffer&)buffer).get_vulkan_buffer_device_address();
	}
#if defined(FLOOR_DEBUG)
	if (device_address == 0) {
		log_error("device address of buffer \"$\" is 0", buffer.get_debug_label());
	}
#endif
	return device_address;
}

void vulkan_argument_buffer::set_arguments(const compute_queue& dev_queue, const vector<compute_kernel_arg>& args) {
	auto vulkan_storage_buffer = (vulkan_buffer*)storage_buffer.get();
	
	// map the memory of the argument buffer so that we can fill it on the CPU side + set up auto unmap
	auto mapped_arg_buffer = vulkan_storage_buffer->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	struct unmap_on_exit_t {
		const compute_queue& dev_queue;
		vulkan_buffer* buffer { nullptr };
		void* mapped_ptr { nullptr };
		unmap_on_exit_t(const compute_queue& dev_queue_, vulkan_buffer* buffer_, void* mapped_ptr_) :
		dev_queue(dev_queue_), buffer(buffer_), mapped_ptr(mapped_ptr_) {
			assert(buffer != nullptr && mapped_ptr != nullptr);
		}
		~unmap_on_exit_t() {
			buffer->unmap(dev_queue, mapped_ptr);
		}
	} unmap_on_exit { dev_queue, vulkan_storage_buffer, mapped_arg_buffer };
	
	auto copy_buffer_ptr = (uint8_t*)mapped_arg_buffer;
	size_t copy_size = 0;
	const auto buffer_size = vulkan_storage_buffer->get_size();
	
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(uint64_t);
			copy_size += arg_size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for buffer pointer in argument buffer");
				return;
			}
			const auto dev_addr = get_buffer_device_address(**buf_ptr);
			memcpy(copy_buffer_ptr, &dev_addr, arg_size);
			copy_buffer_ptr += arg_size;
		} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(uint64_t);
			for (const auto& buf_entry : **vec_buf_ptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return;
				}
				
				const auto dev_addr = (buf_entry ? get_buffer_device_address(*buf_entry) : uint64_t(0));
				memcpy(copy_buffer_ptr, &dev_addr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(uint64_t);
			for (const auto& buf_entry : **vec_buf_sptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return;
				}
				
				const auto dev_addr = (buf_entry ? get_buffer_device_address(*buf_entry) : uint64_t(0));
				memcpy(copy_buffer_ptr, &dev_addr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			log_error("images are not supported for Vulkan");
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			log_error("array of images is not supported for Vulkan");
			return;
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			log_error("array of images is not supported for Vulkan");
			return;
		} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			log_error("nested argument buffers are not supported for Vulkan");
			return;
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			if (arg.size == 0) {
				log_error("generic argument of size 0 can't be set in argument buffer");
				return;
			}
			copy_size += arg.size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for generic argument in argument buffer");
				return;
			}
			memcpy(copy_buffer_ptr, *generic_arg_ptr, arg.size);
			copy_buffer_ptr += arg.size;
		} else {
			log_error("encountered invalid arg");
			return;
		}
	}
}

#endif
