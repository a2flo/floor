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

#include <floor/compute/llvm_toolchain.hpp>
using namespace llvm_toolchain;

#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_argument_buffer.hpp>

//! Vulkan compute/vertex/fragment/argument-buffer argument handler/setter
//! NOTE: do not include manually
namespace vulkan_args {

enum class ENCODER_TYPE {
	COMPUTE,
	SHADER,
	ARGUMENT,
	INDIRECT_SHADER,
	INDIRECT_COMPUTE,
};

struct idx_handler {
	// actual argument index (directly corresponding to the c++ source code)
	uint32_t arg { 0 };
	// binding index in the resp. descriptor set
	uint32_t binding { 0 };
	// flag if this is an implicit arg
	bool is_implicit { false };
	// current implicit argument index
	uint32_t implicit { 0 };
	// current kernel/shader entry (set)
	uint32_t entry { 0 };
};

//! when using kernels/shaders that require additional constant buffers (i.e. UBOs are not enough), this wraps all the necessary info
struct constant_buffer_wrapper_t {
	const floor_core::flat_map<uint32_t, vulkan_constant_buffer_info_t>* constant_buffer_info = nullptr;
	compute_buffer* constant_buffer_storage = nullptr;
	span<uint8_t> constant_buffer_mapping {};
};

//! used to gather all necessary image transitions
struct transition_info_t {
	//! if set, won't transition kernel/shader image arguments to read or write optimal layout during argument encoding
	//! NOTE: this is useful in cases we don't want to or can't have a pipeline barrier
	bool allow_generic_layout { false };
	//! all gathered image transitions / barriers
	vector<VkImageMemoryBarrier2> barriers;
};

template <ENCODER_TYPE enc_type>
static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const void* ptr, const size_t& size,
								const constant_buffer_wrapper_t* const_buf) {
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
		if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE ||
					  enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
			throw runtime_error("should not have any constant buffer SSBOs in indirect compute/shader arguments");
#endif
		} else {
			// -> plain old SSBO
			assert(const_buf);
#if defined(FLOOR_DEBUG)
			if (write_offset + vulkan_buffer::max_ssbo_descriptor_size > host_desc_data.size_bytes()) {
				throw runtime_error("out-of-bounds descriptor/argument buffer write");
			}
#endif
			
			const auto& const_buffer_info = const_buf->constant_buffer_info->at(idx.arg);
			assert(const_buffer_info.size == size);
#if defined(FLOOR_DEBUG)
			if (const_buffer_info.offset + const_buffer_info.size > const_buf->constant_buffer_mapping.size_bytes()) {
				throw runtime_error("out-of-bounds descriptor/argument buffer write");
			}
#endif
			memcpy(const_buf->constant_buffer_mapping.data() + const_buffer_info.offset, ptr, const_buffer_info.size);
			
			const VkDescriptorAddressInfoEXT addr_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
				.pNext = nullptr,
				.address = ((vulkan_buffer*)const_buf->constant_buffer_storage)->get_vulkan_buffer_device_address() + const_buffer_info.offset,
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
}

static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info [[maybe_unused]],
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const compute_buffer* arg) {
	const auto vk_buffer = arg->get_underlying_vulkan_buffer_safe();
	const span<const uint8_t> desc_data { &vk_buffer->get_vulkan_descriptor_data()[0], vk_dev.desc_buffer_sizes.ssbo };
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (!idx.is_implicit && arg_info.args[idx.arg].special_type != llvm_toolchain::SPECIAL_TYPE::SSBO) {
		throw runtime_error("argument is not a buffer, but a buffer was specified");
	}
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
														  const idx_handler& idx,
														  const span<uint8_t>& host_desc_data,
														  const vector<T>& buffer_array, F&& buffer_accessor) {
	assert(!idx.is_implicit);
	const auto elem_count = arg_info.args[idx.arg].size;
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (arg_info.args[idx.arg].special_type != llvm_toolchain::SPECIAL_TYPE::BUFFER_ARRAY) {
		throw runtime_error("argument is not a buffer array, but a buffer array was specified");
	}
	if (elem_count != buffer_array.size()) {
		throw runtime_error("invalid buffer array: expected " + to_string(elem_count) + " elements, got " +
							to_string(buffer_array.size()) + " elements");
	}
	const auto desc_data_total_size = vk_dev.desc_buffer_sizes.ssbo * elem_count;
	if (write_offset + desc_data_total_size > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor/argument buffer write");
	}
#endif
	
	for (uint32_t i = 0; i < elem_count; ++i) {
		auto buf_ptr = buffer_accessor(buffer_array[i]);
		if (!buf_ptr) {
			memset(host_desc_data.data() + write_offset + vk_dev.desc_buffer_sizes.ssbo * i, 0, vk_dev.desc_buffer_sizes.ssbo);
			continue;
		}
		const span<const uint8_t> desc_data { &buf_ptr->get_vulkan_descriptor_data()[0], vk_dev.desc_buffer_sizes.ssbo };
		memcpy(host_desc_data.data() + write_offset + vk_dev.desc_buffer_sizes.ssbo * i, desc_data.data(), vk_dev.desc_buffer_sizes.ssbo);
	}
}

