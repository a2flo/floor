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

#pragma once

#include <floor/device/device_common.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/host/host_common.hpp>
#include <floor/constexpr/sha_256.hpp>

//! Floor Universal Binary ARchive
//!
//! binary format:
//! [magic: char[4] = "FUBA"]
//! [binary format version: uint32_t = 6]
//! [binary count: uint32_t]
//! [FUBAR flags: uint32_t]
//! [binary targets: target_v6[binary count]]
//! [binary offsets: uint64_t[binary count]]
//! [binary toolchain versions: uint32_t[binary count]]
//! [binary SHA-256 hashes: sha_256::hash_t[binary count]]
//! binaries[binary count]... (binary offset #0 points here):
//! # all binaries data is BCM-compressed if FUBAR flags have "is_compressed" set
//!     [function count: uint32_t]
//!     [function info size: uint32_t]
//!     [binary size: uint32_t]
//!     [binary flags: uint32_t]
//!     functions[function count]...:
//!         [function info version: uint32_t = 6]
//!         [type: FUNCTION_TYPE (uint32_t)]
//!         [argument count: uint32_t]
//!         [local size: uint3]
//!         [SIMD-width: uint32_t]
//!         [argument buffer index: uint32_t]
//!         [name: string (0-terminated)]
//!         [args: arg_info[argument count]...]
//!             [size: uint64_t]
//!             [array extent: uint64_t]
//!             [address space: uint32_t]
//!             [access: uint32_t]
//!             [image type: uint32_t]
//!             [flags: uint32_t]
//!     [binary data: uint8_t[binary size]]

namespace fl::universal_binary {
using namespace std::literals;

	//! current version of the binary format
	static constexpr const uint32_t binary_format_version { 6u };
	//! current version of the target format
	static constexpr const uint32_t target_format_version { 6u };
	//! current version of the function info
	static constexpr const uint32_t function_info_version { 6u };
	
	//! target information (64-bit)
	//! NOTE: right now this is still subject to change until said otherwise!
	//!       -> can change without version update
	union __attribute__((packed)) target_v6 {
		
		//! NOTE: due to bitfield and alignment restrictions/requirements, these common variables can't simply be
		//!       put into a base struct or a surrounding union/struct -> put them into every struct manually
#define FLOOR_FUBAR_VERSION_AND_TYPE \
			/*! target format version */ \
			uint64_t version : 4; \
			/*! the compute/graphics backend type, i.e. the main target */ \
			PLATFORM_TYPE type : 4;

		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top
		} common;
		
		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top

			//! major OpenCL target version
			uint64_t major : 6;
			//! minor OpenCL target version
			uint64_t minor : 4;
			
			//! if true, this is a SPIR LLVM binary
			//! if false, this is a SPIR-V binary
			uint64_t is_spir : 1;
			
			//! special device target to enable special workarounds/features
			//! NOTE: this may interact with the OpenCL target version and whether SPIR or SPIR-V is targeted
			enum DEVICE_TARGET : uint64_t {
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
			uint64_t image_depth_support : 1; // implies image_depth_write_support
			uint64_t image_msaa_support : 1; // implies image_msaa_array_support
			uint64_t image_mipmap_support : 1;
			uint64_t image_mipmap_write_support : 1;
			uint64_t image_read_write_support : 1;
			uint64_t double_support : 1;
			uint64_t basic_64_bit_atomics_support : 1;
			uint64_t extended_64_bit_atomics_support : 1;
			uint64_t sub_group_support : 1;
			
			//! required device SIMD width
			//! if 0, no width is assumed
			uint64_t simd_width : 8;
			
			uint64_t _unused : 24;
		} opencl;
		
		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top

			//! major SM target version
			uint64_t sm_major : 6;
			//! minor SM target version
			uint64_t sm_minor : 4;
			//! is SM architecture-accelerated?
			uint64_t sm_aa : 1;
			
			//! major PTX ISA target version
			uint64_t ptx_isa_major : 6;
			//! minor PTX ISA target version
			uint64_t ptx_isa_minor : 4;
			
			//! if true, this is semi-generic PTX
			//! if false, this is a CUBIN
			uint64_t is_ptx : 1;
			
			//! requires use of internal CUDA API,
			//! if 0, this is done in software
			uint64_t image_depth_compare_support : 1;
			
			//! if != 0 and PTX, this restricts/specifies how many registers
			//! can be used when jitting the PTX code
			uint64_t max_registers : 8;
			
