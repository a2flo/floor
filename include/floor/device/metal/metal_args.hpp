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

#pragma once

#include <floor/device/toolchain.hpp>
#include <floor/device/metal/metal_buffer.hpp>
#include <floor/device/metal/metal_image.hpp>
#include <floor/device/metal/metal_argument_buffer.hpp>
#include <floor/device/metal/metal_resource_tracking.hpp>
#include <Metal/MTLComputeCommandEncoder.h>
#include <Metal/MTLRenderCommandEncoder.h>
#include <Metal/MTLArgumentEncoder.h>

namespace fl {
using namespace toolchain;

//! Metal compute/vertex/fragment/argument-buffer argument handler/setter
//! NOTE: do not include manually
namespace metal_args {
	enum class ENCODER_TYPE {
		COMPUTE,
		SHADER,
		ARGUMENT,
		INDIRECT_SHADER,
		INDIRECT_COMPUTE,
	};
	
	template <ENCODER_TYPE enc_type>
	using encoder_selector_t =
		std::conditional_t<(enc_type == ENCODER_TYPE::COMPUTE), id <MTLComputeCommandEncoder>,
		std::conditional_t<(enc_type == ENCODER_TYPE::SHADER), id <MTLRenderCommandEncoder>,
		std::conditional_t<(enc_type == ENCODER_TYPE::ARGUMENT), id <MTLArgumentEncoder>,
		std::conditional_t<(enc_type == ENCODER_TYPE::INDIRECT_COMPUTE), id <MTLIndirectComputeCommand>,
		std::conditional_t<(enc_type == ENCODER_TYPE::INDIRECT_SHADER), id <MTLIndirectRenderCommand>,
						   void>>>>>;
	
	template <ENCODER_TYPE enc_type>
	using pipeline_selector_t =
		std::conditional_t<(enc_type == ENCODER_TYPE::COMPUTE), MTLComputePipelineDescriptor,
		std::conditional_t<(enc_type == ENCODER_TYPE::SHADER), MTLRenderPipelineDescriptor,
						   void>>;
	
	struct idx_handler {
		//! actual argument index (directly corresponding to the C++ source code)
		uint32_t arg { 0 };
		//! flag if this is an implicit arg
		bool is_implicit { false };
		//! current implicit argument index
		uint32_t implicit { 0 };
		//! current buffer index
		uint32_t buffer_idx { 0 };
		//! current texture index
		uint32_t texture_idx { 0 };
		//! current function entry
		uint32_t entry { 0 };
	};

	//! return the argument buffer index for the specified buffer index
	static inline uint32_t arg_buffer_index(const idx_handler& idx, const std::vector<uint32_t>* arg_buffer_indices) {
		if (arg_buffer_indices) {
			if (idx.arg < arg_buffer_indices->size()) {
				return (*arg_buffer_indices)[idx.arg];
			}
#if defined(FLOOR_DEBUG)
			log_error("arg index $ > size of arg buffer indices $", idx.arg, arg_buffer_indices->size());
#endif
		}
		return idx.buffer_idx;
	}
	
	//! actual function argument setters
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder, const function_info& entry,
									const void* ptr, const size_t& size,
									const std::vector<uint32_t>* arg_buffer_indices) {
		if constexpr (enc_type == ENCODER_TYPE::COMPUTE) {
			[encoder setBytes:ptr length:size atIndex:idx.buffer_idx];
		} else if constexpr (enc_type == ENCODER_TYPE::SHADER) {
			if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				[encoder setVertexBytes:ptr length:size atIndex:idx.buffer_idx];
			} else {
				[encoder setFragmentBytes:ptr length:size atIndex:idx.buffer_idx];
			}
		} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
			memcpy([encoder constantDataAtIndex:arg_buffer_index(idx, arg_buffer_indices)], ptr, size);
		} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
			log_error("can not encode a raw value into an indirect compute/render command");
