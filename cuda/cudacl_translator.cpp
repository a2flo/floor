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

#include "cudacl_translator.h"
#include "core/core.h"
#include "core/gl_support.h"
#include "oclraster.h"
#include "core/timer.h"
#include <regex>

#if defined(__APPLE__)
#include <CUDA/cuda.h>
#include <CUDA/cudaGL.h>
#else
#include <cuda.h>
#include <cudaGL.h>
#endif

#include "libtcc.h"
extern "C" {
#include "tcc.h"
}

// parameter mappings
struct param_address_space_type_map { const char* type_str; CUDACL_PARAM_ADDRESS_SPACE type; };
static constexpr array<const param_address_space_type_map, 6> address_space_mapping {
	{
		{ "__global", CUDACL_PARAM_ADDRESS_SPACE::GLOBAL },
		{ "global", CUDACL_PARAM_ADDRESS_SPACE::GLOBAL },
		{ "__local", CUDACL_PARAM_ADDRESS_SPACE::LOCAL },
		{ "local", CUDACL_PARAM_ADDRESS_SPACE::LOCAL },
		{ "__constant", CUDACL_PARAM_ADDRESS_SPACE::CONSTANT },
		{ "constant", CUDACL_PARAM_ADDRESS_SPACE::CONSTANT },
	}
};
struct param_type_type_map { const char* type_str; CUDACL_PARAM_TYPE type; };
static constexpr array<const param_type_type_map, 5> type_mapping {
	{
		{ "*", CUDACL_PARAM_TYPE::BUFFER },
		{ "image1d_t", CUDACL_PARAM_TYPE::IMAGE_1D },
		{ "image2d_t", CUDACL_PARAM_TYPE::IMAGE_2D },
		{ "image3d_t", CUDACL_PARAM_TYPE::IMAGE_3D },
		{ "sampler_t", CUDACL_PARAM_TYPE::SAMPLER },
	}
};
struct param_access_type_map { const char* type_str; CUDACL_PARAM_ACCESS type; };
static constexpr array<const param_access_type_map, 6> access_mapping {
	{
		{ "__read_only", CUDACL_PARAM_ACCESS::READ_ONLY },
		{ "read_only", CUDACL_PARAM_ACCESS::READ_ONLY },
		{ "__write_only", CUDACL_PARAM_ACCESS::WRITE_ONLY },
		{ "write_only", CUDACL_PARAM_ACCESS::WRITE_ONLY },
		{ "__read_write", CUDACL_PARAM_ACCESS::READ_WRITE },
		{ "read_write", CUDACL_PARAM_ACCESS::READ_WRITE },
	}
};