static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const vector<shared_ptr<compute_buffer>>& arg) {
	set_buffer_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const shared_ptr<compute_buffer>& buf) {
		return (buf ? buf->get_underlying_vulkan_buffer_safe() : nullptr);
	});
}

static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const vector<compute_buffer*>& arg) {
	set_buffer_array_argument(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, [](const compute_buffer* buf) {
		return (buf ? buf->get_underlying_vulkan_buffer_safe() : nullptr);
	});
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const vulkan_device& vk_dev floor_unused,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const compute_image* arg,
								transition_info_t* transition_info) {
	assert(!idx.is_implicit);
	const auto vk_img = arg->get_underlying_vulkan_image_safe();
	const auto img_access = arg_info.args[idx.arg].image_access;
	
#if defined(FLOOR_DEBUG)
	if (arg_info.args[idx.arg].image_type == llvm_toolchain::ARG_IMAGE_TYPE::NONE) {
		throw runtime_error("argument is not an image, but an image was specified");
	}
#endif
	
	// soft-transition image if request + gather transition info
	if constexpr (enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::SHADER) {
		if (transition_info) {
			auto vk_img_mut = const_cast<compute_image*>(arg)->get_underlying_vulkan_image_safe();
			if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
				auto [needs_transition, barrier] = vk_img_mut->transition_write(nullptr, nullptr,
																				// also readable?
																				img_access == ARG_IMAGE_ACCESS::READ_WRITE,
																				// always direct-write, never attachment
																				true,
																				// allow general layout?
																				transition_info->allow_generic_layout,
																				// soft transition
																				true);
				if (needs_transition) {
					transition_info->barriers.emplace_back(std::move(barrier));
				}
			} else { // READ
				auto [needs_transition, barrier] = vk_img_mut->transition_read(nullptr, nullptr,
																			   // allow general layout?
																			   transition_info->allow_generic_layout,
																			   // soft transition
																			   true);
				if (needs_transition) {
					transition_info->barriers.emplace_back(std::move(barrier));
				}
			}
		}
	}
	
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

