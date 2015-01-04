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

#if !defined(FLOOR_NO_OPENCL) || !defined(FLOOR_NO_CUDA)

#if !defined(FLOOR_COMPUTE_CLANG)
#define FLOOR_COMPUTE_CLANG "compute_clang"
#endif

#if !defined(FLOOR_COMPUTE_LLC)
#define FLOOR_COMPUTE_LLC "compute_llc"
#endif

#if !defined(FLOOR_COMPUTE_LIBCXX_PATH)
#define FLOOR_COMPUTE_LIBCXX_PATH "/usr/local/include/floor/libcxx/include"
#endif

#if !defined(FLOOR_COMPUTE_CLANG_PATH)
#define FLOOR_COMPUTE_CLANG_PATH "/usr/local/include/floor/libcxx/clang"
#endif

vector<CUmodule> llvm_compute::modules;
unordered_map<string, CUfunction> llvm_compute::functions;

string llvm_compute::compile_program(const string& code, const string additional_options, const TARGET target) {
	// note: llc flags:
	//  -nvptx-sched4reg (NVPTX Specific: schedule for register pressure)
	//  -enable-unsafe-fp-math
	//  -mcpu=sm_35
	
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | "};
	//log_msg("printable: %s", printable_code);
	
	if(target == TARGET::SPIR) {
		string spir_cmd {
			printable_code +
			FLOOR_COMPUTE_CLANG \
			" -x cl -std=gnu++14 -Xclang -cl-std=CL1.2 -target spir64-unknown-unknown" \
			" -Xclang -cl-kernel-arg-info" \
			" -Xclang -cl-mad-enable" \
			" -Xclang -cl-fast-relaxed-math" \
			" -Xclang -cl-unsafe-math-optimizations" \
			" -Xclang -cl-finite-math-only" \
			" -D__SPIR_CLANG__" \
			" -DFLOOR_LLVM_COMPUTE" \
			" -DFLOOR_NO_MATH_STR" \
			" -DPLATFORM_X64" \
			" -DFLOOR_CL_CONSTANT=constant" \
			" -include floor/compute/compute_support.hpp" \
			" -include floor/constexpr/const_math.hpp" \
			" -include floor/constexpr/const_math.cpp" \
			" -isystem " FLOOR_COMPUTE_LIBCXX_PATH \
			" -isystem " FLOOR_COMPUTE_CLANG_PATH \
			" -isystem /usr/local/include" \
			" -m64 -fno-exceptions -Ofast " +
			additional_options +
			" -emit-llvm -c -o spir_3_5.bc - 2>&1"
		};
		
		//log_msg("spir cmd: %s", spir_cmd);
		string spir_bc_output = "";
		core::system(spir_cmd, spir_bc_output);
		log_msg("spir bc/ll: %s", spir_bc_output);
		
		const string spir_3_2_encoder_cmd {
			// NOTE: temporary fix to get this to compile with the intel compiler (readonly fail) and
			// the amd compiler (spir_kernel fail; clang/llvm currently don't emit this)
			"llvm-dis spir_3_5.bc &&"
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
			" llvm-as spir_3_5.ll && "
			// actual spir-encoder call:
			"spir-encoder spir_3_5.bc spir_3_2.bc 2>&1"
		};
		string spir_encoder_output = "";
		core::system(spir_3_2_encoder_cmd, spir_encoder_output);
		log_msg("spir encoder: %s", spir_encoder_output);
		
		string spir_bc;
		if(!file_io::file_to_string("spir_3_2.bc", spir_bc)) return "";
		return spir_bc;
	}
	else if(target == TARGET::PTX) {
		const string preprocess_cuda_cmd {
			printable_code +
			FLOOR_COMPUTE_CLANG \
			" -E -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
			" -Xclang -fcuda-is-device" \
			" -D__CUDA_CLANG__" \
			" -D__CUDA_CLANG_PREPROCESS__" \
			" -DFLOOR_LLVM_COMPUTE" \
			" -DFLOOR_NO_MATH_STR" \
			" -DPLATFORM_X64" \
			" -DFLOOR_DEVICE=\"__attribute__((device)) __attribute__((host))\"" \
			" -include floor/compute/compute_support.hpp" \
			" -isystem " FLOOR_COMPUTE_LIBCXX_PATH \
			" -isystem " FLOOR_COMPUTE_CLANG_PATH \
			" -isystem /usr/local/include" \
			" -m64 -fno-exceptions " +
			additional_options +
			" -o - -"
		};
		
		string ptx_cmd {
			preprocess_cuda_cmd + " | " \
			FLOOR_COMPUTE_CLANG \
			" -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
			" -Xclang -fcuda-is-device" \
			" -D__CUDA_CLANG__" \
			" -DFLOOR_LLVM_COMPUTE" \
			" -DFLOOR_NO_MATH_STR" \
			" -DPLATFORM_X64" \
			" -DFLOOR_DEVICE=\"__attribute__((device)) __attribute__((host))\"" \
			" -Dkernel=\"__attribute__((global))\"" \
			" -include floor/compute/compute_support.hpp" \
			" -isystem " FLOOR_COMPUTE_LIBCXX_PATH \
			" -isystem " FLOOR_COMPUTE_CLANG_PATH \
			" -isystem /usr/local/include" \
			" -m64 -fno-exceptions -Ofast " +
			additional_options +
			" -emit-llvm -S"
		};
		
		//const string ptx_bc_cmd = ptx_cmd + " -o - - | llvm-as -o=code_ptx.bc";
		//const string ptx_bc_cmd = ptx_cmd + " -o - - > code_ptx.ll";
		const string ptx_bc_cmd = ptx_cmd + " -o cuda_ptx.bc - 2>&1";
		//ptx_cmd += " -o - - 2>&1 | " FLOOR_COMPUTE_LLC " -mcpu=sm_" + cucl->get_cc_target_str(); // TODO: !
		ptx_cmd += " -o - - 2>&1 | " FLOOR_COMPUTE_LLC " -mcpu=sm_20";
		
		string ptx_code { "" };
		core::system(ptx_cmd, ptx_code);
		//log_msg("ptx cmd: %s", ptx_cmd);
		log_msg("ptx code:\n%s\n", ptx_code);
		
		string bc_output = "";
		core::system(ptx_bc_cmd, bc_output);
		log_msg("bc/ll: %s", bc_output);
		
		return ptx_code;
	}
	return "";
}