void cudacl_translate(const string& cl_source,
					  const string& preprocess_options,
					  string& cuda_source,
					  vector<cudacl_kernel_info>& kernels,
					  const bool use_cache,
					  bool& found_in_cache,
					  uint128& kernel_hash,
					  std::function<bool(const uint128&)> hash_lookup) {
#define OCLRASTER_REGEX_MARKER "$$$OCLRASTER_REGEX_MARKER$$$"
	static constexpr char cuda_preprocess_header[] {
		"#include \"oclr_cudacl.h\"\n"
		OCLRASTER_REGEX_MARKER "\n"
	};
	static constexpr char cuda_header[] {
		"#include \"cuda_runtime.h\"\n"
		"#undef signbit\n" // must undef cudas signbit define to extend functionality to vector types
	};
	
	oclr_timer timer;
	
	cuda_source = cuda_preprocess_header + cl_source;
	timer.add("source string", false);
	
	// get kernel signatures:
	// to do this, copy the currently processed "cuda source",
	// preprocess the source with tccpp,
	// replace all whitespace by a single ' ' (basicly putting everything in one line),
	// and do some regex magic to get the final info
	
	// preprocess cl source with tccpp
	string kernel_source = "";
	{
		// init
		TCCState* state = tcc_new();
		state->output_type = TCC_OUTPUT_PREPROCESS;
		
		// split build options and let tcc parse them
		const string build_options = ("-I" + core::strip_path(oclraster::kernel_path("")) + " " +
									  "-I" + core::strip_path(oclraster::kernel_path("cuda")) + " " +
									  "-I " + oclraster::get_cuda_base_dir() + "/include/ " + preprocess_options);
		const auto build_option_args = core::tokenize(build_options, ' ');
		const size_t argc = build_option_args.size();
		vector<const char*> argv;
		for(const auto& arg : build_option_args) {
			argv.emplace_back(arg.data());
		}
		tcc_parse_args(state, (int)argc, &argv[0]);
		
		// in-memory preprocessing
		const uint8_t* code_input = (const uint8_t*)cuda_source.c_str();
		tcc_in_memory_preprocess(state, code_input, cuda_source.length(), true, NULL, &kernel_source,
								 [](const char* str, void* ret) -> void {
									 *(string*)ret += str;
								 });
		
		// cleanup + return
		tcc_delete(state);
	}
	timer.add("preprocessing", false);
	
	// code hashing (note that this has to be done _after_ preprocessing,
	// since build options and/or header files might have changed)
	uint128 src_hash = CityHash128(kernel_source.c_str(), kernel_source.size());
	timer.add("hashing", false);
	kernel_hash = src_hash;
	
	// in preprocessed source: find regex marker, erase it, only apply regex on actual user code (add cuda header stuff later)
	const size_t regex_marker_pos = kernel_source.find(OCLRASTER_REGEX_MARKER);
	kernel_source.erase(regex_marker_pos, sizeof(OCLRASTER_REGEX_MARKER));
	string cl_kernel_source = kernel_source.substr(regex_marker_pos);
	cuda_source = cl_kernel_source;
	
	// replace "__kernel" by "kernel"
	static const regex rx_kernel_name("__kernel", regex::optimize);
	cl_kernel_source = regex_replace(cl_kernel_source, rx_kernel_name, "kernel");
	
	static const regex rx_space("\\s+", regex::optimize);
	static const regex rx_comment("#(.*)", regex::optimize);
	static const regex rx_kernel("kernel([\\w ]+)\\(([^\\)]*)\\)", regex::optimize);
	static const regex rx_identifier("[_a-zA-Z]\\w*", regex::optimize);
	static const regex rx_identifier_neg("\\W", regex::optimize);
	static const regex rx_attributes("__attribute__([\\s]*)\\(\\((.*)\\)\\)", regex::optimize);
	
	cl_kernel_source = regex_replace(cl_kernel_source, rx_attributes, "");
	cl_kernel_source = regex_replace(cl_kernel_source, rx_comment, "");
	cl_kernel_source = regex_replace(cl_kernel_source, rx_space, " ");
	
	size_t ws_pos = 0;
	for(sregex_iterator iter(cl_kernel_source.begin(), cl_kernel_source.end(), rx_kernel), end; iter != end; iter++) {
		if(iter->size() == 3) {
			const string params_str = (*iter)[2];
			
			string name_str = core::trim((*iter)[1]);
			if((ws_pos = name_str.find_last_of(" ")) != string::npos) {
				name_str = name_str.substr(ws_pos+1, name_str.size()-ws_pos-1);
				name_str = regex_replace(name_str, rx_identifier_neg, "");
			}
			
			const vector<string> params { core::tokenize(params_str, ',') };
			size_t param_counter = 0;
			vector<cudacl_kernel_info::kernel_param> kernel_parameters;
			for(const auto& param_str : params) {
				string param_name = core::trim(param_str);
				if((ws_pos = param_name.find_last_of(" ")) != string::npos) {
					param_name = param_name.substr(ws_pos+1, param_name.size()-ws_pos-1);
					param_name = regex_replace(param_name, rx_identifier_neg, "");
				}
				else param_name = "<unknown>";
				
				cudacl_kernel_info::kernel_param param(param_name, CUDACL_PARAM_ADDRESS_SPACE::NONE, CUDACL_PARAM_TYPE::NONE, CUDACL_PARAM_ACCESS::NONE);
				for(size_t i = 0; i < address_space_mapping.size(); i++) {
					if(param_str.find(address_space_mapping[i].type_str) != string::npos) {
						get<1>(param) = address_space_mapping[i].type;
					}
				}
				for(size_t i = 0; i < type_mapping.size(); i++) {
					if(param_str.find(type_mapping[i].type_str) != string::npos) {
						get<2>(param) = type_mapping[i].type;
					}
				}
				for(size_t i = 0; i < access_mapping.size(); i++) {
					if(param_str.find(access_mapping[i].type_str) != string::npos) {
						get<3>(param) = access_mapping[i].type;
					}
				}
				
				kernel_parameters.push_back(param);
				
				param_counter++;
			}
			
			kernels.emplace_back(name_str, kernel_parameters);
		}
	}
	timer.add("cl regex", false);
	
	// if cache usage is enabled, check if the hash can be found in the cache
	// note: this can't be done earlier, since the kernel info generated/extracted
	// in the previous lines is required to use the kernel
	if(use_cache) {
		if(hash_lookup(src_hash)) {
			// found it -> can return immediately
			found_in_cache = true;
			return;
		}
	}
	found_in_cache = false;
	
	// replace opencl keywords with cuda keywords
	static const vector<pair<const regex, const string>> rx_cl2cuda {
		{
			{ regex("# ", regex::optimize), "// " },
			
			// remove "global", "local", "private" qualifiers from pointers (cuda doesn't care)
			{ regex("([^\\w_]+)global([\\w\\s]+)(\\*)", regex::optimize), "$1$2$3" },
			{ regex("([^\\w_]+)local([\\w\\s]+)(\\*)", regex::optimize), "$1$2$3" },
			{ regex("([^\\w_]+)private([\\w\\s]+)(\\*)", regex::optimize), "$1$2$3" },
			
			// actual storage declarations, or other address spaces that don't matter:
			{ regex("([^\\w_]+)global ", regex::optimize), "$1 " },
			{ regex("([^\\w_]+)local ", regex::optimize), "$1__shared__ " },
			{ regex("([^\\w_]+)private ", regex::optimize), "$1 " },
			{ regex("([^\\w_]+)constant ", regex::optimize), "$1 " }, // "__constant__ "
			
			// misc
			{ regex("#pragma", regex::optimize), "// #pragma" },
			{ regex("(__)?read_only ", regex::optimize), " " },
			{ regex("(__)?write_only ", regex::optimize), " " },
			{ regex("(__)?read_write ", regex::optimize), " " },
			//{ regex("image1d_t", regex::optimize), "texture<uchar, 4, 0>" }, // TODO
			//{ regex("image2d_t", regex::optimize), "texture<uchar, 4, 0>" }, // TODO
			//{ regex("image3d_t", regex::optimize), "texture<uchar, 4, 0>" }, // TODO
		}
	};
	timer.add("keyword regex", false);
		
	for(const auto& cl2cuda : rx_cl2cuda) {
		cuda_source = regex_replace(cuda_source, cl2cuda.first, cl2cuda.second);
	}
	cuda_source = regex_replace(cuda_source, rx_attributes, "");
	
	// replace as long as there are changes/matches (-> better method for this?)
	static const regex rx_shared_in_inline_func("(inline __device__ )([\\w ]+)\\(([^\\)]*)(__shared__ )([^\\)]*)\\)", regex::optimize);
	size_t src_size = 0;
	do {
		src_size = cuda_source.size();
		cuda_source = regex_replace(cuda_source, rx_shared_in_inline_func, "$1$2($3$5)");
	} while(src_size != cuda_source.size());
	
	// replace "kernel" function by "__global__"
	static const regex rx_cl2cuda_kernel("(__)?kernel", regex::optimize);
	cuda_source = regex_replace(cuda_source, rx_cl2cuda_kernel, "__global__");
	
	// mark all kernels as extern "C" to prevent name mangling
	const string find_str = "__global__", insert_str = "extern \"C\" ";
	const size_t find_len = find_str.size(), insert_len = insert_str.size();
	size_t pos, old_pos = 0;
	while((pos = cuda_source.find(find_str, old_pos)) != string::npos) {
		cuda_source.insert(pos, insert_str);
		old_pos = pos + find_len + insert_len;
	}
	
	// replace all vector constructors
	static const vector<pair<const regex, const string>> rx_vec_types {
		{
			// TODO: proper solution for all vector types
			// "(typen)(...)" -> normal vector constructor "typen(...)"
			{ regex("\\(float2\\)", regex::optimize), "float2" },
			{ regex("\\(float3\\)", regex::optimize), "float3" },
			{ regex("\\(float4\\)", regex::optimize), "float4" },
			{ regex("\\(int2\\)", regex::optimize), "int2" },
			{ regex("\\(int3\\)", regex::optimize), "int3" },
			{ regex("\\(int4\\)", regex::optimize), "int4" },
			{ regex("\\(uint2\\)", regex::optimize), "uint2" },
			{ regex("\\(uint3\\)", regex::optimize), "uint3" },
			{ regex("\\(uint4\\)", regex::optimize), "uint4" },
			{ regex("\\(long2\\)", regex::optimize), "long2" },
			{ regex("\\(long3\\)", regex::optimize), "long3" },
			{ regex("\\(long4\\)", regex::optimize), "long4" },
			{ regex("\\(ulong2\\)", regex::optimize), "ulong2" },
			{ regex("\\(ulong3\\)", regex::optimize), "ulong3" },
			{ regex("\\(ulong4\\)", regex::optimize), "ulong4" },
		}
	};
	for(const auto& rx_vec_type : rx_vec_types) {
		cuda_source = regex_replace(cuda_source, rx_vec_type.first, rx_vec_type.second);
	}
	
	// replace vector accesses
	static const vector<pair<const regex, const string>> rx_vector_op {
		{
			// TODO: find a better (stable) solution for this!
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.xyzw ([\\+\\-\\*/]*)=", regex::optimize), "*((float4*)&$1) $2=" },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.xyz ([\\+\\-\\*/]*)=", regex::optimize), "*((float3*)&$1) $2=" },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.xy ([\\+\\-\\*/]*)=", regex::optimize), "*((float2*)&$1) $2=" },
		}
	};
	for(const auto& rx_vec_op : rx_vector_op) {
		cuda_source = regex_replace(cuda_source, rx_vec_op.first, rx_vec_op.second);
	}
	
	// <regex, components, function?>
	static const vector<tuple<const regex, const size_t, const bool>> rx_vec_accessors {
		{
			// TODO: handle "(*vec_ptr).xyz"
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.(x|y|z|w)(x|y|z|w)(x|y|z|w)(x|y|z|w)", regex::optimize), 4, false },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.(x|y|z|w)(x|y|z|w)(x|y|z|w)", regex::optimize), 3, false },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\.(x|y|z|w)(x|y|z|w)", regex::optimize), 2, false },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\((.*)\\)\\.(x|y|z|w)(x|y|z|w)(x|y|z|w)(x|y|z|w)", regex::optimize), 4, true },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\((.*)\\)\\.(x|y|z|w)(x|y|z|w)(x|y|z|w)", regex::optimize), 3, true },
			{ regex("([\\w\\[\\]\\.\\->_]+)\\((.*)\\)\\.(x|y|z|w)(x|y|z|w)", regex::optimize), 2, true },
		}
	};
	for(const auto& rx_vec_access : rx_vec_accessors) {
		smatch match;
		while(regex_search(cuda_source, match, get<0>(rx_vec_access))) {
			const size_t components = get<1>(rx_vec_access);
			if(match.size() < (components + 2)) {
				continue;
			}
			
			string repl = "get_vector_components_" + size_t2string(components) + "<";
			
			vector<size_t> component_nums(components);
			const auto comp_str_to_num = [](const string& comp_str) -> string {
				return (comp_str == "x" ? "0" : (comp_str == "y" ? "1" : (comp_str == "z" ? "2" : "3")));
			};
			const size_t offset = match.size() - components;
			for(size_t i = 0; i < components; i++) {
				repl += comp_str_to_num(match[offset + i]);
				if(i+1 < components) repl += ", ";
			}
			repl += ">(" + match.str(1) + (get<2>(rx_vec_access) ? ("(" + match.str(2) + ")") : "") + ")";
			
			cuda_source.replace(match.position(), match.length(), repl);
		}
	}
	timer.add("vec regex", false);
	
	// add cuda header source
	cuda_source.insert(0,
					   // add the cuda header and non-regex code to the beginning
					   cuda_header + kernel_source.substr(0, regex_marker_pos) +
					   // also add a dummy ident struct, so it can be replaced by the
					   // actual kernel name later on
					   "struct __oclraster_ident_placeholder {};\n");
	timer.add("end src string", false);
#if 0
	timer.end(true);
#endif
}

#endif
