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

#ifndef __FLOOR_COMPUTE_UNIVERSAL_BINARY_HPP__
#define __FLOOR_COMPUTE_UNIVERSAL_BINARY_HPP__

#include <floor/compute/compute_common.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/constexpr/sha_256.hpp>

//! Floor Universal Binary ARchive
//!
//! binary format:
//! [magic: char[4] = "FUBA"]
//! [binary format version: uint32_t = 2]
//! [binary count: uint32_t]
//! [binary targets: target_v2[binary count]]
//! [binary offsets: uint64_t[binary count]]
//! [binary toolchain versions: uint32_t[binary count]]
//! [binary SHA-256 hashes: sha_256::hash_t[binary count]]
//! binaries[binary count]... (binary offset #0 points here):
//!     [function count: uint32_t]
//!     [function info size: uint32_t]
//!     [binary size: uint32_t]
//!     functions[function count]...:
//!         [function info version: uint32_t = 2]
//!         [type: FUNCTION_TYPE (uint32_t)]
//!         [argument count: uint32_t]
//!         [local size: uint3]
//!         [name: string (0-terminated)]
//!         [args: arg_info[argument count]/uint64_t[argument count]]
//!     [binary data: uint8_t[binary size]]

namespace universal_binary {
	//! current version of the binary format
	static constexpr const uint32_t binary_format_version { 2u };
	//! current version of the target format
	static constexpr const uint32_t target_format_version { 2u };
	//! current version of the function info
	static constexpr const uint32_t function_info_version { 2u };
	
	//! target information (64-bit)
	//! NOTE: right now this is still subject to change until said otherwise!
	//!       -> can change without version update
	union __attribute__((packed)) target_v2 {
		struct __attribute__((packed)) {
			//! target format version
			uint32_t version : 4;
			//! the compute/graphics backend type, i.e. the main target
			COMPUTE_TYPE type : 4;
			//! sub-target information (56-bit)
			//! this corresponds to "type" and only one can be active
			union __attribute__((packed)) {
				struct __attribute__((packed)) {
					//! major OpenCL target version
					uint32_t major : 6;
					//! minor OpenCL target version
					uint32_t minor : 4;
					
					//! if true, this is a SPIR LLVM binary
					//! if false, this is a SPIR-V binary
					uint32_t is_spir : 1;
					
					//! special device target to enable special workarounds/features
					//! NOTE: this may interact with the OpenCL target version and whether SPIR or SPIR-V is targeted
					enum DEVICE_TARGET : uint32_t {
						//! fully generic target
						GENERIC			= 0u,
						//! target CPU-specific code
						GENERIC_CPU		= 1u,
						//! target GPU-specific code
						GENERIC_GPU		= 2u,
						//! target Intel CPUs
						INTEL_CPU		= 3u,
						//! target Intel GPUs
						INTEL_GPU		= 4u,
						//! target AMD CPUs
						AMD_CPU			= 5u,
						//! target AMD GPUs
						AMD_GPU			= 6u,
					};
					DEVICE_TARGET device_target : 4u;
					
					//! optional capabilities
					uint32_t image_depth_support : 1; // implies image_depth_write_support
					uint32_t image_msaa_support : 1; // implies image_msaa_array_support
					uint32_t image_mipmap_support : 1;
					uint32_t image_mipmap_write_support : 1;
					uint32_t image_read_write_support : 1;
					uint32_t double_support : 1;
					uint32_t basic_64_bit_atomics_support : 1;
					uint32_t extended_64_bit_atomics_support : 1;
					uint32_t sub_group_support : 1;
					
					//! required device SIMD width
					//! if 0, no width is assumed
					uint32_t simd_width : 8;
					
					uint64_t _unused : 24;
				} opencl;
				
				struct __attribute__((packed)) {
					//! major sm/cc target version
					uint32_t sm_major : 6;
					//! minor sm/cc target version
					uint32_t sm_minor : 4;
					
					//! major PTX ISA target version
					uint32_t ptx_isa_major : 6;
					//! minor PTX ISA target version
					uint32_t ptx_isa_minor : 4;
					
					//! if true, this is semi-generic PTX
					//! if false, this is a CUBIN
					uint32_t is_ptx : 1;
					
