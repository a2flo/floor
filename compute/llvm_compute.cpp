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
#include <regex>

#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

static bool get_floor_metadata(const string& lines_str, vector<llvm_compute::kernel_info>& kernels) {
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
				// kernel arg info: #elem_idx size, address space, image type, image access
				const auto data = stoull(elem);
				kernel.args.emplace_back(llvm_compute::kernel_info::kernel_arg_info {
					.size			= (uint32_t)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::ARG_SIZE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::ARG_SIZE_SHIFT)),
					.address_space	= (llvm_compute::kernel_info::ARG_ADDRESS_SPACE)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::ADDRESS_SPACE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::ADDRESS_SPACE_SHIFT)),
					.image_type		= (llvm_compute::kernel_info::ARG_IMAGE_TYPE)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_TYPE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_TYPE_SHIFT)),
					.image_access	= (llvm_compute::kernel_info::ARG_IMAGE_ACCESS)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_ACCESS_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_ACCESS_SHIFT)),
				});
			}
			++elem_idx;
		});
	}
	
#if 0
	// print info about all kernels and their arguments
	for(const auto& kernel : kernels) {
		string sizes_str = "";
		for(size_t i = 0, count = kernel.arg_sizes.size(); i < count; ++i) {
			switch(kernel.arg_address_spaces[i]) {
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL:
					sizes_str += "global ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::LOCAL:
					sizes_str += "local ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT:
					sizes_str += "constant ";
					break;
				default: break;
			}
			sizes_str += to_string(kernel.arg_sizes[i]) + (i + 1 < count ? "," : "") + " ";
		}
		sizes_str = core::trim(sizes_str);
		log_msg("kernel: %s (%s)", kernel.name, sizes_str);
	}