string llvm_compute::compile_program_file(const string& filename, const string additional_options, const TARGET target) {
	return compile_program(file_io::file_to_string(filename), additional_options, target);
}

void llvm_compute::load_module_from_file(const string& file_name, const vector<pair<string, string>>& function_mappings) {
	string module_data;
	if(!file_io::file_to_string(file_name, module_data)) {
		log_error("failed to load cuda module: %s", file_name);
		return;
	}
	load_module(module_data.c_str(), function_mappings);
}

void llvm_compute::load_module(const char* module_data, const vector<pair<string, string>>& function_mappings) {
	// jit the module / ptx code
	const CUjit_option jit_options[] {
		CU_JIT_TARGET,
		CU_JIT_GENERATE_LINE_INFO,
		CU_JIT_GENERATE_DEBUG_INFO,
		CU_JIT_MAX_REGISTERS
	};
	constexpr auto option_count = sizeof(jit_options) / sizeof(CUjit_option);
	const struct alignas(void*) {
		unsigned int ui;
	} jit_option_values[] {
		//{ cucl->get_cc_target() }, // TODO: !
		{ CU_TARGET_COMPUTE_20 },
		{ .ui = (floor::get_cuda_profiling() || floor::get_cuda_debug()) ? 1u : 0u },
		{ .ui = floor::get_cuda_debug() ? 1u : 0u },
		{ 32u }
	};
	static_assert(option_count == (sizeof(jit_option_values) / sizeof(void*)), "mismatching option count");
	
	CUmodule module;
	CU(cuModuleLoadDataEx(&module,
						  module_data,
						  option_count,
						  (CUjit_option*)&jit_options[0],
						  (void**)&jit_option_values[0]));
	modules.emplace_back(module);
	
	// get the functions
	for(const auto& func : function_mappings) {
		CUfunction cuda_func;
		CU(cuModuleGetFunction(&cuda_func, module, func.first.c_str()));
		functions[func.second] = cuda_func;
	}
}

#endif