					//! requires use of internal CUDA API,
					//! if 0, this is done in software
					uint32_t image_depth_compare_support : 1;
					
					//! if != 0 and PTX, this restricts/specifies how many registers
					//! can be used when jitting the PTX code
					uint32_t max_registers : 8;
					
					uint64_t _unused : 26;
				} cuda;
				
				struct __attribute__((packed)) {
					//! major Metal target version
					uint32_t major : 6;
					//! minor Metal target version
					uint32_t minor : 4;
					
					//! if true, this target iOS
					//! if false, this target macOS/OS X
					uint32_t is_ios : 1;
					
					//! special device target to enable special workarounds/features
					enum DEVICE_TARGET : uint32_t {
						//! fully generic target
						//! NOTE: iOS is always GENERIC
						GENERIC		= 0u,
						//! target NVIDIA GPUs
						NVIDIA		= 1u,
						//! target AMD GPUs
						AMD			= 2u,
						//! target Intel GPUs
						INTEL		= 3u,
					};
					DEVICE_TARGET device_target : 4u;
					
					//! required device SIMD width
					//! if 0, no width is assumed
					uint32_t simd_width : 8;
					
					uint64_t _unused : 33;
				} metal;
				
				struct __attribute__((packed)) {
					//! CPU architecture target
					enum CPU_TARGET : uint32_t {
						//! aka AMD64/Intel64
						X64		= 0u,
						//! aka ARMv8-A
						ARM64	= 1u,
					};
					CPU_TARGET cpu_target : 4u;
					
					//! vector extension support
					//! NOTE: x64 with SSE2 is the minimum requirement
					//! TODO: ARM NEON?
					enum VECTOR_TARGET : uint32_t {
						SSE2	= 0u,
						SSE3	= 1u,
						SSSE3	= 2u,
						SSE4_1	= 3u,
						SSE4_2	= 4u,
						AVX		= 5u,
						//! NOTE: implies FMA3 and F16C support
						AVX2	= 6u,
						//! NOTE: implies F, CD, VL, DQ, BW support
						AVX_512	= 7u,
					};
					VECTOR_TARGET vector_target : 8;
					
					uint64_t _unused : 44;
				} host;
				
				struct __attribute__((packed)) {
					//! major Vulkan target version
					uint32_t vulkan_major : 6;
					//! minor Vulkan target version
					uint32_t vulkan_minor : 4;
					
					//! major SPIR-V target version
					uint32_t spirv_major : 6;
					//! minor SPIR-V target version
					uint32_t spirv_minor : 4;
					
					//! special device target to enable special workarounds/features
					enum DEVICE_TARGET : uint32_t {
						//! fully generic target
						GENERIC		= 0u,
						//! target NVIDIA GPUs
						NVIDIA		= 1u,
						//! target AMD GPUs
						AMD			= 2u,
						//! target Intel GPUs
						INTEL		= 3u,
					};
					DEVICE_TARGET device_target : 4u;
					
					//! optional capabilities
					uint32_t double_support : 1;
					uint32_t basic_64_bit_atomics_support : 1;
					uint32_t extended_64_bit_atomics_support : 1;
					
					uint64_t _unused : 29;
				} vulkan;
			};
		};
		uint64_t value;
	};
	static_assert(sizeof(target_v2) == sizeof(uint64_t));
	
	//! static part of the universal binary archive header (these are the first bytes)
	struct __attribute__((packed)) header_v2 {
		//! magic identifier
		char magic[4] { 'F', 'U', 'B', 'A' }; // floor universal binary archive
		//! == binary_format_version
		uint32_t binary_format_version;
		//! number of contained binaries
		uint32_t binary_count;
	};
	static_assert(sizeof(header_v2) == sizeof(uint32_t) * 3u);
	
	//! extended/dynamic part of the header
	struct header_dynamic_v2 {
		//! static part of the header
		header_v2 static_header;
		//! binary targets
		vector<target_v2> targets;
		//! binary offsets inside the file
		vector<uint64_t> offsets;
		//! binary LLVM toolchain versions (currently 40000)
		vector<uint32_t> toolchain_versions;
		//! binary SHA2-256 hashes
		vector<sha_256::hash_t> hashes;
	};
	