template <ENCODER_TYPE enc_type, typename T, typename F>
floor_inline_always static void set_image_array_argument(const vulkan_device& vk_dev,
														 const llvm_toolchain::function_info& arg_info,
														 const vector<VkDeviceSize>& argument_offsets,
														 const idx_handler& idx,
														 const span<uint8_t>& host_desc_data,
														 const vector<T>& image_array,
														 transition_info_t* transition_info,
														 F&& image_accessor) {
	assert(!idx.is_implicit);
	// TODO: write/read-write array support
	
#if defined(FLOOR_DEBUG)
	if (arg_info.args[idx.arg].special_type != llvm_toolchain::SPECIAL_TYPE::IMAGE_ARRAY) {
		throw runtime_error("argument is not an image array, but an image array was specified");
	}
#endif
	
	// soft-transition image if request + gather transition info
	if constexpr (enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::SHADER) {
		if (transition_info) {
			const auto img_access = arg_info.args[idx.arg].image_access;
			if (img_access == ARG_IMAGE_ACCESS::WRITE || img_access == ARG_IMAGE_ACCESS::READ_WRITE) {
				for (auto& img : image_array) {
					auto img_ptr = image_accessor(img);
					if (!img_ptr) {
						continue;
					}
					auto vk_img_mut = const_cast<vulkan_image*>(img_ptr)->get_underlying_vulkan_image_safe();
					auto [needs_transition, barrier] = vk_img_mut->transition_write(nullptr, nullptr,
																					// also readable?
																					img_access == ARG_IMAGE_ACCESS::READ_WRITE,
																					// always direct-write, never attachment
																					true,
																					// allow general layout?
																					transition_info->allow_generic_layout,
																					// soft transition
																					true);
					if (needs_transition) {
						transition_info->barriers.emplace_back(std::move(barrier));
					}
				}
			} else { // READ
				for (auto& img : image_array) {
					auto img_ptr = image_accessor(img);
					if (!img_ptr) {
						continue;
					}
					auto vk_img_mut = const_cast<vulkan_image*>(img_ptr)->get_underlying_vulkan_image_safe();
					auto [needs_transition, barrier] = vk_img_mut->transition_read(nullptr, nullptr,
																				   // allow general layout?
																				   transition_info->allow_generic_layout,
																				   // soft transition
																				   true);
					if (needs_transition) {
						transition_info->barriers.emplace_back(std::move(barrier));
					}
				}
			}
		}
	}
	
	const auto elem_count = arg_info.args[idx.arg].size;
	const auto desc_data_size = vk_dev.desc_buffer_sizes.sampled_image;
	const auto write_offset = argument_offsets[idx.binding];
#if defined(FLOOR_DEBUG)
	if (elem_count != image_array.size()) {
		throw runtime_error("invalid image array: expected " + to_string(elem_count) + " elements, got " +
							to_string(image_array.size()) + " elements");
	}
	const auto desc_data_total_size = desc_data_size * elem_count;
	if (write_offset + desc_data_total_size > host_desc_data.size_bytes()) {
		throw runtime_error("out-of-bounds descriptor/argument buffer write");
	}
#endif
	
	for (uint32_t i = 0; i < elem_count; ++i) {
		auto img_ptr = image_accessor(image_array[i]);
		if (!img_ptr) {
			memset(host_desc_data.data() + write_offset + desc_data_size * i, 0, desc_data_size);
			continue;
		}
		const auto& desc_data = img_ptr->get_vulkan_descriptor_data_sampled();
		memcpy(host_desc_data.data() + write_offset + desc_data_size * i, desc_data.data(), desc_data_size);
	}
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const vector<shared_ptr<compute_image>>& arg,
								transition_info_t* transition_info) {
	set_image_array_argument<enc_type>(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, transition_info,
									   [](const shared_ptr<compute_image>& img) {
		return (img ? img->get_underlying_vulkan_image_safe() : nullptr);
	});
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const vulkan_device& vk_dev,
								const idx_handler& idx,
								const function_info& arg_info,
								const vector<VkDeviceSize>& argument_offsets,
								const span<uint8_t>& host_desc_data,
								const vector<compute_image*>& arg,
								transition_info_t* transition_info) {
	set_image_array_argument<enc_type>(vk_dev, arg_info, argument_offsets, idx, host_desc_data, arg, transition_info, [](const compute_image* img) {
		return (img ? img->get_underlying_vulkan_image_safe() : nullptr);
	});
}

//! returns the entry for the current indices, makes sure that stage_input args are ignored
template <ENCODER_TYPE enc_type>
static inline std::tuple<const function_info*, const vector<VkDeviceSize>*, const constant_buffer_wrapper_t*, span<uint8_t>>
arg_pre_handler(const vector<span<uint8_t>>& mapped_host_desc_data,
				const vector<const function_info*>& entries,
				const vector<const vector<VkDeviceSize>*> per_entry_argument_offsets,
				const vector<const constant_buffer_wrapper_t*>& per_entry_const_buffers,
				idx_handler& idx) {
	// make sure we have a usable entry
	const function_info* entry = nullptr;
	const vector<VkDeviceSize>* argument_offsets = nullptr;
	const constant_buffer_wrapper_t* const_buf = nullptr;
	span<uint8_t> host_desc_data = mapped_host_desc_data[0];
	for (;;) {
		// get the next non-nullptr entry or use the current one if it's valid
		while (entries[idx.entry] == nullptr) {
			++idx.entry;
			if (idx.entry >= entries.size()) {
#if defined(FLOOR_DEBUG)
				throw runtime_error("shader/kernel entry out of bounds");
#else
				log_error("shader/kernel entry out of bounds");
#endif
				return { nullptr, nullptr, nullptr, {} };
			}
		}
		entry = entries[idx.entry];
		argument_offsets = (idx.entry < per_entry_argument_offsets.size() ? per_entry_argument_offsets[idx.entry] : nullptr);
		const_buf = (idx.entry < per_entry_const_buffers.size() ? per_entry_const_buffers[idx.entry] : nullptr);
		host_desc_data = mapped_host_desc_data[idx.entry];
		
		// ignore any stage input args
		while (idx.arg < entry->args.size() &&
			   entry->args[idx.arg].special_type == SPECIAL_TYPE::STAGE_INPUT) {
			if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
				throw runtime_error("should not have stage_input argument in argument buffer");
			}
			++idx.arg;
		}
		
		// have all args been specified for this entry?
		if (idx.arg >= entry->args.size()) {
			// implicit args at the end
			const auto implicit_arg_count = (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->flags) ? 1u : 0u);
			if (idx.arg < entry->args.size() + implicit_arg_count) {
				idx.is_implicit = true;
			} else { // actual end
				// get the next entry
				++idx.entry;
				// reset
				idx.arg = 0;
				idx.binding = 0;
				idx.is_implicit = false;
				idx.implicit = 0;
				continue;
			}
		}
		break;
	}
	
	return { entry, argument_offsets, const_buf, host_desc_data };
}

