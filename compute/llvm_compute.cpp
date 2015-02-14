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

string llvm_compute::compile_program(shared_ptr<compute_device> device,
									 const string& code, const string additional_options, const TARGET target,
									 vector<string>* kernel_names) {
	// note: llc flags:
	//  -nvptx-sched4reg (NVPTX Specific: schedule for register pressure)
	//  -nvptx-fma-level=2 (0: disabled, 1: enabled, 2: aggressive)
	//  -enable-unsafe-fp-math
	//  -mcpu=sm_35
	// note: additional clang flags:
	//  -vectorize-loops -vectorize-slp -vectorize-slp-aggressive
	//  -menable-unsafe-fp-math
	
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	//log_msg("printable: %s", printable_code);
	
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
			" -m64 -fno-exceptions -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -flto -Ofast " +
			(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : "") +
			warning_flags +
			additional_options +
			" -emit-llvm -c -o spir_3_5.bc - 2>&1"
		};
		
		//log_msg("spir cmd: %s", spir_cmd);
		string spir_bc_output = "";
		core::system(spir_cmd, spir_bc_output);
		//log_msg("spir cmd: %s", spir_cmd);
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
#if !defined(FLOOR_NO_CUDA)
		const auto& sm = ((cuda_device*)device.get())->sm;
		const string sm_version = to_string(sm.x * 10 + sm.y);
#else
		const string sm_version = "20"; // just default to fermi/sm_20
#endif
		string ptx_cmd {
			printable_code +
			FLOOR_COMPUTE_CLANG \
			" -x cuda -std=cuda -target nvptx64-nvidia-cuda" \
			" -Xclang -fcuda-is-device" \
			" -D__CUDA_CLANG__" \
			" -DFLOOR_LLVM_COMPUTE" \
			" -DFLOOR_NO_MATH_STR" \
			" -DPLATFORM_X64" \
			" -include floor/compute/compute_support.hpp" \
			" -include floor/constexpr/const_math.hpp" \
			" -include floor/constexpr/const_math.cpp" \
			" -isystem " FLOOR_COMPUTE_LIBCXX_PATH \
			" -isystem " FLOOR_COMPUTE_CLANG_PATH \
			" -isystem /usr/local/include" \
			" -m64 -fno-exceptions -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -flto -Ofast " +
			warning_flags +
			additional_options +
			" -emit-llvm -S"
		};
		
		// TODO: clean this up + do this properly!
		const string ptx_bc_cmd = ptx_cmd + " -o cuda_ptx.ll - 2>&1";
		ptx_cmd += " -o - - 2>&1";
		ptx_cmd += (" | " FLOOR_COMPUTE_LLC " -nvptx-fma-level=2 -nvptx-sched4reg -enable-unsafe-fp-math -mcpu=sm_" + sm_version + " 2>&1");
		ptx_cmd += u8R"RAW( | sed -E "s/@\"\\\\01([a-zA-Z0-9_]+)\"/@\1/g")RAW";
		ptx_cmd += u8R"RAW( | sed -E "s/\[\"(.*)\"\]/\[\1\]/g" | tr -d "\001")RAW";
		ptx_cmd += " > cuda.ptx && cat cuda.ptx";
		
		string bc_output = "";
		core::system(ptx_bc_cmd, bc_output);
		log_msg("bc/ll: %s", bc_output);
		
		string ptx_code { "" };
		core::system(ptx_cmd, ptx_code);
		//log_msg("ptx cmd: %s", ptx_cmd);
		//log_msg("ptx code:\n%s\n", ptx_code);
		
		// retrieve kernel names
		if(kernel_names != nullptr) {
			const string kernel_names_cmd { u8R"RAW(cat cuda_ptx.ll | grep "metadata !\"kernel\"")RAW" };
			string kernel_names_data = "";
			core::system(kernel_names_cmd, kernel_names_data);
			const auto kernel_lines = core::tokenize(kernel_names_data, '\n');
			for(const auto& line : kernel_lines) {
				const auto at_pos = line.find('@');
				if(at_pos == string::npos) continue; // probably not a kernel metadata line
				const auto comma_pos = line.find(',', at_pos);
				if(comma_pos == string::npos) continue;
				kernel_names->emplace_back(line.substr(at_pos + 1, comma_pos - at_pos - 1));
			}
		}
		
		return ptx_code;
	}
	return "";
}

string llvm_compute::compile_program_file(shared_ptr<compute_device> device,
										  const string& filename, const string additional_options, const TARGET target,
										  vector<string>* kernel_names) {
	return compile_program(device, file_io::file_to_string(filename), additional_options, target, kernel_names);
}
