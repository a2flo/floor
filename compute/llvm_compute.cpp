/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>

#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

static bool process_air_llvm(const string& filename, string& code) {
	code = "";
	
	string lines_str;
	if(!file_io::file_to_string(filename, lines_str)) {
		log_error("failed to process air llvm file: %s", filename);
		return false;
	}
	
	// replace header (module id, data layout, target triple)
	auto lines = core::tokenize(lines_str, '\n');
	lines[0] = "; ModuleID = '" + filename + "'";
	lines[1] = "target datalayout = \"e-i64:64-f80:128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-n8:16:32\"";
	lines[2] = "target triple = \"air64-apple-ios8.0.0\"";

	// concat again for final code output
	for(auto& line : lines) {
		code += line + '\n';
	}
	return true;
}

static bool get_floor_metadata(const string& filename, vector<llvm_compute::kernel_info>& kernels) {
	string lines_str;
	if(!file_io::file_to_string(filename, lines_str)) {
		log_error("failed to process air llvm file: %s", filename);
		return false;
	}
	
	// parses metadata lines of the format: !... = !{!N, !M, !I, !J, ...}
	const auto parse_metadata_line = [](const string& line, const auto& per_elem_func) {
		const auto set_start = line.find('{');
		const auto set_end = line.rfind('}');
		if(set_start != string::npos && set_end != string::npos &&
		   set_start + 1 < set_end) { // not interested in empty sets!
			const auto set_str = core::tokenize(line.substr(set_start + 1, set_end - set_start - 1), ',');
			for(const auto& elem_ws : set_str) {
				auto elem = core::trim(elem_ws); // trim whitespace, just in case
				
#if 1 // llvm 3.5
				if(elem[0] == '!') {
					// ref, just forward
					per_elem_func(elem);
					continue;
				}
				
				const auto ws_pos = elem.find(' ');
				if(ws_pos != string::npos) {
					const auto elem_front = elem.substr(0, ws_pos);
					const auto elem_back = elem.substr(ws_pos + 1, elem.size() - ws_pos - 1);
					if(elem_front == "i32" || elem_front == "i64") {
						// int, forward back
						per_elem_func(elem_back);
						continue;
					}
					else if(elem_front == "metadata") {
						// string
						const auto str_front = elem_back.find('\"'), str_back = elem_back.rfind('\"');
						if(str_front != string::npos && str_back != string::npos && str_back > str_front) {
							per_elem_func(elem_back.substr(str_front + 1, str_back - str_front - 1));
							continue;
						}
					}
				}
#else // llvm 3.6/3.7+
				const auto ws_pos = elem.find(' ');
				if(ws_pos != string::npos) {
					const auto elem_front = elem.substr(0, ws_pos);
					const auto elem_back = elem.substr(ws_pos + 1, elem.size() - ws_pos - 1);
					if(elem_front == "i32" || elem_front == "i64") {
						// int, forward back
						per_elem_func(elem_back);
						continue;
					}
					// else: something else
				}
				else if(elem[0] == '!' && elem.find('\"') != string::npos) {
					// string
					const auto str_front = elem.find('\"'), str_back = elem.rfind('\"');
					if(str_front != string::npos && str_back != string::npos && str_back > str_front) {
						per_elem_func(elem.substr(str_front + 1, str_back - str_front - 1));
						continue;
					}
				}
#endif
				
				// no idea what this is, just forward
				per_elem_func(elem);
			}
		}
	};
	
	// go through all lines and process the found metadata lines
	auto lines = core::tokenize(lines_str, '\n');
	unordered_map<uint64_t, const string*> metadata_refs;
	vector<uint64_t> kernel_refs;
	for(const auto& line : lines) {
		// check if it's a metadata line
		if(line[0] == '!') {
			const string metadata_type = line.substr(1, line.find(' ') - 1);
			if(metadata_type[0] >= '0' && metadata_type[0] <= '9') {
				// !N metadata
				const auto mid = stoull(metadata_type);
				metadata_refs.emplace(mid, &line);
			}
			else if(metadata_type == "floor.kernels") {
				// contains all references to kernels metadata
				// format: !floor.kernels = !{!N, !M, !I, !J, ...}
				parse_metadata_line(line, [&kernel_refs](const string& elem) {
					if(elem[0] == '!') {
						const auto kernel_ref_idx = stoull(elem.substr(1));
						kernel_refs.emplace_back(kernel_ref_idx);
					}
				});
				
				// now that we know the kernel count, alloc enough memory
				kernels.resize(kernel_refs.size());
			}
		}
	}
	
	// parse the individual kernel metadata lines and put the info into the "kernels" vector
	for(size_t i = 0, count = kernel_refs.size(); i < count; ++i) {
		const auto& metadata = *metadata_refs[kernel_refs[i]];
		auto& kernel = kernels[i];
		uint32_t elem_idx = 0;
		parse_metadata_line(metadata, [&elem_idx, &kernel](const string& elem) {
			if(elem_idx == 0) {
				// version check
				static constexpr const int floor_kernels_version { 1 };
				if(stoi(elem) != floor_kernels_version) {
					log_warn("invalid floor.kernels version, expected %u, got %u!",
							 floor_kernels_version, elem);
				}
			}
			else if(elem_idx == 1) {
				// kernel function name
				kernel.name = elem;
			}
			else {
				// arg #elem_idx size + address space
				const auto data = stoull(elem);
				kernel.arg_sizes.emplace_back(data & 0xFFFFFFFF);
				kernel.arg_address_spaces.emplace_back((llvm_compute::kernel_info::ARG_ADDRESS_SPACE)((data >> 32ull) & 0x3));
			}
			++elem_idx;
		});
	}
	
	return true;
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program(shared_ptr<compute_device> device,
																			  const string& code, const string additional_options,
																			  const TARGET target) {
	// TODO/NOTE: additional clang flags:
	//  -vectorize-loops -vectorize-slp -vectorize-slp-aggressive
	//  -menable-unsafe-fp-math
	
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	
	// for now, only enable these in debug mode (note that these cost a not insignificant amount of compilation time)
#if defined(FLOOR_DEBUG)
	const char* warning_flags {
		// let's start with everything
		" -Weverything"
		// remove compat warnings
		" -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c99-extensions -Wno-c11-extensions -Wno-gnu"
		// in case we're using warning options that aren't supported by other clang versions
		" -Wno-unknown-warning-option"
		// really don't want to be too pedantic
		" -Wno-old-style-cast -Wno-date-time -Wno-system-headers -Wno-header-hygiene -Wno-documentation"
		// again: not too pedantic, also useful language features
		" -Wno-nested-anon-types -Wno-global-constructors -Wno-exit-time-destructors"
		// usually conflicting with the other switch/case warning, so disable it
		" -Wno-switch-enum"
		// TODO: also add -Wno-padded -Wno-packed? or make these optional? there are situations were these are useful
		// end
		" "
	};
#else
	const char* warning_flags { "" };
#endif
	
	// generic compilation flags used by all implementations
	// TODO: use debug/profiling config options
	const char* generic_flags {
		" -DFLOOR_COMPUTE"
		" -DFLOOR_NO_MATH_STR"
		" -DPLATFORM_X64"
		" -include floor/compute/device/common.hpp"
		" -include floor/constexpr/const_math.cpp"
		" -isystem /usr/local/include"
		" -m64 -fno-exceptions -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -flto -Ofast "
	};
	
	if(target == TARGET::SPIR) {
		string spir_cmd {
			printable_code +
			floor::get_opencl_compiler() +
			" -x cl -std=gnu++14 -Xclang -cl-std=CL1.2 -target spir64-unknown-unknown" \
			" -Xclang -cl-kernel-arg-info" \
			" -Xclang -cl-mad-enable" \
			" -Xclang -cl-fast-relaxed-math" \
			" -Xclang -cl-unsafe-math-optimizations" \
			" -Xclang -cl-finite-math-only" \
			" -DFLOOR_COMPUTE_SPIR" +
			(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : "") +
			" -isystem " + floor::get_opencl_libcxx_path() +
			" -isystem " + floor::get_opencl_clang_path() + " " +
			warning_flags +
			additional_options +
			generic_flags +
			" -emit-llvm -S -o spir_3_5.ll - 2>&1"
		};
		
		//log_msg("spir cmd: %s", spir_cmd);
		string spir_bc_output = "";
		core::system(spir_cmd, spir_bc_output);
		//log_msg("spir cmd: %s", spir_cmd);
		log_msg("spir bc/ll: %s", spir_bc_output);
		
		const string spir_3_2_encoder_cmd {
			// NOTE: temporary fix to get this to compile with the intel compiler (readonly fail) and
			// the amd compiler (spir_kernel fail; clang/llvm currently don't emit this)
#if defined(__APPLE__)
			" sed -i \"\""
#else
			" sed -i"
#endif
			" -E \"s/readonly//g\" spir_3_5.ll &&"
#if defined(__APPLE__)
			" sed -i \"\""
#else
			" sed -i"
#endif
			" -E \"s/^define (.*)section \\\"spir_kernel\\\" (.*)/define spir_kernel \\1\\2/\" spir_3_5.ll &&"
			" " + floor::get_opencl_as() + " spir_3_5.ll && "
			// actual spir-encoder call:
			"spir-encoder spir_3_5.bc spir_3_2.bc 2>&1"
		};
		string spir_encoder_output = "";
		core::system(spir_3_2_encoder_cmd, spir_encoder_output);
		log_msg("spir encoder: %s", spir_encoder_output);
		
		string spir_bc;
		if(!file_io::file_to_string("spir_3_2.bc", spir_bc)) return { "", {} };
		
		vector<kernel_info> kernels;
		get_floor_metadata("spir_3_5.ll", kernels);
		return { spir_bc, kernels };
	}
	else if(target == TARGET::AIR) {
		const string air_cmd {
			printable_code +
			floor::get_metal_compiler() +
			" -x cl -std=gnu++14 -Xclang -cl-std=CL1.2 -target spir64-unknown-unknown" \
			" -Xclang -air-kernel-info" \
			" -Xclang -cl-mad-enable" \
			" -Xclang -cl-fast-relaxed-math" \
			" -Xclang -cl-unsafe-math-optimizations" \
			" -Xclang -cl-finite-math-only" \
			" -DFLOOR_COMPUTE_NO_DOUBLE" \
			" -DFLOOR_COMPUTE_METAL" +
			" -isystem " + floor::get_metal_libcxx_path() +
			" -isystem " + floor::get_metal_clang_path() + " " +
			warning_flags +
			additional_options +
			generic_flags +
			" -emit-llvm -S -o air_3_5.ll - 2>&1"
		};
		
		string air_ll_output = "";
		core::system(air_cmd, air_ll_output);
		log_msg("air ll: %s", air_ll_output);
		
		//
		string air_code = "";
		if(!process_air_llvm("air_3_5.ll", air_code)) {
			return { "", {} };
		}
		file_io::string_to_file("air_processed.ll", air_code); // for debugging purposes only
		
		vector<kernel_info> kernels;
		get_floor_metadata("air_processed.ll", kernels);
		return { air_code, kernels };
	}
	else if(target == TARGET::PTX) {
		const auto& force_sm = floor::get_cuda_force_compile_sm();
#if !defined(FLOOR_NO_CUDA)
		const auto& sm = ((cuda_device*)device.get())->sm;
		const string sm_version = (force_sm.empty() ? to_string(sm.x * 10 + sm.y) : force_sm);
#else
		const string sm_version = (force_sm.empty() ? "20" /* just default to fermi/sm_20 */ : force_sm);
#endif
		string ptx_cmd {
			printable_code +
			floor::get_cuda_compiler() +
			" -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
			" -Xclang -fcuda-is-device" \
			" -DFLOOR_COMPUTE_CUDA" +
			" -isystem " + floor::get_cuda_libcxx_path() +
			" -isystem " + floor::get_cuda_clang_path() + " " +
			warning_flags +
			additional_options +
			generic_flags +
			" -emit-llvm -S"
		};
		
		// TODO: clean this up + do this properly!
		const string ptx_bc_cmd = ptx_cmd + " -o cuda_ptx.ll - 2>&1";
		ptx_cmd += " -o - - 2>&1";
		ptx_cmd += (" | " + floor::get_cuda_llc() + " -nvptx-fma-level=2 -nvptx-sched4reg -enable-unsafe-fp-math -mcpu=sm_" + sm_version + " -mattr=ptx42 2>&1");
#if 1 // TODO: keep this for now, remove it when no longer needed (and this always works properly)
		ptx_cmd += " > cuda.ptx && cat cuda.ptx";
#endif
		
		string bc_output = "";
		core::system(ptx_bc_cmd, bc_output);
		log_msg("bc/ll: %s", bc_output);
		
		string ptx_code { "" };
		core::system(ptx_cmd, ptx_code);
		//log_msg("ptx cmd: %s", ptx_cmd);
		//log_msg("ptx code:\n%s\n", ptx_code);
		
		vector<kernel_info> kernels;
		get_floor_metadata("cuda_ptx.ll", kernels);
		return { ptx_code, kernels };
	}
	return { "", {} };
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program_file(shared_ptr<compute_device> device,
																				   const string& filename,
																				   const string additional_options,
																				   const TARGET target) {
	return compile_program(device, file_io::file_to_string(filename), additional_options, target);
}