//! increments indicies dependent on the arg
static inline void arg_post_handler(const function_info& arg_info,
									idx_handler& idx) {
	// advance all indices
	if (!idx.is_implicit) {
		if (arg_info.args[idx.arg].image_access == ARG_IMAGE_ACCESS::READ_WRITE) {
			// read/write images are implemented as two args -> inc twice
			++idx.binding;
		}
	} else {
		++idx.implicit;
	}
	// argument buffer doesn't use a binding, it's a separate descriptor set
	if (idx.is_implicit || (!idx.is_implicit && arg_info.args[idx.arg].special_type != SPECIAL_TYPE::ARGUMENT_BUFFER)) {
		++idx.binding;
	}
	// next arg
	++idx.arg;
}

//! sets and handles all arguments in the compute/vertex/fragment function or argument buffer
//! NOTE: if "transition_info" is non-nullptr, this will gather all necessary transition info into it
template <ENCODER_TYPE enc_type>
static inline pair<bool, vector<pair<uint32_t /* entry idx */, const vulkan_buffer* /* argument buffer storage */>>>
set_arguments(const vulkan_device& dev,
			  const vector<span<uint8_t>>& mapped_host_desc_data,
			  const vector<const function_info*>& entries,
			  const vector<const vector<VkDeviceSize>*>& per_entry_argument_offsets,
			  const vector<const constant_buffer_wrapper_t*>& per_entry_const_buffers,
			  const vector<compute_kernel_arg>& args,
			  const vector<compute_kernel_arg>& implicit_args,
			  transition_info_t* transition_info = nullptr) {
	// transition_info can and must only be set for direct COMPUTE/SHADER encoding/execution
	assert(((enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::SHADER) && transition_info) ||
		   ((enc_type != ENCODER_TYPE::COMPUTE && enc_type != ENCODER_TYPE::SHADER) && !transition_info));
	
	idx_handler idx;
	const size_t arg_count = args.size() + implicit_args.size();
	size_t explicit_idx = 0, implicit_idx = 0;
	vector<pair<uint32_t /* entry idx */, const vulkan_buffer* /* argument buffer storage */>> argument_buffers;
	for (size_t i = 0; i < arg_count; ++i) {
#if defined(FLOOR_DEBUG)
		try
#endif
		{
			auto [arg_info_ptr, argument_offsets_ptr, const_buf_ptr, host_desc_data] = arg_pre_handler<enc_type>(mapped_host_desc_data, entries,
																												 per_entry_argument_offsets,
																												 per_entry_const_buffers, idx);
			if (!arg_info_ptr || !argument_offsets_ptr) {
				return { false, {} };
			}
			const auto& arg_info = *arg_info_ptr;
			const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
			const auto& arg_offsets = *argument_offsets_ptr;
			
			if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
				set_argument(dev, idx, arg_info, arg_offsets, host_desc_data, *buf_ptr);
			} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
				set_argument(dev, idx, arg_info, arg_offsets, host_desc_data, **vec_buf_ptrs);
			} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
				set_argument(dev, idx, arg_info, arg_offsets, host_desc_data, **vec_buf_sptrs);
			} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
				set_argument<enc_type>(dev, idx, arg_info, arg_offsets, host_desc_data, *img_ptr, transition_info);
			} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
				set_argument<enc_type>(dev, idx, arg_info, arg_offsets, host_desc_data, **vec_img_ptrs, transition_info);
			} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
				set_argument<enc_type>(dev, idx, arg_info, arg_offsets, host_desc_data, **vec_img_sptrs, transition_info);
			} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
				// argument buffers may not be set by this: these must be handled by the user -> collect and return them
				const auto arg_storage_buf = (*arg_buf_ptr)->get_storage_buffer();
				argument_buffers.emplace_back(idx.entry, (const vulkan_buffer*)arg_storage_buf);
			} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
				set_argument<enc_type>(dev, idx, arg_info, arg_offsets, host_desc_data, *generic_arg_ptr, arg.size, const_buf_ptr);
			} else {
				log_error("encountered invalid arg");
				return { false, {} };
			}
			
			arg_post_handler(arg_info, idx);
		}
#if defined(FLOOR_DEBUG)
		catch (std::exception& exc) {
			log_error("in $: argument #$: $", (idx.entry < entries.size() && entries[idx.entry] ?
											   entries[idx.entry]->name : "<invalid-function>"), i, exc.what());
			return { false, {} };
		}
#endif
	}
	
	return { true, argument_buffers };
}

} // namespace vulkan_args