			uint64_t _unused : 25;
		} cuda;
		
		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top

			//! major Metal language target version
			uint64_t major : 6;
			//! minor Metal language target version
			uint64_t minor : 4;
			
			//! Apple platform target
			enum PLATFORM_TARGET : uint64_t {
				//! target macOS
				MACOS				= 0u,
				//! target iOS/iPadOS
				IOS					= 1u,
				//! target visionOS
				VISIONOS			= 2u,
				//! target iOS/iPadOS simulator
				IOS_SIMULATOR		= 3u,
				//! target visionOS simulator
				VISIONOS_SIMULATOR	= 4u,
				// TODO: tvOS, ...
			};
			PLATFORM_TARGET platform_target : 4u;
			
			//! special device target to enable special workarounds/features
			enum DEVICE_TARGET : uint64_t {
				//! fully generic target
				GENERIC		= 0u,
				//! target Apple GPUs
				//! NOTE: iOS/visionOS are always APPLE
				APPLE		= 1u,
				//! target AMD GPUs
				AMD			= 2u,
				//! target Intel GPUs
				INTEL		= 3u,
			};
			DEVICE_TARGET device_target : 4u;
			
			//! required device SIMD width
			//! if 0, no width is assumed
			uint64_t simd_width : 8;
			
			//! if set, enables soft-printf support
			//! NOTE: if set, this overrides any global option; if not set, it can be overriden by the global option
			uint64_t soft_printf : 1;
			
			//! if set, enables barycentric coord support
			//! NOTE: always supported on iOS (with Metal3), varies on macOS Metal3/Mac2
			uint64_t barycentric_coord_support : 1;
			
			uint64_t _unused : 28;
		} metal;
		
		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top
			
			//! CPU tier (includes x86 and ARM)
			HOST_CPU_TIER cpu_tier : 16;
			
			uint64_t _unused : 40;
		} host;
		
		struct __attribute__((packed)) {
			FLOOR_FUBAR_VERSION_AND_TYPE // see top

			//! major Vulkan target version
			uint64_t vulkan_major : 6;
			//! minor Vulkan target version
			uint64_t vulkan_minor : 4;
			
			//! major SPIR-V target version
			uint64_t spirv_major : 6;
			//! minor SPIR-V target version
			uint64_t spirv_minor : 4;
			
			//! special device target to enable special workarounds/features
			enum DEVICE_TARGET : uint64_t {
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
			uint64_t double_support : 1;
			uint64_t basic_64_bit_atomics_support : 1;
			uint64_t extended_64_bit_atomics_support : 1;
			
			//! if set, enables soft-printf support
			//! NOTE: if set, this overrides any global option; if not set, it can be overriden by the global option
			uint64_t soft_printf : 1;
			
			//! if set, enables basic 32-bit float atomics support (add/ld/st/xchg)
			uint64_t basic_32_bit_float_atomics_support : 1;
			
			//! if set, enables primitive id support
			uint64_t primitive_id_support : 1;
			
			//! if set, enables barycentric coord support
			uint64_t barycentric_coord_support : 1;
			
			//! if set, enables tessellation support
			uint64_t tessellation_support : 1;
			
			//! required device SIMD width
			//! if 0, no width is assumed
			uint64_t simd_width : 8;
			
			//! max mip level count in [1, 31]
			uint64_t max_mip_levels : 5;
			
			uint64_t _unused : 11;
		} vulkan;

		//! packed value
		uint64_t value { 0u };
		
		bool operator==(const target_v6& cmp) const {
			return (value == cmp.value);
		}
		
#undef FLOOR_FUBAR_VERSION_AND_TYPE // cleanup
	};
	static_assert(sizeof(target_v6) == sizeof(uint64_t));
	
	//! static part of the universal binary archive header (these are the first bytes)
	struct __attribute__((packed)) header_v6 {
		//! magic identifier
		char magic[4] { 'F', 'U', 'B', 'A' }; // floor universal binary archive
		//! == binary_format_version
		uint32_t binary_format_version;
		//! number of contained binaries
		uint32_t binary_count;
		//! FUBAR flags
		struct __attribute__((packed)) flags_t {
			//! are binaries compressed?
			uint32_t is_compressed : 1 { 0u };
			uint32_t _unused_flags : 31 { 0u };
		} flags;
	};
	static_assert(sizeof(header_v6) == sizeof(uint32_t) * 4u);
	
	//! extended/dynamic part of the header
	struct header_dynamic_v6 {
		//! static part of the header
		header_v6 static_header;
		//! binary targets
		std::vector<target_v6> targets;
		//! binary offsets inside the file
		std::vector<uint64_t> offsets;
		//! binary toolchain versions (currently 140000)
		std::vector<uint32_t> toolchain_versions;
		//! binary SHA2-256 hashes
		std::vector<sha_256::hash_t> hashes;
	};
	
	//! per-function information inside a binary (static part)
	struct function_info_v6 {
		//! == function_info_version
		uint32_t function_info_version { 0u };
		//! function type (kernel, fragment, vertex, ...)
		toolchain::FUNCTION_TYPE type { toolchain::FUNCTION_TYPE::NONE };
		//! function flags (uses-soft-printf, ...)
		toolchain::FUNCTION_FLAGS flags { toolchain::FUNCTION_FLAGS::NONE };
		//! number of function arguments
		uint32_t arg_count { 0u };
		//! functions: required local size/dim needed for execution
		uint3 local_size;
		//! functions: required SIMD-width (if non-zero)
		uint32_t simd_width { 0u };
		//! argument buffer: index of the argument buffer in the function
		uint32_t argument_buffer_index { 0u };
	};
	static_assert(sizeof(function_info_v6) == sizeof(uint32_t) * 9u);
	
	//! per-function information inside a binary (dynamic part)
	struct function_info_dynamic_v6 {
		//! static part of the function info
		function_info_v6 static_function_info;
		//! function name
		std::string name;
		//! per-argument specific information
		struct arg_info {
			uint64_t size { 0 };
			uint64_t array_extent { 0u };
			toolchain::ARG_ADDRESS_SPACE address_space { toolchain::ARG_ADDRESS_SPACE::GLOBAL };
			toolchain::ARG_ACCESS access { toolchain::ARG_ACCESS::UNSPECIFIED };
			toolchain::ARG_IMAGE_TYPE image_type { toolchain::ARG_IMAGE_TYPE::NONE };
			toolchain::ARG_FLAG flags { toolchain::ARG_FLAG::NONE };
		};
		static_assert(sizeof(arg_info) == 2u * sizeof(uint64_t) + 4u * sizeof(uint32_t));
		std::vector<arg_info> args;
	};
	
	//! per-binary header (static part)
	struct __attribute__((packed)) binary_v6 {
		//! count of all contained functions
		uint32_t function_count;
		//! size of the function info data
		uint32_t function_info_size;
		//! size of the binary data
		uint32_t binary_size;
		//! binary flags
		struct __attribute__((packed)) flags_t {
			uint32_t _unused_flags : 32 { 0u };
		} flags;
	};
	static_assert(sizeof(binary_v6) == sizeof(uint32_t) * 4);
	
	//! per-binary header (dynamic part)
	struct binary_dynamic_v6 {
		//! static part of the binary header
		binary_v6 static_binary_header;
		//! function info for all contained functions
		std::vector<function_info_dynamic_v6> function_info;
		//! binary data
		std::vector<uint8_t> data;
	};
	
	//! in-memory floor universal binary archive
	struct archive {
		header_dynamic_v6 header;
		std::vector<binary_dynamic_v6> binaries;
	};
	
	//! aliases for current formats
	using target = target_v6;
	using header = header_v6;
	using header_dynamic = header_dynamic_v6;
	using function_info = function_info_v6;
	using function_info_dynamic = function_info_dynamic_v6;
	using binary = binary_v6;
	using binary_dynamic = binary_dynamic_v6;
	
	//! loads a binary archive into memory and returns it if successful (nullptr if not)
	std::unique_ptr<archive> load_archive(const std::string& file_name);
	
	//! loads a binary archive from in-memory data and returns it if successful (nullptr if not)
	std::unique_ptr<archive> load_archive(std::span<const uint8_t> data, const std::string_view file_name_hint = ""sv);
	
	//! loads a binary archive into memory, finds the best matching binaries for the specified
	//! devices and returns them
	//! if an error occurred, ar will be nullptr and dev_binaries will be empty
	struct archive_binaries {
		//! loaded archive
		std::unique_ptr<archive> ar;
		//! matching binaries
		std::vector<std::pair<const binary_dynamic_v6*, const target_v6>> dev_binaries;
	};
	archive_binaries load_dev_binaries_from_archive(const std::string& file_name, const std::vector<const device*>& devices);
	archive_binaries load_dev_binaries_from_archive(const std::string& file_name, const device_context& ctx);
	archive_binaries load_dev_binaries_from_archive(const std::span<const uint8_t> data, const std::vector<const device*>& devices);
	archive_binaries load_dev_binaries_from_archive(const std::span<const uint8_t> data, const device_context& ctx);
	
	//! builds an archive from the given source file/code, with the specified options, for the specified targets,
	//! writing the binary output to the specified destination if successful (returns false if not),
	//! if "use_precompiled_header" is set, a pre-compiled header will be generated and used for each target
	//! NOTE: compile_options::target is ignored for this
	bool build_archive_from_file(const std::string& src_file_name,
								 const std::string& dst_archive_file_name,
								 const toolchain::compile_options& options,
								 const std::vector<target>& targets,
								 const bool use_precompiled_header = false);
	bool build_archive_from_memory(const std::string& src_code,
								   const std::string& dst_archive_file_name,
								   const toolchain::compile_options& options,
								   const std::vector<target>& targets,
								   const bool use_precompiled_header = false);
	
	//! finds the best matching binary for the specified device inside the specified archive,
	//! returns nullptr if no compatible binary has been found at all
	std::pair<const binary_dynamic_v6*, const target_v6>
	find_best_match_for_device(const device& dev,
							   const archive& ar);
	
	//! translates universal binary function info to toolchain function info
	std::vector<toolchain::function_info> translate_function_info(const std::pair<const binary_dynamic_v6*, const target_v6>& bin);
	
} // namespace fl::universal_binary