	//! per-function information inside a binary (static part)
	struct function_info_v2 {
		//! == function_info_version
		uint32_t function_info_version;
		//! function type (kernel, fragment, vertex, ...)
		llvm_toolchain::function_info::FUNCTION_TYPE type;
		//! function flags (uses-soft-printf, ...)
		llvm_toolchain::function_info::FUNCTION_FLAGS flags;
		//! number of function arguments
		uint32_t arg_count;
		//! required local size/dim needed for execution
		uint3 local_size;
	};
	static_assert(sizeof(function_info_v2) == sizeof(uint32_t) * 7u);
	
	//! per-function information inside a binary (dynamic part)
	struct function_info_dynamic_v2 {
		//! static part of the function info
		function_info_v2 static_function_info;
		//! function name
		string name;
		//! per-argument specific information
		//! -> FLOOR_METADATA
		union arg_info {
			struct {
				uint32_t argument_size;
				llvm_toolchain::function_info::ARG_ADDRESS_SPACE address_space : 3;
				uint32_t _unused_0 : 5;
				llvm_toolchain::function_info::ARG_IMAGE_TYPE image_type : 8;
				llvm_toolchain::function_info::ARG_IMAGE_ACCESS image_access : 2;
				uint32_t _unused_1 : 6;
				llvm_toolchain::function_info::SPECIAL_TYPE special_type : 8;
			};
			uint64_t value;
		};
		static_assert(sizeof(arg_info) == sizeof(uint64_t));
		vector<arg_info> args;
	};
	
	//! per-binary header (static part)
	struct __attribute__((packed)) binary_v2 {
		//! count of all contained functions
		uint32_t function_count;
		//! size of the function info data
		uint32_t function_info_size;
		//! size of the binary data
		uint32_t binary_size;
	};
	static_assert(sizeof(binary_v2) == sizeof(uint32_t) * 3);
	
	//! per-binary header (dynamic part)
	struct binary_dynamic_v2 {
		//! static part of the binary header
		binary_v2 static_binary_header;
		//! function info for all contained functions
		vector<function_info_dynamic_v2> functions;
		//! binary data
		vector<uint8_t> data;
	};
	
	//! in-memory floor universal binary archive
	struct archive {
		header_dynamic_v2 header;
		vector<binary_dynamic_v2> binaries;
	};
	
	//! aliases for current formats
	using target = target_v2;
	using header = header_v2;
	using header_dynamic = header_dynamic_v2;
	using function_info = function_info_v2;
	using function_info_dynamic = function_info_dynamic_v2;
	using binary = binary_v2;
	using binary_dynamic = binary_dynamic_v2;
	
	//! loads a binary archive into memory and returns it if successful (nullptr if not)
	unique_ptr<archive> load_archive(const string& file_name);
	
	//! loads a binary archive into memory, finds the best matching binaries for the specified
	//! devices and returns them
	//! if an error occurred, ar will be nullptr and dev_binaries will be empty
	struct archive_binaries {
		//! loaded archive
		unique_ptr<archive> ar;
		//! matching binaries
		vector<pair<const binary_dynamic_v2*, const target_v2>> dev_binaries;
	};
	archive_binaries load_dev_binaries_from_archive(const string& file_name, const vector<const compute_device*>& devices);
	archive_binaries load_dev_binaries_from_archive(const string& file_name, const compute_context& ctx);
	
	//! builds an archive from the given source file/code, with the specified options, for the specified targets,
	//! writing the binary output to the specified destination if successful (returns false if not)
	//! NOTE: compile_options::target is ignored for this
	bool build_archive_from_file(const string& src_file_name,
								 const string& dst_archive_file_name,
								 const llvm_toolchain::compile_options& options,
								 const vector<target>& targets);
	bool build_archive_from_memory(const string& src_code,
								   const string& dst_archive_file_name,
								   const llvm_toolchain::compile_options& options,
								   const vector<target>& targets);
	
	//! finds the best matching binary for the specified device inside the specified archive,
	//! returns nullptr if no compatible binary has been found at all
	pair<const binary_dynamic_v2*, const target_v2>
	find_best_match_for_device(const compute_device& dev,
							   const archive& ar);
	
	//! translates universal binary function info to LLVM toolchain function info
	vector<llvm_toolchain::function_info> translate_function_info(const vector<function_info_dynamic_v2>& functions);
	
	
}

#endif
