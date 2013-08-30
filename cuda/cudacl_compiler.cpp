/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#if defined(OCLRASTER_CUDA_CL)

#include "cudacl_compiler.h"
#include "core/core.h"
#include "oclraster.h"

#include "libtcc.h"
extern "C" {
#include "tcc.h"
}

static string cudacl_preprocess(const string& code, const string& build_options, const string& filename) {
	// init
	string preprocessed_code = "";
	TCCState* state = tcc_new();
	state->output_type = TCC_OUTPUT_PREPROCESS;
	
	// split build options and let tcc parse them
	const auto build_option_args = core::tokenize(build_options, ' ');
	const size_t argc = build_option_args.size();
	vector<const char*> argv;
	for(const auto& arg : build_option_args) {
		argv.emplace_back(arg.data());
		//cout << "\t### build arg: " << arg << endl;
	}
	tcc_parse_args(state, (int)argc, &argv[0]);
	
	// in-memory preprocessing
	const uint8_t* code_input = (const uint8_t*)code.c_str();
	tcc_in_memory_preprocess(state, code_input, code.length(), true, filename.c_str(), &preprocessed_code,
							 [](const char* str, void* ret) -> void {
								 *(string*)ret += str;
							 });
	
	// cleanup + return
	tcc_delete(state);
	return preprocessed_code;
}

