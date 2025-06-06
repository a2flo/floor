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

#include <floor/device/cuda/cuda_program.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/cuda/cuda_function.hpp>
#include <floor/device/cuda/cuda_device.hpp>

namespace fl {

// note that CUDA doesn't have any special argument types and everything is just sized "memory"
// -> only need to add up sizes
static size_t device_function_args_size(const toolchain::function_info& info) {
	size_t ret = 0;
	const auto arg_count = info.args.size();
	for(size_t i = 0; i < arg_count; ++i) {
		// actual arg or pointer?
		if(info.args[i].address_space == toolchain::ARG_ADDRESS_SPACE::CONSTANT) {
			ret += info.args[i].size;
		}
		else if(info.args[i].address_space == toolchain::ARG_ADDRESS_SPACE::IMAGE) {
			ret += sizeof(uint32_t) * cuda_sampler::max_sampler_count;
			ret += sizeof(uint64_t) * 1 /* surface */;
			ret += sizeof(cu_device_ptr) * 1 /* surfaces lod buffer */;
			ret += sizeof(IMAGE_TYPE);
			ret += 4 /* padding */;
		}
		else ret += sizeof(void*);
	}
	return ret;
}

cuda_program::cuda_program(program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
#if 0 // for debugging/testing purposes
	for (auto&& prog : programs) {
		uint32_t func_count = 0u;
		CU_CALL_CONT(cu_module_get_function_count(&func_count, prog.second.program),
					 "failed to retrieve function count")
		std::vector<cu_function> funcs(func_count, nullptr);
		CU_CALL_CONT(cu_module_enumerate_functions(funcs.data(), func_count, prog.second.program),
					 "failed to enumerate functions")
		log_msg("CUDA module: expected $' functions, got $'", prog.second.functions.size(), func_count);
		for (auto& func : funcs) {
			const char* func_name = nullptr;
			CU_CALL_CONT(cu_func_get_name(&func_name, func), "failed to query function name")
			log_msg("CUDA function: loading \"$\" ...", func_name ? func_name : "<invalid-func-name>");
			CU_CALL_CONT(cu_func_load(func), "function load finalize failed")
		}
	}
#endif
	
	// create all functions of all device programs
	// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
	functions.reserve(function_names.size());
	for (const auto& function_name : function_names) {
		cuda_function::function_map_type function_map;
		for (auto&& prog : programs) {
			if (!prog.second.valid) continue;
			for (const auto& info : prog.second.functions) {
				if (info.name == function_name) {
					if (should_ignore_function_for_device(*prog.first, info)) {
						continue;
					}
					
					cuda_function::cuda_function_entry entry;
					entry.info = &info;
					entry.function_args_size = device_function_args_size(info);
					entry.max_local_size = prog.first->max_local_size;
					
					CU_CALL_CONT(cu_module_get_function(&entry.function, prog.second.program, function_name.c_str()),
								 "failed to get function \"" + function_name + "\"")
					
					// retrieve max local work size for this kernel for this device
					int max_total_local_size = 0;
					CU_CALL_IGNORE(cu_function_get_attribute(&max_total_local_size,
															 CU_FUNCTION_ATTRIBUTE::MAX_THREADS_PER_BLOCK, entry.function))
					entry.max_total_local_size = (max_total_local_size < 0 ? 0 : (uint32_t)max_total_local_size);
					if (info.has_valid_required_local_size()) {
						// check and update local size if a required local size was specified
						const auto req_local_size = info.required_local_size.maxed(1u).extent();
						if (req_local_size > entry.max_total_local_size) {
							log_error("in kernel $: supported total local size $' is < required total local size $' ($)",
									  function_name, entry.max_total_local_size, req_local_size, info.required_local_size);
							continue;
						}
						entry.max_total_local_size = req_local_size;
						entry.max_local_size = info.required_local_size;
					}
					
					if (info.has_valid_required_simd_width()) {
						entry.required_simd_width = info.required_simd_width;
						assert(entry.required_simd_width == 32u && "SIMD width should always be 32 for CUDA devices");
					}
					
					// we only support static local/shared memory, not dynamic
					// -> ignore the function if it uses too much memory
					int local_mem_size = 0;
					CU_CALL_IGNORE(cu_function_get_attribute(&local_mem_size,
															 CU_FUNCTION_ATTRIBUTE::LOCAL_SIZE_BYTES, entry.function))
					if (local_mem_size > 0 && uint64_t(local_mem_size) > prog.first->local_mem_size) {
						log_error("kernel $ requires $' bytes of local memory, but the device only provides $' bytes",
								  function_name, uint64_t(local_mem_size), prog.first->local_mem_size);
						break;
					}
					
#if 0 // WIP
					// use this to compute max occupancy
					int min_grid_size = 0, block_size = 0;
					CU_CALL_NO_ACTION(cu_occupancy_max_potential_block_size(&min_grid_size, &block_size, entry.function, nullptr, 0, 0),
									  "failed to compute max potential occupancy");
					log_debug("$ max occupancy: grid size >= $ with block size $", function_name, min_grid_size, block_size);
					
					//
					static const std::array<uint32_t, 6> check_local_sizes {{
						32, 64, 128, 256, 512, 1024
					}};
					for(const auto& local_size : check_local_sizes) {
						int block_count = 0;
						CU_CALL_NO_ACTION(cu_occupancy_max_active_blocks_per_multiprocessor(&block_count, entry.function,
																							int(local_size), 0),
										  "failed to compute max active blocks per mp");
						log_debug("$: #blocks: $ for local-size $", function_name, block_count, local_size);
					}
#endif
					
					// success, insert into map
					function_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		functions.emplace_back(std::make_shared<cuda_function>(function_name, std::move(function_map)));
	}
}

} // namespace fl

#endif
