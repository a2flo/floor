/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/compute_context.hpp>

vulkan_argument_buffer::vulkan_argument_buffer(const compute_kernel& func_,
											   shared_ptr<compute_buffer> storage_buffer_,
											   const llvm_toolchain::function_info& arg_info_,
											   const vulkan_descriptor_set_layout_t& layout_,
											   vector<VkDeviceSize>&& argument_offsets_,
											   span<uint8_t> mapped_host_memory_,
											   shared_ptr<compute_buffer> constant_buffer_storage_,
											   span<uint8_t> constant_buffer_mapping_) :
argument_buffer(func_, storage_buffer_), arg_info(arg_info_), layout(layout_), argument_offsets(argument_offsets_),
mapped_host_memory(mapped_host_memory_), constant_buffer_storage(constant_buffer_storage_), constant_buffer_mapping(constant_buffer_mapping_) {
	if (mapped_host_memory.data() == nullptr || mapped_host_memory.size_bytes() == 0) {
		throw runtime_error("argument buffer host memory has not been mapped");
	}
	if (constant_buffer_storage && (constant_buffer_mapping.data() == nullptr || constant_buffer_mapping.size_bytes() == 0)) {
		throw runtime_error("constant buffer for argument buffer exists, but no host memory has been mapped");
	}
	if (has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(arg_info.flags)) {
		throw runtime_error("should not have soft-printf flag in argument buffer function info");
	}
#if defined(FLOOR_DEBUG)
	if ((((const vulkan_buffer*)storage_buffer.get())->get_vulkan_buffer_usage() & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) == 0) {
		throw runtime_error("argument buffer storage has not been created with descriptor buffer support");
	}
	const auto& vk_dev = (const vulkan_device&)storage_buffer->get_device();
	if (!vk_dev.descriptor_buffer_support ||
		!vk_dev.argument_buffer_support ||
		!vk_dev.argument_buffer_image_support) {
		throw runtime_error("device has no support for argument buffers");
	}
#endif
}

static inline void arg_pre_handler(const llvm_toolchain::function_info& arg_info,
								   vulkan_kernel::idx_handler& idx) {
	// check validity
	if (arg_info.args[idx.arg].special_type == llvm_toolchain::SPECIAL_TYPE::STAGE_INPUT) {
		throw runtime_error("should not have stage_input argument in argument buffer");
	}
	if (idx.arg >= arg_info.args.size()) {
		throw runtime_error("specified out-of-bounds argument in argument buffer");
	}
}

static inline void arg_post_handler(const llvm_toolchain::function_info& arg_info,
									vulkan_kernel::idx_handler& idx) {
	// check validity
	if (idx.is_implicit) {
		throw runtime_error("should not have implicit arguments in argument buffer");
	}
	
	// advance all indices
	if (arg_info.args[idx.arg].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE) {
		// read/write images are implemented as two args -> inc twice
		++idx.binding;
	}
	++idx.arg;
	++idx.binding;
}

void vulkan_argument_buffer::set_arguments(const compute_queue& dev_queue, const vector<compute_kernel_arg>& args) {
	const auto& vk_dev = (const vulkan_device&)dev_queue.get_device();
	
	// NOTE: since the descriptor buffer is always mapped in host memory, we can just write into it
	span<uint8_t> host_desc_data = mapped_host_memory;
	vulkan_kernel::idx_handler idx;
	for (size_t i = 0, user_arg_idx = 0, arg_count = args.size(); i < arg_count; ++i) {
		arg_pre_handler(arg_info, idx);
		const auto& arg = args[user_arg_idx++];
		
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, *buf_ptr);
		} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, **vec_buf_ptrs);
		} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, **vec_buf_sptrs);
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, *img_ptr);
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, **vec_img_ptrs);
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, **vec_img_sptrs);
		} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			const auto arg_storage_buf = (*arg_buf_ptr)->get_storage_buffer();
			set_argument(vk_dev, idx, host_desc_data, arg_storage_buf);
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_argument(vk_dev, idx, host_desc_data, *generic_arg_ptr, arg.size);
		} else {
			log_error("encountered invalid arg");
			return;
		}
		
		arg_post_handler(arg_info, idx);
	}
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const void* ptr, const size_t& size) const {
	const auto write_offset = argument_offsets[idx.binding];
	if (!idx.is_implicit && arg_info.args[idx.arg].special_type == llvm_toolchain::SPECIAL_TYPE::IUB) {
		// -> inline uniform buffer (directly writes into the descriptor buffer memory)
#if defined(FLOOR_DEBUG)
		if (write_offset + size > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor/argument buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, ptr, size);
	} else {
		// -> plain old SSBO
#if defined(FLOOR_DEBUG)
		if (write_offset + vulkan_buffer::max_ssbo_descriptor_size > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor/argument buffer write");
		}
#endif
		
		assert(constant_buffer_storage != nullptr);
		const auto& const_buffer_info = layout.constant_buffer_info.at(idx.arg);
		assert(const_buffer_info.size == size);
#if defined(FLOOR_DEBUG)
		if (const_buffer_info.offset + const_buffer_info.size > constant_buffer_mapping.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor/argument buffer write");
		}
#endif
		memcpy(constant_buffer_mapping.data() + const_buffer_info.offset, ptr, const_buffer_info.size);
		
		const VkDescriptorAddressInfoEXT addr_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
			.pNext = nullptr,
			.address = ((vulkan_buffer*)constant_buffer_storage.get())->get_vulkan_buffer_device_address() + const_buffer_info.offset,
			.range = const_buffer_info.size,
			.format = VK_FORMAT_UNDEFINED,
		};
		const VkDescriptorGetInfoEXT desc_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
			.pNext = nullptr,
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.data = {
				.pStorageBuffer = &addr_info,
			},
		};
		((vulkan_compute*)vk_dev.context)->vulkan_get_descriptor(vk_dev.device, &desc_info,
																 vk_dev.desc_buffer_sizes.ssbo,
																 host_desc_data.data() + write_offset);
	}
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const compute_buffer* arg) const {
	const auto vk_buffer = ((const vulkan_buffer*)arg)->get_underlying_vulkan_buffer_safe();
	const span<const uint8_t> desc_data { &vk_buffer->get_vulkan_descriptor_data()[0], vk_dev.desc_buffer_sizes.ssbo };
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (write_offset + desc_data.size() > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor/argument buffer write");
	}
#endif
	memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size());
}