string cudacl_compiler::compile(const string& code,
								const string& identifier,
								const string& cc_target_str,
								const string& user_options,
								const string& tmp_name,
								string& error_log,
								string& info_log) {
	// cuda (or nvcc in the default/automatic mode) uses several steps to compile a .cu file to a .ptx file:
	// 1) preprocess in c++ mode
	// 2) cudafe: split code into device and host code, and c++ -> c
	// 3) preprocess in c mode
	// 4) cudafe: "compile" and strip unused code
	// 5) preprocess in c mode (again)
	// 6) cicc: finally compiles the processed c code to ptx
	
	// static defines for all steps
	const string cuda_defines {
		// cc/sm version of the device
		" -D__CUDA_ARCH__="+cc_target_str+"0"+
		// for now, always include double math funcs
		" -DCUDA_DOUBLE_MATH_FUNCTIONS"+
		// pretend we're gcc, so the cuda headers don't complain ...
		" -D__GNUC__=4 -D__GNUC_MINOR__=2 -D__GNUC_PATCHLEVEL__=1"+
		// also pretend we're compiling on os x
		" -D__APPLE__=1"+
		// architecture defines (TODO: split 32/64-bit)
		" -D__i386__=1 -D__x86_64=1 -D__x86_64__=1 -D_LP64=1 -D__LP64__=1"+
		// necessary fp defines
		" -D__FLT_MIN__=1.17549435e-38F -D__DBL_MIN__=2.2250738585072014e-308 -D__LDBL_MIN__=3.36210314311209350626e-4932L"
	};
	const string cuda_include_dir { oclraster::get_cuda_base_dir() + "/include" };
	
	// TODO: error + log handling (parse output, directly abort if error)
	// TODO: pipe tcc output into log
	
	// 1: preprocess in c++ mode
	string code_step1 = cudacl_preprocess(string {
											  // include oclrasters vector lib header before the cuda runtime header,
											  // since oclraster complete replaces all cuda vector types
											  "#include \"oclr_cuda_vector_lib.h\"\n"
											  "#include \"cuda_runtime.h\"\n"
											  // after both the vector lib (vector base classes) and cuda runtime header,
											  // include the vector math header that provides all additional vector functions
											  // necessary for opencl emulation/wrapping (and are external to the vector classes)
											  "#include \"oclr_cuda_vector_math.h\"\n"
										  } + code,
										  // general cuda defines
										  cuda_defines +
										  // cuda defines specific to this stage
										  " -D__CUDACC__ -D__NVCC__ -D__cplusplus"+
										  " -D__VECTOR_TYPES_H__ -D_POSIX_C_SOURCE "+
										  // user options
										  user_options+
										  // general cuda includes
										  " -I/usr/include -I"+cuda_include_dir,
										  tmp_name+".cpp1.ii");
	// replace ident placeholder by the kernels identifier, so cudafe can tell in which
	// file something went wrong (note that this still doesn't give us the actual line number)
	core::find_and_replace(code_step1,
						   "struct __oclraster_ident_placeholder {};\n",
						   "# 1 \""+identifier+"\" 1\n");
	file_io::string_to_file(tmp_name+".cpp1.ii", code_step1);
	
	// 2: cudafe, pass one
	string output = "";
	core::system(string { "cudafe" } +
				 " --clang --m64 --gnu_version=40201 -tused --no_remove_unneeded_entities"+
				 " --gen_c_file_name "+tmp_name+".cudafe1.c"+
				 " --stub_file_name "+tmp_name+".cudafe1.stub.c"+
				 " --gen_device_file_name "+tmp_name+".cudafe1.gpu"+
				 " --nv_arch \"compute_"+cc_target_str+"\""+
				 " --gen_module_id_file --module_id_file_name "+tmp_name+".module_id"+
				 " --include_file_name "+tmp_name+".fatbin.c"+
				 " "+tmp_name+".cpp1.ii 2>&1",
				 output);
	if(!output.empty()) {
		if(output.find("error") != string::npos) {
			error_log += "cudafe #1: " + output;
		}
		else {
			info_log += "cudafe #1: " + output;
		}
	}
	
	// 3: preprocess in c mode
	string code_step3 = cudacl_preprocess(file_io::file_to_string(tmp_name+".cudafe1.gpu"),
										  // general cuda defines
										  cuda_defines +
										  // cuda defines specific to this stage
										  " -D__CUDACC__ -D__NVCC__ -D__CUDANVVM__"+
										  " -D__VECTOR_TYPES_H__ -D_POSIX_C_SOURCE"+
										  " -D__CUDA_PREC_DIV -D__CUDA_PREC_SQRT "+
										  // user options
										  user_options+
										  // general cuda includes
										  " -I/usr/include -I"+cuda_include_dir,
										  tmp_name+".cpp2.i");
	file_io::string_to_file(tmp_name+".cpp2.i", code_step3);
	
	// 4: cudafe, pass two
	output = "";
	core::system(string { "cudafe" } +
				 " -w --clang --m64 --gnu_version=40201 --c"+
				 " --gen_c_file_name "+tmp_name+".cudafe2.c"+
				 " --stub_file_name "+tmp_name+".cudafe2.stub.c"+
				 " --gen_device_file_name "+tmp_name+".cudafe2.gpu"+
				 " --nv_arch \"compute_"+cc_target_str+"\""+
				 " --module_id_file_name "+tmp_name+".module_id"+
				 " --include_file_name "+tmp_name+".fatbin.c"+
				 " "+tmp_name+".cpp2.i 2>&1",
				 output);
	if(!output.empty()) {
		if(output.find("error") != string::npos) {
			error_log += "cudafe #2: " + output;
		}
		else {
			info_log += "cudafe #2: " + output;
		}
	}
	
	// 5: preprocess one last time, in c mode
	string code_step5 = cudacl_preprocess(file_io::file_to_string(tmp_name+".cudafe2.gpu"),
										  // general cuda defines
										  cuda_defines +
										  // cuda defines specific to this stage
										  " -D__CUDABE__ -D__CUDANVVM__"+
										  " -D__VECTOR_TYPES_H__ -D_POSIX_C_SOURCE"+
										  " -D__CUDA_PREC_DIV -D__CUDA_PREC_SQRT "+
										  // user options
										  user_options+
										  // general cuda includes
										  " -I/usr/include -I"+cuda_include_dir,
										  tmp_name+".cpp3.i");
	file_io::string_to_file(tmp_name+".cpp3.i", code_step5);
	
	// 6: actual compilation using cicc (nvidias new llvm based compiler)
	output = "";
	core::system(string { "cicc" } +
				 (oclraster::get_cuda_profiling() || oclraster::get_cuda_debug() ? " -show-src" : "")+
				 " -arch \"compute_"+cc_target_str+"\""+
				 " -m64 -ftz=0 -prec_div=1 -prec_sqrt=1 -fmad=1"+
				 " -nvvmir-library " + oclraster::get_cuda_base_dir() + "/nvvm/libdevice/libdevice.compute_"+
				 (cc_target_str == "21" ? "20" : (cc_target_str == "32" ? "30" : cc_target_str))+
				 ".10.bc"+
				 " --orig_src_file_name "+tmp_name+".cu"+
				 " "+tmp_name+".cpp3.i"+
				 " -o "+tmp_name+".ptx",
				 output);
	if(!output.empty()) {
		if(output.find("error") != string::npos) {
			error_log += "cicc: " + output;
		}
		else {
			info_log += "cicc: " + output;
		}
	}
	
	//
	return file_io::file_to_string(tmp_name+".ptx");
}

#endif
