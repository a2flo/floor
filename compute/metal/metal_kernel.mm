/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/compute/metal/metal_kernel.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_device.hpp>

struct metal_kernel::metal_encoder {
	id <MTLCommandBuffer> cmd_buffer { nil };
	id <MTLComputeCommandEncoder> encoder { nil };
};

static unique_ptr<metal_kernel::metal_encoder> create_encoder(const compute_queue& cqueue, const metal_kernel::metal_kernel_entry& entry) {
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	auto ret = make_unique<metal_kernel::metal_encoder>(metal_kernel::metal_encoder { cmd_buffer, [cmd_buffer computeCommandEncoder] });
	[ret->encoder setComputePipelineState:(__bridge id <MTLComputePipelineState>)entry.kernel_state];
	return ret;
}

metal_kernel::metal_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

static constexpr const uint32_t printf_buffer_size { 1 * 1024 * 1024 };
static constexpr const uint32_t printf_buffer_header_size { 2 * sizeof(uint32_t) };
static void handle_printf_buffer(const unique_ptr<uint32_t[]>& buf) {
	const uint32_t total_size = buf[1];
	if (total_size != printf_buffer_size) {
		log_error("device printf has overwritten printf buffer size!");
		return;
	}
	const uint32_t bytes_written = min(buf[0], printf_buffer_size);
	if (bytes_written <= printf_buffer_header_size) {
		return; // nothing was written
	}
	
	// handle/decode printf buffer
	const uint32_t* buf_ptr = &buf[2];
	do {
		const auto cur_size = uintptr_t(buf_ptr) - uintptr_t(&buf[0]);
		if (cur_size >= bytes_written) {
			break; // done
		}
		
		const uint32_t entry_size = *buf_ptr;
		if (entry_size == 0) {
			log_error("printf entry with 0 size");
			break;
		}
		if (entry_size % 4u != 0u) {
			log_error("invalid entry size: %u (expected multiple of 4)", entry_size);
			break;
		}
		
		if (cur_size + entry_size > bytes_written) {
			log_error("out-of-bounds entry: total %u, entry: %u", bytes_written, cur_size + entry_size);
			break;
		}
		uint32_t entry_bytes_parsed = 4;
		
		// get format string
		string format_str;
		const char* format_ptr = (const char*)(buf_ptr + 1);
		while (*format_ptr != '\0' && entry_bytes_parsed < entry_size) {
			format_str += *format_ptr++;
			++entry_bytes_parsed;
		}
		// round to next multiple of 4
		switch (entry_bytes_parsed % 4u) {
			case 3: entry_bytes_parsed += 1; break;
			case 2: entry_bytes_parsed += 2; break;
			case 1: entry_bytes_parsed += 3; break;
			default: break;
		}
		
		// get args
		// NOTE: we only support 32-bit values right now
		union printf_arg_t {
			uint32_t u32;
			int32_t i32;
			float f32;
		};
		vector<printf_arg_t> args;
		const uint32_t* arg_ptr = buf_ptr + entry_bytes_parsed / 4;
		while (entry_bytes_parsed < entry_size) {
			args.emplace_back(printf_arg_t { .u32 = *arg_ptr++ });
			entry_bytes_parsed += 4;
		}
		
		// print
		stringstream sstr;
		bool is_invalid = false;
		auto cur_arg = args.cbegin();
		for (auto ch = format_str.cbegin(); ch != format_str.cend(); ) {
			if (*ch != '%') {
				sstr << *ch++;
				continue;
			}
			
			// must have an arg for this format specifier
			if (cur_arg == args.cend()) {
				is_invalid = true;
				log_error("insufficient #args for printf");
				break;
			}
			
			// parse and handle format specifier
			if (++ch == format_str.cend()) {
				log_error("premature end of format string after '%%'");
				is_invalid = true;
				break;
			}
			bool is_done = true;
			do {
				is_done = true;
				const auto cur_ch = *ch;
				switch (*ch) {
					case '%':
						sstr << '%';
						break;
					case 'u': // unsigned integer
						sstr << cur_arg->u32;
						break;
					case 'd':
					case 'i': // integer
						sstr << cur_arg->i32;
						break;
					case 'X':
					case 'x': // hex
						sstr << hex;
						if (cur_ch == 'X') sstr << uppercase;
						sstr << cur_arg->u32;
						if (cur_ch == 'X') sstr << nouppercase;
						sstr << dec;
						break;
					case 'o': // octal
						sstr << oct;
						sstr << cur_arg->u32;
						sstr << dec;
						break;
					case 'F':
					case 'f': // float
						sstr << cur_arg->f32;
						break;
					case 'j': // *intmax
					case 'z': // size_t
					case 't': // ptrdiff
					case 'L': // long double
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after '%c'", cur_ch);
							is_invalid = true;
							break;
						}
						is_done = false;
						break;
					case 'l': // long...
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after 'l'");
							is_invalid = true;
							break;
						}
						if (*ch == 'l') {
							if (++ch == format_str.cend()) {
								log_error("premature end of format string after 'll'");
								is_invalid = true;
								break;
							}
							if (*ch == 'l') {
								log_error("'lll' format specified is invalid");
								is_invalid = true;
								break;
							}
						}
						is_done = false;
						break;
					case 'h': // short...
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after 'h'");
							is_invalid = true;
							break;
						}
						if (*ch == 'h') {
							if (++ch == format_str.cend()) {
								log_error("premature end of format string after 'hh'");
								is_invalid = true;
								break;
							}
							if (*ch == 'h') {
								log_error("'hhh' format specified is invalid");
								is_invalid = true;
								break;
							}
						}
						is_done = false;
						break;
					case 'c':
					case 's':
					case 'E':
					case 'e':
					case 'A':
					case 'a':
					case 'G':
					case 'g':
					case 'n':
					case 'p':
						log_error("unsupported format specifier: %c", *ch);
						is_invalid = true;
						break;
					default:
						log_error("unknown/invalid format specifier: %c", *ch);
						is_invalid = true;
						break;
				}
				if (is_invalid) break;
			} while(!is_done);
			if (is_invalid) break;
			
			++cur_arg;
			++ch;
		}
		if (!is_invalid) {
			printf("%s", sstr.str().c_str());
		}
		
		// done
		buf_ptr += entry_size / 4;
	} while(true);
}

