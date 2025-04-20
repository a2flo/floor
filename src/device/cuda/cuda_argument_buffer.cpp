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

#if !defined(FLOOR_NO_CUDA)
#include <floor/device/cuda/cuda_argument_buffer.hpp>
#include <floor/device/device_context.hpp>
#include <cassert>

namespace fl {

cuda_argument_buffer::cuda_argument_buffer(const device_function& func_, std::shared_ptr<device_buffer> storage_buffer_,
										   const toolchain::function_info& arg_info_) :
argument_buffer(func_, storage_buffer_), arg_info(arg_info_) {}

bool cuda_argument_buffer::set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) {
	auto cuda_storage_buffer = (cuda_buffer*)storage_buffer.get();
	
	// map the memory of the argument buffer so that we can fill it on the CPU side + set up auto unmap
	auto mapped_arg_buffer = cuda_storage_buffer->map(dev_queue, MEMORY_MAP_FLAG::WRITE_INVALIDATE | MEMORY_MAP_FLAG::BLOCK);
	struct unmap_on_exit_t {
		const device_queue& dev_queue;
		cuda_buffer* buffer { nullptr };
		void* mapped_ptr { nullptr };
		unmap_on_exit_t(const device_queue& dev_queue_, cuda_buffer* buffer_, void* mapped_ptr_) :
		dev_queue(dev_queue_), buffer(buffer_), mapped_ptr(mapped_ptr_) {
			assert(buffer != nullptr && mapped_ptr != nullptr);
		}
		~unmap_on_exit_t() {
			buffer->unmap(dev_queue, mapped_ptr);
		}
	} unmap_on_exit { dev_queue, cuda_storage_buffer, mapped_arg_buffer };
	
	auto copy_buffer_ptr = (uint8_t*)mapped_arg_buffer;
	size_t copy_size = 0;
	const auto buffer_size = cuda_storage_buffer->get_size();
	
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(cu_device_ptr);
			copy_size += arg_size;
			if (copy_size > buffer_size) {
				log_error("out-of-bounds write for buffer pointer in argument buffer");
				return false;
			}
			const auto ptr = ((const cuda_buffer*)(*buf_ptr))->get_cuda_buffer();
			memcpy(copy_buffer_ptr, &ptr, arg_size);
			copy_buffer_ptr += arg_size;
		} else if (auto vec_buf_ptrs = get_if<const std::vector<device_buffer*>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(cu_device_ptr);
			for (const auto& buf_entry : **vec_buf_ptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return false;
				}
				
				const auto ptr = (buf_entry ? ((const cuda_buffer*)buf_entry)->get_cuda_buffer() : cu_device_ptr(0u));
				memcpy(copy_buffer_ptr, &ptr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto vec_buf_sptrs = get_if<const std::vector<std::shared_ptr<device_buffer>>*>(&arg.var)) {
			static constexpr const size_t arg_size = sizeof(cu_device_ptr);
			for (const auto& buf_entry : **vec_buf_sptrs) {
				copy_size += arg_size;
				if (copy_size > buffer_size) {
					log_error("out-of-bounds write for a buffer pointer in an buffer array in argument buffer");
					return false;
				}
				
				const auto ptr = (buf_entry ? ((const cuda_buffer*)buf_entry.get())->get_cuda_buffer() : cu_device_ptr(0u));
				memcpy(copy_buffer_ptr, &ptr, arg_size);
				copy_buffer_ptr += arg_size;
			}
		} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
			auto cu_img = (const cuda_image*)*img_ptr;
			
			// set texture+sampler objects
			const auto& textures = cu_img->get_cuda_textures();
			memcpy(copy_buffer_ptr, textures.data(), textures.size() * sizeof(uint32_t));
			copy_buffer_ptr += textures.size() * sizeof(uint32_t);
			
			// set surface object
			memcpy(copy_buffer_ptr, &cu_img->get_cuda_surfaces()[0], sizeof(uint64_t));
			copy_buffer_ptr += sizeof(uint64_t);
			
			// set ptr to surfaces lod buffer
			const auto lod_buffer = cu_img->get_cuda_surfaces_lod_buffer();
			if(lod_buffer != nullptr) {
				memcpy(copy_buffer_ptr, &lod_buffer->get_cuda_buffer(), sizeof(cu_device_ptr));
			} else {
				memset(copy_buffer_ptr, 0, sizeof(cu_device_ptr));
			}
			copy_buffer_ptr += sizeof(cu_device_ptr);
			
			// set run-time image type
			memcpy(copy_buffer_ptr, &cu_img->get_image_type(), sizeof(IMAGE_TYPE));
			copy_buffer_ptr += sizeof(IMAGE_TYPE);
		} else if ([[maybe_unused]] auto vec_img_ptrs = get_if<const std::vector<device_image*>*>(&arg.var)) {
			log_error("array of images is not supported for CUDA");
			return false;
		} else if ([[maybe_unused]] auto vec_img_sptrs = get_if<const std::vector<std::shared_ptr<device_image>>*>(&arg.var)) {
			log_error("array of images is not supported for CUDA");
			return false;
		} else if ([[maybe_unused]] auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			log_error("nested argument buffers are not supported for CUDA");
			return false;
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
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