#endif
		}
	}
	
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder, const function_info& entry,
									const device_buffer* arg,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		const auto mtl_buffer = arg->get_underlying_metal_buffer_safe();
		auto mtl_buffer_obj = mtl_buffer->get_metal_buffer();
		const auto is_read_only = (entry.args[idx.arg].access == ARG_ACCESS::READ);
		// NOTE: we can only ignore heap-allocated buffers when their access is read-only
		//       -> otherwise they are writable and we must mark them as read+write
		const auto ignore_heap_alloc = (is_read_only && mtl_buffer->is_heap_allocated());
		if constexpr (enc_type == ENCODER_TYPE::COMPUTE) {
			[encoder setBuffer:mtl_buffer_obj
						offset:0
					   atIndex:idx.buffer_idx];
		} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE) {
			[encoder setKernelBuffer:mtl_buffer_obj
							  offset:0
							 atIndex:idx.buffer_idx];
			if (!ignore_heap_alloc) {
				if (is_read_only) {
					res_info->read_only.emplace_back(mtl_buffer_obj);
				} else {
					res_info->read_write.emplace_back(mtl_buffer_obj);
				}
			}
		} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
			[encoder setBuffer:mtl_buffer_obj
						offset:0
					   atIndex:arg_buffer_index(idx, arg_buffer_indices)];
			if (!ignore_heap_alloc) {
				if (is_read_only) {
					res_info->read_only.emplace_back(mtl_buffer_obj);
				} else {
					res_info->read_write.emplace_back(mtl_buffer_obj);
				}
			}
		} else if constexpr (enc_type == ENCODER_TYPE::SHADER || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
			if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				[encoder setVertexBuffer:mtl_buffer_obj
								  offset:0
								 atIndex:idx.buffer_idx];
			} else {
				[encoder setFragmentBuffer:mtl_buffer_obj
									offset:0
								   atIndex:idx.buffer_idx];
			}
			if constexpr (enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
				if (!ignore_heap_alloc) {
					if (is_read_only) {
						res_info->read_only.emplace_back(mtl_buffer_obj);
					} else {
						res_info->read_write.emplace_back(mtl_buffer_obj);
					}
				}
			}
		}
	}
	
	template <ENCODER_TYPE enc_type, typename buffer_array_elem_type> requires(enc_type == ENCODER_TYPE::ARGUMENT)
	static inline void set_argument_buffer_array(const idx_handler& idx,
												 encoder_selector_t<enc_type> encoder,
												 const function_info& entry,
												 const std::vector<buffer_array_elem_type>& arg,
												 const device& dev,
												 const std::vector<uint32_t>* arg_buffer_indices,
												 metal_resource_tracking::resource_info_t* res_info) {
		const auto count = arg.size();
		if (count < 1) {
			return;
		}
		
		std::vector<id <MTLBuffer>> mtl_buf_array(count, nil);
		std::vector<id <MTLBuffer>> mtl_buf_array_for_res_info;
		mtl_buf_array_for_res_info.reserve(count);
		std::vector<NSUInteger> offsets(count, 0);
		const auto is_read_only = (entry.args[idx.arg].access == ARG_ACCESS::READ);
		for (size_t i = 0; i < count; ++i) {
			if (arg[i]) {
				const auto mtl_buffer = arg[i]->get_underlying_metal_buffer_safe();
				mtl_buf_array[i] = mtl_buffer->get_metal_buffer();
				const auto ignore_heap_alloc = (is_read_only && mtl_buffer->is_heap_allocated());
				if (res_info && !ignore_heap_alloc) {
					mtl_buf_array_for_res_info.emplace_back(mtl_buf_array[i]);
				}
			} else {
				const auto null_buffer = (const metal_buffer*)metal_buffer::get_null_buffer(dev);
				assert(null_buffer);
				mtl_buf_array[i] = null_buffer->get_metal_buffer();
			}
		}
		
		[encoder setBuffers:mtl_buf_array.data()
					offsets:offsets.data()
				  withRange:NSRange { arg_buffer_index(idx, arg_buffer_indices), count }];
		if (!mtl_buf_array_for_res_info.empty()) {
			if (is_read_only) {
				res_info->read_only.insert(res_info->read_only.end(), mtl_buf_array_for_res_info.begin(), mtl_buf_array_for_res_info.end());
			} else {
				res_info->read_write.insert(res_info->read_write.end(), mtl_buf_array_for_res_info.begin(), mtl_buf_array_for_res_info.end());
			}
		}
	}

	template <ENCODER_TYPE enc_type> requires(enc_type == ENCODER_TYPE::ARGUMENT)
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const std::vector<std::shared_ptr<device_buffer>>& arg,
									const device& dev,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		set_argument_buffer_array<enc_type, std::shared_ptr<device_buffer>>(idx, encoder, entry, arg, dev, arg_buffer_indices, res_info);
	}
	
	template <ENCODER_TYPE enc_type> requires(enc_type == ENCODER_TYPE::ARGUMENT)
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const std::vector<device_buffer*>& arg,
									const device& dev,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		set_argument_buffer_array<enc_type, device_buffer*>(idx, encoder, entry, arg, dev, arg_buffer_indices, res_info);
	}
	
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const argument_buffer* arg_buf,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		const auto buf = arg_buf->get_storage_buffer();
		const metal_buffer* mtl_buffer = buf->get_underlying_metal_buffer_safe();
		// NOTE: argument buffers are currently always read-only
		const auto is_heap_alloc = mtl_buffer->is_heap_allocated();
		auto mtl_buffer_obj = mtl_buffer->get_metal_buffer();
		if constexpr (enc_type == ENCODER_TYPE::COMPUTE) {
			[encoder setBuffer:mtl_buffer_obj
						offset:0
					   atIndex:idx.buffer_idx];
			((const metal_argument_buffer*)arg_buf)->make_resident(encoder);
		} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE) {
			[encoder setKernelBuffer:mtl_buffer_obj
							  offset:0
							 atIndex:idx.buffer_idx];
			if (!is_heap_alloc) {
				res_info->read_only.emplace_back(mtl_buffer_obj);
			}
		} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
			[encoder setBuffer:mtl_buffer_obj
						offset:0
					   atIndex:arg_buffer_index(idx, arg_buffer_indices)];
			if (!is_heap_alloc) {
				res_info->read_only.emplace_back(mtl_buffer_obj);
			}
		} else if constexpr (enc_type == ENCODER_TYPE::SHADER || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
			if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				[encoder setVertexBuffer:mtl_buffer_obj
								  offset:0
								 atIndex:idx.buffer_idx];
				if constexpr (enc_type == ENCODER_TYPE::SHADER) {
					((const metal_argument_buffer*)arg_buf)->make_resident(encoder, entry.type);
				} else {
					if (!is_heap_alloc) {
						res_info->read_only.emplace_back(mtl_buffer_obj);
					}
				}
			} else {
				[encoder setFragmentBuffer:mtl_buffer_obj
									offset:0
								   atIndex:idx.buffer_idx];
				if constexpr (enc_type == ENCODER_TYPE::SHADER) {
					((const metal_argument_buffer*)arg_buf)->make_resident(encoder, FUNCTION_TYPE::FRAGMENT);
				} else {
					if (!is_heap_alloc) {
						res_info->read_only.emplace_back(mtl_buffer_obj);
					}
				}
			}
		}
		
		if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
			res_info->add_resources(((const metal_argument_buffer*)arg_buf)->get_resources());
		}
	}
	
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const device_image* arg,
									const std::vector<uint32_t>* arg_buffer_indices floor_unused,
									metal_resource_tracking::resource_info_t* res_info) {
		if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
			log_error("can not encode an image into an indirect compute/render command");
#endif
			return;
		}
		
		const auto mtl_image = arg->get_underlying_metal_image_safe();
		// NOTE: we can only ignore heap-allocated image if they are never writable, otherwise we must still make them resident explicitly
		const auto ignore_heap_alloc = (mtl_image->is_image_read_only() && mtl_image->is_heap_allocated());
		auto mtl_image_obj = mtl_image->get_metal_image();
		if constexpr (enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::ARGUMENT) {
			[encoder setTexture:mtl_image_obj
						atIndex:idx.texture_idx];
		} else if constexpr (enc_type == ENCODER_TYPE::SHADER) {
			if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				[encoder setVertexTexture:mtl_image_obj
								  atIndex:idx.texture_idx];
			} else {
				[encoder setFragmentTexture:mtl_image_obj
									atIndex:idx.texture_idx];
			}
		}
		
		// if this is a read/write image, add it again (one is read-only, the other is write-only)
		if (entry.args[idx.arg].access == ARG_ACCESS::READ_WRITE) {
			if constexpr (enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::ARGUMENT) {
				[encoder setTexture:mtl_image_obj
							atIndex:(idx.texture_idx + 1)];
				if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
					assert(!ignore_heap_alloc && "read-only image should never be writable");
					if (!ignore_heap_alloc) {
						res_info->read_write_images.emplace_back(mtl_image_obj);
					}
				}
			} else if constexpr (enc_type == ENCODER_TYPE::SHADER) {
				if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
					[encoder setVertexTexture:mtl_image_obj
									  atIndex:(idx.texture_idx + 1)];
				} else {
					[encoder setFragmentTexture:mtl_image_obj
										atIndex:(idx.texture_idx + 1)];
				}
			}
		} else {
			if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
				if (!ignore_heap_alloc) {
					res_info->read_only_images.emplace_back(mtl_image_obj);
				}
			}
		}
	}
	
	template <ENCODER_TYPE enc_type, typename image_array_elem_type>
	static inline void set_argument_image_array(const idx_handler& idx,
												encoder_selector_t<enc_type> encoder,
												const function_info& entry,
												const std::vector<image_array_elem_type>& arg,
												const std::vector<uint32_t>* arg_buffer_indices floor_unused,
												metal_resource_tracking::resource_info_t* res_info) {
		if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
			log_error("can not encode images into an indirect compute/render command");
#endif
			return;
		}
		
		const auto count = arg.size();
		if (count < 1) return;
		
		std::vector<id <MTLTexture>> mtl_img_array(count, nil);
		std::vector<id <MTLTexture>> mtl_img_array_for_res_info;
		mtl_img_array_for_res_info.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			metal_image* mtl_image = (arg[i] ? arg[i]->get_underlying_metal_image_safe() : nullptr);
			mtl_img_array[i] = (mtl_image ? mtl_image->get_metal_image() : nil);
			// NOTE: we can only ignore heap-allocated image if they are never writable, otherwise we must still make them resident explicitly
			if (res_info && mtl_image && (!mtl_image->is_image_read_only() || !mtl_image->is_heap_allocated())) {
				mtl_img_array_for_res_info.emplace_back(mtl_img_array[i]);
			}
		}
	
		if constexpr (enc_type == ENCODER_TYPE::COMPUTE || enc_type == ENCODER_TYPE::ARGUMENT) {
			[encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { idx.texture_idx, count }];
			if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
				if (!mtl_img_array_for_res_info.empty()) {
					res_info->read_only_images.insert(res_info->read_only_images.end(),
													  mtl_img_array_for_res_info.begin(), mtl_img_array_for_res_info.end());
				}
			}
		} else if constexpr (enc_type == ENCODER_TYPE::SHADER) {
			if (entry.type == FUNCTION_TYPE::VERTEX || entry.type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				[encoder setVertexTextures:mtl_img_array.data()
								 withRange:NSRange { idx.texture_idx, count }];
			} else {
				[encoder setFragmentTextures:mtl_img_array.data()
								   withRange:NSRange { idx.texture_idx, count }];
			}
		}
	}
	
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const std::vector<std::shared_ptr<device_image>>& arg,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		set_argument_image_array<enc_type, std::shared_ptr<device_image>>(idx, encoder, entry, arg, arg_buffer_indices, res_info);
	}
	
	template <ENCODER_TYPE enc_type>
	static inline void set_argument(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const function_info& entry,
									const std::vector<device_image*>& arg,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal_resource_tracking::resource_info_t* res_info) {
		set_argument_image_array<enc_type, device_image*>(idx, encoder, entry, arg, arg_buffer_indices, res_info);
	}
	
	//! returns the entry for the current indices and makes sure that stage_input args are ignored
	//! NOTE: for normal use, "print_error_on_failure" is true and prints and error when going out-of-bounds,
	//!       however, there may also be a valid use case (set_buffer_mutability), so this can be set to false
	template <bool print_error_on_failure = true>
	static inline const function_info* arg_pre_handler(const std::vector<const function_info*>& entries, idx_handler& idx) {
		// make sure we have a usable entry
		const function_info* entry = nullptr;
		for (;;) {
			if (idx.entry >= entries.size()) {
				if constexpr (print_error_on_failure) {
					log_error("function entry is out-of-bounds");
				}
				return nullptr;
			}
			
			// get the next non-nullptr entry or use the current one if it's valid
			while (entries[idx.entry] == nullptr) {
				++idx.entry;
				if (idx.entry >= entries.size()) {
					if constexpr (print_error_on_failure) {
						log_error("function entry is out-of-bounds");
					}
					return nullptr;
				}
			}
			entry = entries[idx.entry];
			
			// ignore any stage input args
			while (idx.arg < entry->args.size() && has_flag<ARG_FLAG::STAGE_INPUT>(entry->args[idx.arg].flags)) {
				if (entry->type == FUNCTION_TYPE::TESSELLATION_EVALUATION) {
					// offset buffer index by the amount of vertex attribute buffers
					idx.buffer_idx += entry->args[idx.arg].size;
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
					idx.is_implicit = false;
					idx.implicit = 0;
					idx.buffer_idx = 0;
					idx.texture_idx = 0;
					continue;
				}
			}
			break;
		}
		return entry;
	}
	
	//! increments indicies dependent on the arg
	static inline void arg_post_handler(const function_info& entry, idx_handler& idx) {
		// advance all indices
		if (idx.is_implicit) {
			++idx.implicit;
			// always a buffer for now
			++idx.buffer_idx;
		} else {
			if (entry.args[idx.arg].image_type == ARG_IMAGE_TYPE::NONE) {
				// buffer
				const auto buf_idx_count = (entry.args[idx.arg].is_array() ? entry.args[idx.arg].array_extent : 1u);
				assert(buf_idx_count > 0u);
				idx.buffer_idx += buf_idx_count;
			} else {
				// texture
				const auto tex_arg_count = (entry.args[idx.arg].is_array() ? entry.args[idx.arg].array_extent : 1u);
				assert(tex_arg_count > 0u);
				
				idx.texture_idx += tex_arg_count;
				if (entry.args[idx.arg].access == ARG_ACCESS::READ_WRITE) {
					// read/write images are implemented as two images -> add twice
					idx.texture_idx += tex_arg_count;
				}
			}
		}
		// finally
		++idx.arg;
	}
	
	//! sets and handles all arguments in the compute/vertex/fragment function
	//! NOTE: ensure this is enclosed in an @autoreleasepool when called!
	template <ENCODER_TYPE enc_type>
	bool set_and_handle_arguments(const device& dev,
								  encoder_selector_t<enc_type> encoder,
								  const std::vector<const function_info*>& entries,
								  const std::vector<device_function_arg>& args,
								  const std::vector<device_function_arg>& implicit_args,
								  const std::vector<uint32_t>* arg_buffer_indices = nullptr,
								  metal_resource_tracking::resource_info_t* res_info = nullptr) {
		const size_t arg_count = args.size() + implicit_args.size();
		idx_handler idx;
		size_t explicit_idx = 0, implicit_idx = 0;
		for (size_t i = 0; i < arg_count; ++i) {
			auto entry = arg_pre_handler(entries, idx);
			if (entry == nullptr) {
				return false;
			}
			const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
			
			if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, *buf_ptr, arg_buffer_indices, res_info);
			} else if (auto vec_buf_ptrs = get_if<const std::vector<device_buffer*>*>(&arg.var)) {
				if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
					set_argument<enc_type>(idx, encoder, *entry, **vec_buf_ptrs, dev, arg_buffer_indices, res_info);
				} else {
					log_error("buffer arrays are only supported for argument buffers");
					return false;
				}
			} else if (auto vec_buf_sptrs = get_if<const std::vector<std::shared_ptr<device_buffer>>*>(&arg.var)) {
				if constexpr (enc_type == ENCODER_TYPE::ARGUMENT) {
					set_argument<enc_type>(idx, encoder, *entry, **vec_buf_sptrs, dev, arg_buffer_indices, res_info);
				} else {
					log_error("buffer arrays are only supported for argument buffers");
					return false;
				}
			} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, *img_ptr, arg_buffer_indices, res_info);
			} else if (auto vec_img_ptrs = get_if<const std::vector<device_image*>*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, **vec_img_ptrs, arg_buffer_indices, res_info);
			} else if (auto vec_img_sptrs = get_if<const std::vector<std::shared_ptr<device_image>>*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, **vec_img_sptrs, arg_buffer_indices, res_info);
			} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, *arg_buf_ptr, arg_buffer_indices, res_info);
			} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
				set_argument<enc_type>(idx, encoder, *entry, *generic_arg_ptr, arg.size, arg_buffer_indices);
			} else {
				log_error("encountered invalid arg");
				return false;
			}
			
			arg_post_handler(*entry, idx);
		}
		return true;
	}
	
	//! sets the buffer mutability of all buffers of the specified "entries" in the specified pipeline descriptor
	template <ENCODER_TYPE enc_type>
	bool set_buffer_mutability(pipeline_selector_t<enc_type>* pipeline_desc,
							   const std::vector<const function_info*>& entries) {
		idx_handler idx;
		for (;;) {
			auto entry = arg_pre_handler<false>(entries, idx);
			if (entry == nullptr) {
				break; // we're done here
			}
			
			const auto& arg = entry->args[idx.arg];
			if (has_flag<ARG_FLAG::STAGE_INPUT>(arg.flags) ||
				arg.image_type != ARG_IMAGE_TYPE::NONE ||
				(arg.address_space != ARG_ADDRESS_SPACE::GLOBAL &&
				 arg.address_space != ARG_ADDRESS_SPACE::CONSTANT)) {
				// ignore non-buffer args
			} else {
				// -> buffer
				const auto mutability = (arg.access == ARG_ACCESS::READ ? MTLMutability::MTLMutabilityImmutable : MTLMutability::MTLMutabilityMutable);
				const auto buf_count = (entry->args[idx.arg].is_array() ? entry->args[idx.arg].array_extent : 1u);
				if constexpr (enc_type == ENCODER_TYPE::COMPUTE) {
					for (uint32_t i = 0; i < buf_count; ++i) {
						pipeline_desc.buffers[idx.buffer_idx + i].mutability = mutability;
					}
				} else {
					if (entry->type == FUNCTION_TYPE::FRAGMENT) {
						for (uint32_t i = 0; i < buf_count; ++i) {
							pipeline_desc.fragmentBuffers[idx.buffer_idx + i].mutability = mutability;
						}
					} else {
						for (uint32_t i = 0; i < buf_count; ++i) {
							pipeline_desc.vertexBuffers[idx.buffer_idx + i].mutability = mutability;
						}
					}
				}
			}
			
			arg_post_handler(*entry, idx);
		}
		return true;
	}
	
} // namespace metal_args
} // namespace fl