void metal_kernel::execute(const compute_queue& cqueue,
						   const bool& is_cooperative,
						   const uint32_t& dim,
						   const uint3& global_work_size,
						   const uint3& local_work_size,
						   const vector<compute_kernel_arg>& args) const {
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Metal");
		return;
	}
	
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
	if(kernel_iter == kernels.cend()) {
		log_error("no kernel for this compute queue/device exists!");
		return;
	}
	
	// check work size (NOTE: will set elements to at least 1)
	const auto block_dim = check_local_work_size(kernel_iter->second, local_work_size);
	
	//
	auto encoder = create_encoder(cqueue, kernel_iter->second);
	
	// set and handle kernel arguments
	uint32_t total_idx = 0, buffer_idx = 0, texture_idx = 0;
	const kernel_entry& entry = kernel_iter->second;
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, *buf_ptr);
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, *img_ptr);
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, **vec_img_ptrs);
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, **vec_img_sptrs);
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_const_argument(*encoder, buffer_idx, *generic_arg_ptr, arg.size);
		} else {
			log_error("encountered invalid arg");
			return;
		}
		++total_idx;
	}
	
	// create + init printf buffer if this function uses soft-printf
	shared_ptr<compute_buffer> printf_buffer;
	if (llvm_toolchain::function_info::has_flag<llvm_toolchain::function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		printf_buffer = cqueue.get_device().context->create_buffer(cqueue, printf_buffer_size);
		printf_buffer->write_from(uint2 { printf_buffer_header_size, printf_buffer_size }, cqueue);
		set_kernel_argument(total_idx++, buffer_idx, texture_idx, *encoder, entry, printf_buffer.get());
	}
	
	// run
	const uint3 grid_dim_overflow {
		dim >= 1 && global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		dim >= 2 && global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		dim >= 3 && global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	
	// TODO/NOTE: guarantee that all buffers have finished their prior processing
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	[encoder->encoder dispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	[encoder->encoder endEncoding];
	[encoder->cmd_buffer commit];
	
	// if soft-printf is being used, block/wait for completion here and read-back results
	if (llvm_toolchain::function_info::has_flag<llvm_toolchain::function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		[encoder->cmd_buffer waitUntilCompleted];
		auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(cqueue, cpu_printf_buffer.get());
		handle_printf_buffer(cpu_printf_buffer);
	}
}

typename metal_kernel::kernel_map_type::const_iterator metal_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const metal_device&)cqueue.get_device());
}

void metal_kernel::set_const_argument(metal_encoder& encoder, uint32_t& buffer_idx,
									  const void* ptr, const size_t& size) const {
	[encoder.encoder setBytes:ptr length:size atIndex:buffer_idx++];
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t& buffer_idx, uint32_t&,
									   metal_encoder& encoder, const kernel_entry&,
									   const compute_buffer* arg) const {
	[encoder.encoder setBuffer:((const metal_buffer*)arg)->get_metal_buffer()
						offset:0
					   atIndex:buffer_idx++];
}

void metal_kernel::set_kernel_argument(const uint32_t total_idx, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry& entry,
									   const compute_image* arg) const {
	[encoder.encoder setTexture:((const metal_image*)arg)->get_metal_image()
						atIndex:texture_idx++];
	
	// if this is a read/write image, add it again (one is read-only, the other is write-only)
	if(entry.info->args[total_idx].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
		[encoder.encoder setTexture:((const metal_image*)arg)->get_metal_image()
							atIndex:texture_idx++];
	}
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry&,
									   const vector<shared_ptr<compute_image>>& arg) const {
	const auto count = arg.size();
	if(count < 1) return;
	
	vector<id <MTLTexture>> mtl_img_array(count, nil);
	for(size_t i = 0; i < count; ++i) {
		mtl_img_array[i] = ((metal_image*)arg[i].get())->get_metal_image();
	}
	
	[encoder.encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { texture_idx, count }];
	texture_idx += count;
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry&,
									   const vector<compute_image*>& arg) const {
	const auto count = arg.size();
	if(count < 1) return;
	
	vector<id <MTLTexture>> mtl_img_array(count, nil);
	for(size_t i = 0; i < count; ++i) {
		mtl_img_array[i] = ((metal_image*)arg[i])->get_metal_image();
	}
	
	[encoder.encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { texture_idx, count }];
	texture_idx += count;
}

const compute_kernel::kernel_entry* metal_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const metal_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

#endif