template <typename T, typename F>
floor_inline_always static void set_buffer_array_argument(const vulkan_device& vk_dev,
														  const llvm_toolchain::function_info& arg_info,
														  const vector<VkDeviceSize>& argument_offsets,
														  const vulkan_kernel::idx_handler& idx,
														  const span<uint8_t>& host_desc_data,
														  const vector<T>& buffer_array, F&& buffer_accessor) {
	const auto elem_count = arg_info.args[idx.arg].size;
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (elem_count != buffer_array.size()) {
		log_error("invalid buffer array: expected $ elements, got $ elements", elem_count, buffer_array.size());
		return;
	}
	const auto desc_data_total_size = vk_dev.desc_buffer_sizes.ssbo * elem_count;
	if (write_offset + desc_data_total_size > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor/argument buffer write");
	}
#endif
	
	for (uint32_t i = 0; i < elem_count; ++i) {
		const span<const uint8_t> desc_data { &buffer_accessor(buffer_array[i])->get_vulkan_descriptor_data()[0], vk_dev.desc_buffer_sizes.ssbo };
		memcpy(host_desc_data.data() + write_offset + vk_dev.desc_buffer_sizes.ssbo * i, desc_data.data(), vk_dev.desc_buffer_sizes.ssbo);
	}
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const vector<shared_ptr<compute_buffer>>& arg) const {
	set_buffer_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const shared_ptr<compute_buffer>& buf) {
		return (const vulkan_buffer*)buf.get();
	});
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const vector<compute_buffer*>& arg) const {
	set_buffer_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const compute_buffer* buf) {
		return (const vulkan_buffer*)buf;
	});
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev floor_unused,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const compute_image* arg) const {
	const auto vk_img = ((const vulkan_image*)arg)->get_underlying_vulkan_image_safe();
	const auto img_access = arg_info.args[idx.arg].image_access;
	
	// read image desc/obj
	if (img_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ ||
		img_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE) {
		const auto& desc_data = vk_img->get_vulkan_descriptor_data_sampled();
		const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
		if (write_offset + desc_data.size_bytes() > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor/argument buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size());
	}
	
	// write image descs/objs
	if (img_access == llvm_toolchain::ARG_IMAGE_ACCESS::WRITE ||
		img_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE) {
		const auto& desc_data = vk_img->get_vulkan_descriptor_data_storage();
		const uint32_t rw_offset = (img_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE ? 1u : 0u);
		const auto write_offset = argument_offsets[idx.binding + rw_offset];
#if defined(FLOOR_DEBUG)
		if (write_offset + desc_data.size_bytes() > host_desc_data.size_bytes()) {
			throw runtime_error("out-of-bounds descriptor/argument buffer write");
		}
#endif
		memcpy(host_desc_data.data() + write_offset, desc_data.data(), desc_data.size());
	}
}

template <typename T, typename F>
floor_inline_always static void set_image_array_argument(const vulkan_device& vk_dev,
														 const llvm_toolchain::function_info& arg_info,
														 const vector<VkDeviceSize>& argument_offsets,
														 const vulkan_kernel::idx_handler& idx,
														 const span<uint8_t>& host_desc_data,
														 const vector<T>& image_array, F&& image_accessor) {
	// TODO: write/read-write array support

	const auto elem_count = arg_info.args[idx.arg].size;
	const auto desc_data_size = vk_dev.desc_buffer_sizes.sampled_image;
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (elem_count != image_array.size()) {
		log_error("invalid image array: expected $ elements, got $ elements", elem_count, image_array.size());
		return;
	}
	const auto desc_data_total_size = desc_data_size * elem_count;
	if (write_offset + desc_data_total_size > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor/argument buffer write");
	}
#endif
	
	for (uint32_t i = 0; i < elem_count; ++i) {
		const auto& desc_data = image_accessor(image_array[i])->get_vulkan_descriptor_data_sampled();
		memcpy(host_desc_data.data() + write_offset + desc_data_size * i, desc_data.data(), desc_data_size);
	}
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const vector<shared_ptr<compute_image>>& arg) const {
	set_image_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const shared_ptr<compute_image>& img) {
		return (const vulkan_image*)img.get();
	});
}

void vulkan_argument_buffer::set_argument(const vulkan_device& vk_dev,
										  const vulkan_kernel::idx_handler& idx,
										  const span<uint8_t>& host_desc_data,
										  const vector<compute_image*>& arg) const {
	set_image_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const compute_image* img) {
		return (const vulkan_image*)img;
	});
}

#endif