#endif
	
	return true;
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program(shared_ptr<compute_device> device,
																			  const string& code,
																			  const string additional_options,
																			  const TARGET target) {
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	return compile_input("-", printable_code, device, additional_options, target);
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program_file(shared_ptr<compute_device> device,
																				   const string& filename,
																				   const string additional_options,
																				   const TARGET target) {
	return compile_input(filename, "", device, additional_options, target);
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_input(const string& input,
																			const string& cmd_prefix,
																			shared_ptr<compute_device> device,
																			const string additional_options,
																			const TARGET target) {
	// TODO/NOTE: additional clang flags:
	//  -vectorize-loops -vectorize-slp -vectorize-slp-aggressive
	//  -menable-unsafe-fp-math
	
	// for now, only enable these in debug mode (note that these cost a not insignificant amount of compilation time)
#if defined(FLOOR_DEBUG) && 0
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
	
	// create the initial clang compilation command
	string clang_cmd = cmd_prefix + " ";
	string libcxx_path = " -isystem ", clang_path = " -isystem ";
	switch(target) {
		case TARGET::SPIR:
			clang_cmd += {
				floor::get_opencl_compiler() +
				" -x cl -std=gnu++14 -Xclang -cl-std=CL1.2" \
				" -target " + (device->bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_SPIR" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : "")
			};
			libcxx_path += floor::get_opencl_libcxx_path();
			clang_path += floor::get_opencl_clang_path();
			break;
		case TARGET::AIR:
			clang_cmd += {
				floor::get_metal_compiler() +
				// NOTE: always compiling to 64-bit here, because 32-bit never existed
				" -x cl -std=gnu++14 -Xclang -cl-std=CL1.2 -target spir64-unknown-unknown" \
				" -Xclang -air-kernel-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_NO_DOUBLE" \
				" -DFLOOR_COMPUTE_METAL"
			};
			libcxx_path += floor::get_metal_libcxx_path();
			clang_path += floor::get_metal_clang_path();
			break;
		case TARGET::PTX:
			clang_cmd += {
				floor::get_cuda_compiler() +
				" -x cuda -std=cuda" \
				" -target " + (device->bitness == 32 ? "nvptx-nvidia-cuda" : "nvptx64-nvidia-cuda") +
				" -Xclang -fcuda-is-device" \
				" -DFLOOR_COMPUTE_CUDA"
			};
			libcxx_path += floor::get_cuda_libcxx_path();
			clang_path += floor::get_cuda_clang_path();
			break;
	}
	
	// add generic flags/options that are always used
	// TODO: use debug/profiling config options
	clang_cmd += {
		" -DFLOOR_COMPUTE"
		" -DFLOOR_NO_MATH_STR" +
		libcxx_path + clang_path +
#if !defined(_MSC_VER) // TODO: better use a config entry for these as well
		" -isystem /usr/include"
#else
		" -isystem %FLOOR_INCLUDE%"
#endif
		" -include floor/compute/device/common.hpp"
		" -fno-exceptions -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -flto -Ofast " +
		warning_flags +
		additional_options +
		// compile to the right device bitness
		(device->bitness == 32 ? " -m32 -DPLATFORM_X32" : " -m64 -DPLATFORM_X64") +
		" -emit-llvm -S -o - " + input
	};
	
	// on sane systems, redirect errors to stdout so that we can grab them
#if !defined(_MSC_VER)
	clang_cmd += " 2>&1";
#endif
	
	// compile and store the llvm ir in a string (stdout output)
	string ir_output = "";
	core::system(clang_cmd, ir_output);
	// if the output is empty or doesn't start with the llvm "ModuleID", something is probably wrong
	if(ir_output == "" || ir_output.size() < 10 || ir_output.substr(0, 10) != "; ModuleID") {
		log_error("compilation failed! failed cmd was:\n%s", clang_cmd);
		if(ir_output != "") {
			log_error("compilation errors:\n%s", ir_output);
		}
		return {};
	}
	
	// grab floor metadata from compiled ir and create per-kernel info
	vector<kernel_info> kernels;
	get_floor_metadata(ir_output, kernels);
	
	// final target specific processing/compilation
	string compiled_code = "";
	if(target == TARGET::SPIR) {
		// NOTE: temporary fixes to get this to compile with the intel compiler (readonly fail) and
		// the amd compiler (spir_kernel fail; clang/llvm currently don't emit this)
		core::find_and_replace(ir_output, "readonly", "");
		core::find_and_replace(ir_output, "nocapture readnone", "");
		
		static const regex rx_spir_kernel("define (.*)section \\\"spir_kernel\\\" (.*)", regex::optimize);
		ir_output = regex_replace(ir_output, rx_spir_kernel, "define spir_kernel $1$2");
		
		// output modified ir back to a file and create a bc file so spir-encoder can consume it
		if(!file_io::string_to_file("spir_3_5.ll", ir_output)) {
			log_error("failed to output LLVM IR for SPIR consumption");
			return {};
		}
		core::system(floor::get_opencl_as() + " -o spir_3_5.bc spir_3_5.ll");
		
		// run spir-encoder for 3.5 -> 3.2 conversion
		const string spir_3_2_encoder_cmd {
			"spir-encoder spir_3_5.bc spir_3_2.bc"s
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		string spir_encoder_output = "";
		core::system(spir_3_2_encoder_cmd, spir_encoder_output);
		log_msg("spir encoder: %s", spir_encoder_output);
		
		// finally, read converted bitcode data back, this is the code that will be compiled by the opencl implementation
		if(!file_io::file_to_string("spir_3_2.bc", compiled_code)) {
			log_error("failed to read back SPIR 1.2 .bc file");
			return {};
		}
	}
	else if(target == TARGET::AIR) {
		// this exchanges the module header/target to the one apple expects
		static const regex rx_datalayout("target datalayout = \"(.*)\"");
		static const regex rx_triple("target triple = \"(.*)\"");
		ir_output = regex_replace(ir_output, rx_datalayout,
								  "target datalayout \"e-i64:64-f80:128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-n8:16:32\"");
		ir_output = regex_replace(ir_output, rx_triple, "target triple = \"air64-apple-ios8.1.0\"");
		
		// output final processed ir if this was specified in the config
		if(floor::get_compute_keep_temp()) {
			file_io::string_to_file("air_processed.ll", ir_output);
		}
		
		// llvm ir is the final output format
		compiled_code.swap(ir_output);
	}
	else if(target == TARGET::PTX) {
		const auto& force_sm = floor::get_cuda_force_compile_sm();
#if !defined(FLOOR_NO_CUDA)
		const auto& sm = ((cuda_device*)device.get())->sm;
		const string sm_version = (force_sm.empty() ? to_string(sm.x * 10 + sm.y) : force_sm);
#else
		const string sm_version = (force_sm.empty() ? "20" /* just default to fermi/sm_20 */ : force_sm);
#endif
		
		// llc expects an input file (or stdin, but we can't write there reliably)
		if(!file_io::string_to_file("cuda_ptx.ll", ir_output)) {
			log_error("failed to output LLVM IR for llc consumption");
			return {};
		}
		
		// compile llvm ir to ptx
		const string llc_cmd {
			floor::get_cuda_llc() +
			" -nvptx-fma-level=2 -nvptx-sched4reg -enable-unsafe-fp-math" \
			" -mcpu=sm_" + sm_version + " -mattr=ptx42" +
			" -o - cuda_ptx.ll"
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		core::system(llc_cmd, compiled_code);
		
		// only output the compiled ptx code if this was specified in the config
		if(floor::get_compute_keep_temp()) {
			file_io::string_to_file("cuda.ptx", compiled_code);
		}
		
		// check if we have sane output
		if(compiled_code == "" || compiled_code.find("Generated by LLVM NVPTX Back-End") == string::npos) {
			log_error("llc/ptx compilation failed!\n%s", compiled_code);
			return {};
		}
	}
	
	return { compiled_code, kernels };
}
