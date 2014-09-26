/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#if !defined(FLOOR_NO_OPENCL) && !defined(FLOOR_NO_CUDA_CL)

cudacl* llvm_compute::cucl { nullptr };
vector<CUmodule> llvm_compute::modules;
unordered_map<string, CUfunction> llvm_compute::functions;

void llvm_compute::init() {
	// TODO: test if ocl is actually cudacl
	cucl = (cudacl*)ocl;
}

void llvm_compute::compile_kernel(const string& code) {
	// clang flags:
	//  -Xclang -fcuda-is-device -D__CUDA_CLANG__ -m64 -target nvptx64-nvidia-cuda -std=cuda -fno-exceptions -Ofast -emit-llvm
	//  -S for human-readable llvm ir
	// note: llc flags:
	//  -nvptx-sched4reg (NVPTX Specific: schedule for register pressure)
	//  -enable-unsafe-fp-math
	//  -mcpu=sm_35
	
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | "};
	log_msg("printable: %s", printable_code);
	
	const string preprocess_cuda_cmd {
		printable_code +
		"cuda_clang -E -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
		" -Xclang -fcuda-is-device" \
		" -D__CUDA_CLANG__" \
		" -D__CUDA_CLANG_PREPROCESS__" \
		" -DFLOOR_LLVM_COMPUTE" \
		" -DFLOOR_NO_MATH_STR" \
		" -DPLATFORM_X64" \
		" -DFLOOR_DEVICE=\"__attribute__((device)) __attribute__((host))\"" \
		" -include cuda_support.hpp" \
		" -isystem /Users/flo/clang/libcxx/include" \
		" -isystem /usr/local/include" \
		" -m64 -fno-exceptions" \
		" -o - -"
	};
	
	//string preprocessed_cuda_code = "";
	//core::system(preprocess_cuda_cmd, preprocessed_cuda_code);
	
	//log_msg("preproc: %s", preprocessed_cuda_code);
	//const string printable_cuda_code { "printf \"" + core::str_hex_escape(preprocessed_cuda_code) + "\" | "};
	//log_msg("printable_cuda_code: %s", printable_cuda_code);
	
	// cuda clang/llc call
	//printable_code += "cuda_clang -Xclang -fcuda-is-device -D__CUDA_CLANG__ -m64 -target nvptx64-nvidia-cuda -std=cuda -fno-exceptions -Ofast -emit-llvm -x cuda -S -o - - | cuda_llc -mcpu=sm_30";
	
	//string ptx_cmd = printable_cuda_code +
	string ptx_cmd = preprocess_cuda_cmd +
	" | cuda_clang -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
	" -Xclang -fcuda-is-device" \
	" -D__CUDA_CLANG__" \
	" -DFLOOR_LLVM_COMPUTE" \
	" -DFLOOR_NO_MATH_STR" \
	" -DPLATFORM_X64" \
	" -DFLOOR_DEVICE=\"__attribute__((device)) __attribute__((host))\"" \
	" -Dkernel=\"__attribute__((global))\"" \
	" -include cuda_support.hpp" \
	" -isystem /Users/flo/clang/libcxx/include" \
	" -isystem /usr/local/include/floor" \
	" -m64 -fno-exceptions -Ofast -emit-llvm -S";
	
	string spir_cmd = printable_code + "cuda_clang -x cl -std=gnu++14 -Xclang -cl-std=CL1.2 -target spir64-unknown-unknown" \
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
	" -include cuda_support.hpp" \
	" -isystem /Users/flo/clang/libcxx/include" \
	" -isystem /usr/local/include/floor" \
	" -m64 -fno-exceptions -Ofast -emit-llvm -S -o - -"; // TODO: 2>&1 // > code_spir64.ll";
	
	//const string ptx_bc_cmd = ptx_cmd + " -o - - | llvm-as -o=code_ptx.bc";
	//const string ptx_bc_cmd = ptx_cmd + " -o - - > code_ptx.ll";
	const string ptx_bc_cmd = ptx_cmd + " -o cuda_ptx.bc - 2>&1";
	ptx_cmd += " -o - - 2>&1 | cuda_llc -mcpu=sm_" + cucl->get_cc_target_str();
	
	string ptx_code { "" };
	core::system(ptx_cmd, ptx_code);
	//log_msg("ptx cmd: %s", ptx_cmd);
	log_msg("ptx code:\n%s\n", ptx_code);
	
	string bc_output = "";
	core::system(ptx_bc_cmd, bc_output);
	log_msg("bc/ll: %s", bc_output);
	
	this_thread::yield();
	
	log_msg("spir cmd: %s", spir_cmd);
	string spir_bc_output = "";
	core::system(spir_cmd, spir_bc_output);
	log_msg("spir bc/ll: %s", spir_bc_output);
}

void llvm_compute::compile_kernel_file(const string& filename) {
	compile_kernel(file_io::file_to_string(filename));
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
		{ cucl->get_cc_target() },
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
