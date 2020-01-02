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

#include <floor/compute/llvm_toolchain.hpp>
using namespace llvm_toolchain;

//! Metal compute/vertex/fragment argument handler/setter
//! NOTE: do not include manually
namespace metal_args {
	enum class FUNCTION_TYPE {
		COMPUTE,
		SHADER,
	};
	
	template <FUNCTION_TYPE func_type>
	using encoder_selector_t = conditional_t<(func_type == FUNCTION_TYPE::COMPUTE), id <MTLComputeCommandEncoder>, id <MTLRenderCommandEncoder>>;
	
	struct idx_handler {
		//! actual argument index (directly corresponding to the c++ source code)
		uint32_t arg { 0 };
		//! flag if this is an implicit arg
		bool is_implicit { false };
		//! current implicit argument index
		uint32_t implicit { 0 };
		//! current buffer index
		uint32_t buffer_idx { 0 };
		//! current texture index
		uint32_t texture_idx { 0 };
		//! current kernel/shader entry
		uint32_t entry { 0 };
	};
	
	//! actual kernel argument setters
	template <FUNCTION_TYPE func_type>
	static void set_argument(const idx_handler& idx,
							 encoder_selector_t<func_type> encoder, const compute_kernel::kernel_entry& entry,
							 const void* ptr, const size_t& size) {
		if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
			[encoder setBytes:ptr length:size atIndex:idx.buffer_idx];
		} else {
			if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
				[encoder setVertexBytes:ptr length:size atIndex:idx.buffer_idx];
			} else {
				[encoder setFragmentBytes:ptr length:size atIndex:idx.buffer_idx];
			}
		}
	}
	
	template <FUNCTION_TYPE func_type>
	static void set_argument(const idx_handler& idx,
							 encoder_selector_t<func_type> encoder, const compute_kernel::kernel_entry& entry,
							 const compute_buffer* arg) {
		const metal_buffer* mtl_buffer = nullptr;
		if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(arg->get_flags())) {
			mtl_buffer = arg->get_shared_metal_buffer();
			if (mtl_buffer == nullptr) {
				mtl_buffer = (const metal_buffer*)arg;
#if defined(FLOOR_DEBUG)
				if (auto test_cast_mtl_buffer = dynamic_cast<const metal_buffer*>(arg); !test_cast_mtl_buffer) {
					log_error("specified buffer is neither a Metal buffer nor a shared Metal buffer");
					return;
				}
#endif
			} else {
				if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING_SYNC_SHARED>(arg->get_flags())) {
					arg->sync_metal_buffer();
				}
			}
		} else {
			mtl_buffer = (const metal_buffer*)arg;
		}
		
		if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
			[encoder setBuffer:mtl_buffer->get_metal_buffer()
						offset:0
					   atIndex:idx.buffer_idx];
		} else {
			if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
				[encoder setVertexBuffer:mtl_buffer->get_metal_buffer()
								  offset:0
								 atIndex:idx.buffer_idx];
			} else {
				[encoder setFragmentBuffer:mtl_buffer->get_metal_buffer()
									offset:0
								   atIndex:idx.buffer_idx];
			}
		}
	}
	
	template <FUNCTION_TYPE func_type>
	static void set_argument(const idx_handler& idx,
							 encoder_selector_t<func_type> encoder, const compute_kernel::kernel_entry& entry,
							 const compute_image* arg) {
		const metal_image* mtl_image = nullptr;
		if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING>(arg->get_flags())) {
			mtl_image = arg->get_shared_metal_image();
			if (mtl_image == nullptr) {
				mtl_image = (const metal_image*)arg;
#if defined(FLOOR_DEBUG)
				if (auto test_cast_mtl_image = dynamic_cast<const metal_image*>(arg); !test_cast_mtl_image) {
					log_error("specified image is neither a Metal image nor a shared Metal image");
					return;
				}
#endif
			} else {
				if (has_flag<COMPUTE_MEMORY_FLAG::METAL_SHARING_SYNC_SHARED>(arg->get_flags())) {
					arg->sync_metal_image();
				}
			}
		} else {
			mtl_image = (const metal_image*)arg;
		}
		
		
		if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
			[encoder setTexture:mtl_image->get_metal_image()
						atIndex:idx.texture_idx];
		} else {
			if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
				[encoder setVertexTexture:mtl_image->get_metal_image()
								  atIndex:idx.texture_idx];
			} else {
				[encoder setFragmentTexture:mtl_image->get_metal_image()
									atIndex:idx.texture_idx];
			}
		}
		
		// if this is a read/write image, add it again (one is read-only, the other is write-only)
		if (entry.info->args[idx.arg].image_access == function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
				[encoder setTexture:mtl_image->get_metal_image()
							atIndex:(idx.texture_idx + 1)];
			} else {
				if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
					[encoder setVertexTexture:mtl_image->get_metal_image()
									  atIndex:(idx.texture_idx + 1)];
				} else {
					[encoder setFragmentTexture:mtl_image->get_metal_image()
										atIndex:(idx.texture_idx + 1)];
				}
			}
		}
	}
	
	template <FUNCTION_TYPE func_type>
	static void set_argument(const idx_handler& idx,
							 encoder_selector_t<func_type> encoder, const compute_kernel::kernel_entry& entry,
							 const vector<shared_ptr<compute_image>>& arg) {
		const auto count = arg.size();
		if (count < 1) return;
		
		vector<id <MTLTexture>> mtl_img_array(count, nil);
		for (size_t i = 0; i < count; ++i) {
			mtl_img_array[i] = ((metal_image*)arg[i].get())->get_metal_image();
		}
		
		if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
			[encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { idx.texture_idx, count }];
		} else {
			if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
				[encoder setVertexTextures:mtl_img_array.data()
								 withRange:NSRange { idx.texture_idx, count }];
			} else {
				[encoder setFragmentTextures:mtl_img_array.data()
								   withRange:NSRange { idx.texture_idx, count }];
			}
		}
	}
	
	template <FUNCTION_TYPE func_type>
	static void set_argument(const idx_handler& idx,
							 encoder_selector_t<func_type> encoder, const compute_kernel::kernel_entry& entry,
							 const vector<compute_image*>& arg) {
		const auto count = arg.size();
		if (count < 1) return;
		
		vector<id <MTLTexture>> mtl_img_array(count, nil);
		for (size_t i = 0; i < count; ++i) {
			mtl_img_array[i] = ((metal_image*)arg[i])->get_metal_image();
		}
		
		if constexpr (func_type == FUNCTION_TYPE::COMPUTE) {
			[encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { idx.texture_idx, count }];
		} else {
			if (entry.info->type == function_info::FUNCTION_TYPE::VERTEX) {
				[encoder setVertexTextures:mtl_img_array.data()
								 withRange:NSRange { idx.texture_idx, count }];
			} else {
				[encoder setFragmentTextures:mtl_img_array.data()
								   withRange:NSRange { idx.texture_idx, count }];
			}
		}
	}
	
	//! returns the entry for the current indices and makes sure that stage_input args are ignored
	static inline const compute_kernel::kernel_entry* arg_pre_handler(const vector<const compute_kernel::kernel_entry*>& entries, idx_handler& idx) {
		// make sure we have a usable entry
		const compute_kernel::kernel_entry* entry = nullptr;
		for (;;) {
			// get the next non-nullptr entry or use the current one if it's valid
			while (entries[idx.entry] == nullptr) {
				++idx.entry;
#if defined(FLOOR_DEBUG)
				if (idx.entry >= entries.size()) {
					log_error("shader/kernel entry out of bounds");
					return nullptr;
				}
#endif
			}
			entry = entries[idx.entry];
			
			// ignore any stage input args
			while (idx.arg < entry->info->args.size() &&
				   entry->info->args[idx.arg].special_type == function_info::SPECIAL_TYPE::STAGE_INPUT) {
				++idx.arg;
			}
			
			// have all args been specified for this entry?
			if (idx.arg >= entry->info->args.size()) {
				// implicit args at the end
				const auto implicit_arg_count = (function_info::has_flag<function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags) ? 1u : 0u);
				if (idx.arg < entry->info->args.size() + implicit_arg_count) {
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
	static inline void arg_post_handler(const compute_kernel::kernel_entry& entry, idx_handler& idx, const compute_kernel_arg& arg) {
		// advance all indices
		if (idx.is_implicit) {
			++idx.implicit;
			// always a buffer for now
			++idx.buffer_idx;
		} else {
			if (entry.info->args[idx.arg].image_type == function_info::ARG_IMAGE_TYPE::NONE) {
				// buffer
				++idx.buffer_idx;
			} else {
				// texture
				size_t tex_arg_count = 0;
				if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
					tex_arg_count = (*vec_img_ptrs)->size();
				} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
					tex_arg_count = (*vec_img_sptrs)->size();
				} else {
					tex_arg_count = 1;
				}
				
				idx.texture_idx += tex_arg_count;
				if (entry.info->args[idx.arg].image_access == function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
					// read/write images are implemented as two images -> add twice
					idx.texture_idx += tex_arg_count;
				}
			}
		}
		// finally
		++idx.arg;
	}
	
	//! sets and handles all arguments in the compute/vertex/fragment function
	template <FUNCTION_TYPE func_type>
	bool set_and_handle_arguments(encoder_selector_t<func_type> encoder,
								  const vector<const compute_kernel::kernel_entry*>& entries,
								  const vector<compute_kernel_arg>& args,
								  const vector<compute_kernel_arg>& implicit_args) {
		const size_t arg_count = args.size() + implicit_args.size();
		idx_handler idx;
		size_t explicit_idx = 0, implicit_idx = 0;
		for (size_t i = 0; i < arg_count; ++i) {
			auto entry = arg_pre_handler(entries, idx);
			const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
			
			if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
				set_argument<func_type>(idx, encoder, *entry, *buf_ptr);
			} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
				set_argument<func_type>(idx, encoder, *entry, *img_ptr);
			} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
				set_argument<func_type>(idx, encoder, *entry, **vec_img_ptrs);
			} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
				set_argument<func_type>(idx, encoder, *entry, **vec_img_sptrs);
			} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
				set_argument<func_type>(idx, encoder, *entry, *generic_arg_ptr, arg.size);
			} else {
				log_error("encountered invalid arg");
				return false;
			}
			
			arg_post_handler(*entry, idx, arg);
		}
		return true;
	}
	
} // namespace metal_args
