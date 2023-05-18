/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_disassembly.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <regex>
#include <filesystem>

namespace vulkan_disassembly {

static void disassemble_nvidia(const std::string& identifier, std::span<const uint8_t> nv_pipeline_data) {
	/// format
	// header:
	// [entry count - uint32_t]
	//
	// entry:
	// [UUID? - 16 bytes]
	// [payload size - uint32_t]
	// [CPKV magic - char[4]]
	// [unknown - uint32_t]
	// [same UUID? - 16 bytes]
	// [unknown - uint32_t]
	// [unknown - uint32_t]
	// [payload size - uint32_t]
	// [unknown - uint32_t]
	//
	// payload:
	// [decompressed size - uint32_t]
	// [zstd magic - uint32_t]
	// [zstd data ...]
	
	struct __attribute__((packed)) nv_pipeline_header_t {
		uint32_t entry_count;
	};
	static_assert(sizeof(nv_pipeline_header_t) == 4);
	
	if (nv_pipeline_data.size_bytes() < sizeof(nv_pipeline_header_t)) {
		log_error("NVIDIA pipeline data is smaller than expected (require at least $' bytes, got $') in $",
				  sizeof(nv_pipeline_header_t), nv_pipeline_data.size_bytes(), identifier);
		return;
	}
	const auto entry_count = ((const nv_pipeline_header_t*)nv_pipeline_data.data())->entry_count;
	
	struct __attribute__((packed)) nv_entry_header_t {
		uint8_t uuid_0[16];
		uint32_t payload_size_0;
		uint32_t cpkv_magic;
		uint32_t unknown_0;
		uint8_t uuid_1[16];
		uint32_t unknown_1;
		uint32_t unknown_2;
		uint32_t payload_size_1;
		uint32_t unknown_3;
	};
	static_assert(sizeof(nv_entry_header_t) == 60);
	
	struct __attribute__((packed)) nv_entry_payload_header_t {
		uint32_t decompressed_size;
		uint32_t zstd_magic;
	};
	static_assert(sizeof(nv_entry_payload_header_t) == 8);
	
	nv_pipeline_data = nv_pipeline_data.subspan(sizeof(nv_pipeline_header_t));
	for (uint32_t entry_idx = 0; entry_idx < entry_count; ++entry_idx) {
		const auto identifier_with_suffix = identifier + (entry_count > 1 ? "_entry_" + to_string(entry_idx) : "");
		
		if (nv_pipeline_data.size_bytes() < sizeof(nv_entry_header_t)) {
			log_error("NVIDIA pipeline entry data is smaller than expected @ entry #$ (require at least $' bytes, got $') in $",
					  entry_idx, sizeof(nv_entry_header_t), nv_pipeline_data.size_bytes(), identifier_with_suffix);
			return;
		}
		const auto& entry_header = *(const nv_entry_header_t*)nv_pipeline_data.data();
		nv_pipeline_data = nv_pipeline_data.subspan(sizeof(nv_entry_header_t));
		
		if (entry_header.cpkv_magic != 0x564B5043u) {
			log_error("invalid CPKV magic in $", identifier_with_suffix);
			return;
		}
		
		if (nv_pipeline_data.size_bytes() < entry_header.payload_size_0) {
			log_error("NVIDIA pipeline entry payload data is smaller than expected @ entry #$ (require at least $' bytes, got $') in $",
					  entry_idx, entry_header.payload_size_0, nv_pipeline_data.size_bytes(), identifier_with_suffix);
			return;
		}
		const auto entry_span = nv_pipeline_data.subspan(0, entry_header.payload_size_0);
		nv_pipeline_data = nv_pipeline_data.subspan(entry_header.payload_size_0);
		const auto& payload_header = *(const nv_entry_payload_header_t*)entry_span.data();
		
		if (payload_header.zstd_magic != 0xFD2FB528u) {
			log_error("invalid zstd magic in $", identifier_with_suffix);
			return;
		}
		
		// dump zstd file and decompress it
		if (!file_io::buffer_to_file(identifier_with_suffix + ".zstd", (const char*)(entry_span.data() + 4u /* ignore decompressed_size */),
									 entry_header.payload_size_1 - 4u /* ignore decompressed_size */)) {
			log_error("failed to dump zstd pipeline payload in $", identifier_with_suffix);
			return;
		}
		string zstd_output;
		core::system("zstd -d '" + identifier_with_suffix + ".zstd' -o '" + identifier_with_suffix + ".nvbin' -v --force 2>&1", zstd_output);
#if !defined(__APPLE__)
		{
			error_code ec {};
			filesystem::remove(identifier_with_suffix + ".zstd", ec);
		}
#endif
		
		// validate deompressed size
		// expexcting: "identifier_with_suffix.zstd: N bytes"
		static const regex rx_zstd_decompressed_bytes(": ([0-9]+) bytes");
		smatch regex_result;
		if (!regex_search(zstd_output, regex_result, rx_zstd_decompressed_bytes) || regex_result.empty()) {
			log_error("failed to decompress zstd data in $:\n$", identifier_with_suffix, zstd_output);
			return;
		}
		const auto zstd_decompressed_bytes = strtoull(regex_result[1].str().c_str(), nullptr, 10);
		if (zstd_decompressed_bytes != payload_header.decompressed_size) {
			log_error("unexpected decompressed size (expected $', got $') in $",
					  payload_header.decompressed_size, zstd_decompressed_bytes, identifier_with_suffix);
			return;
		}
		
		// we should now have a "NVDANVVM" binary, consisting of the actual GPU binary (NVuc) and LLVM/NVVM bitcode (BC)
		auto [nvvm_binary_data, nvvm_binary_size] = file_io::file_to_buffer(identifier_with_suffix + ".nvbin");
#if !defined(__APPLE__)
		{
			error_code ec {};
			filesystem::remove(identifier_with_suffix + ".nvbin", ec);
		}
#endif
		
		struct __attribute__((packed)) nvuc_header_t {
			uint32_t nvuc_magic;
			uint32_t unknown_0;
			uint32_t unknown_1;
			uint32_t unknown_2;
			uint32_t unknown_3;
			uint32_t unknown_4;
			uint32_t nvuc_size;
			// probably more, but this is sufficient ...
		};
		
		if (!nvvm_binary_data || nvvm_binary_size < (8u + sizeof(nvuc_header_t))) {
			log_error("failed to load/create NVVM binary or created binary is too small, in $", identifier_with_suffix);
			return;
		}
		if (strncmp((const char*)nvvm_binary_data.get(), "NVDANVVM", 8) != 0) {
			log_error("invalid NVDA NVVM binary/header in $", identifier_with_suffix);
			return;
		}
		const auto& nvuc_header = *(const nvuc_header_t*)(nvvm_binary_data.get() + 8u);
		if (nvuc_header.nvuc_magic != 0x6375564Eu) {
			log_error("invalid NVuc magic in $", identifier_with_suffix);
			return;
		}
		if (nvuc_header.nvuc_size > nvvm_binary_size - 8u) {
			log_error("NVuc size is smaller than expected (have $' bytes, but header says size is $') in $",
					  nvvm_binary_size - 8u, nvuc_header.nvuc_size, identifier_with_suffix);
			return;
		}
		
		// dump and disassemble NVuc
		if (!file_io::buffer_to_file(identifier_with_suffix + ".nvuc", (const char*)(nvvm_binary_data.get() + 8u), nvuc_header.nvuc_size)) {
			log_error("failed to dump zstd pipeline payload in $", identifier_with_suffix);
			return;
		}
		string nvdisasm_output;
		core::system("nvdisasm -g '" + identifier_with_suffix + ".nvuc'", nvdisasm_output);
		if (!file_io::string_to_file(identifier_with_suffix + ".nvucdis", nvdisasm_output)) {
			log_error("failed to write disassembled NVuc data in $", identifier_with_suffix);
			return;
		}
		
		// dump and disassemble LLVM/NVVM bitcode
		// NOTE: I currently don't see a direct way to get at the BC offset, so we need to find it
		const auto post_nvuc_start_ptr = (nvvm_binary_data.get() + 8u + nvuc_header.nvuc_size);
		const auto post_nvuc_end_ptr = (nvvm_binary_data.get() + nvvm_binary_size);
		static constexpr const uint8_t bc_magic[4] { 'B', 'C', 0xC0, 0xDE };
		const auto bc_start_ptr = search(post_nvuc_start_ptr, post_nvuc_end_ptr, &bc_magic[0], &bc_magic[3] + 1);
		if (bc_start_ptr == post_nvuc_end_ptr) {
			log_error("failed to find NVVM bitcode start in $", identifier_with_suffix);
			return;
		}
		
		// BC must be aligned to 4 bytes and end on a 4-byte zero value
		string_view bc((const char*)bc_start_ptr, (size_t)std::distance(bc_start_ptr, post_nvuc_end_ptr));
		const auto spirv_str_pos = bc.rfind("SPIR-V");
		if (spirv_str_pos == string::npos) {
			log_error("failed to find end of NVVM bitcode in $", identifier_with_suffix);
			return;
		}
		auto bc_size = spirv_str_pos + 7 /* SPIR-V with \0 */;
		bc_size += (bc_size % 4u != 0u ? (4u - (bc_size % 4u)) : 0u) + 4u; // alignment + zero val
		
		if (!file_io::buffer_to_file(identifier_with_suffix + ".bc", (const char*)bc_start_ptr, (uint64_t)bc_size)) {
			log_error("failed to dump NVVM bitcode in $", identifier_with_suffix);
			return;
		}
		
		// can just use the llvm-dis from the floor toolchain here
		core::system(floor::get_vulkan_dis() + " -o '" + identifier_with_suffix + ".ll' '" + identifier_with_suffix + ".bc'");
#if !defined(__APPLE__)
		{
			error_code ec {};
			filesystem::remove(identifier_with_suffix + ".bc", ec);
		}
#endif
	}
}

void disassemble(const vulkan_device& dev, const std::string& identifier, const VkPipeline& pipeline, const VkPipelineCache* cache) {
	// query initial props
	const VkPipelineInfoKHR exec_props_query_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
		.pNext = nullptr,
		.pipeline = pipeline,
	};
	uint32_t exec_props_count = 0;
	dev.vulkan_get_pipeline_executable_properties(dev.device, &exec_props_query_info, &exec_props_count, nullptr);
	if (exec_props_count > 0) {
		vector<VkPipelineExecutablePropertiesKHR> exec_props(exec_props_count, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR,
			.pNext = nullptr,
		});
		dev.vulkan_get_pipeline_executable_properties(dev.device, &exec_props_query_info, &exec_props_count, exec_props.data());
		
		stringstream pipeline_info;
		pipeline_info << "pipeline info: " << identifier << "\n\n";
		for (uint32_t exec_idx = 0; exec_idx < exec_props_count; ++exec_idx) {
			const auto& exec_prop = exec_props[exec_idx];
			pipeline_info << exec_prop.name << ": " << exec_prop.description << " (sub-group size " << exec_prop.subgroupSize << ")\n";
			
			// query IR
			const VkPipelineExecutableInfoKHR exec_info {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,
				.pNext = nullptr,
				.pipeline = pipeline,
				.executableIndex = exec_idx,
			};
			uint32_t ir_count = 0;
			dev.vulkan_get_pipeline_executable_internal_representation(dev.device, &exec_info, &ir_count, nullptr);
			pipeline_info << "IR count: " << ir_count << "\n";
			if (ir_count > 0) {
				vector<VkPipelineExecutableInternalRepresentationKHR> ir_data(ir_count, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR,
					.pNext = nullptr
				});
				dev.vulkan_get_pipeline_executable_internal_representation(dev.device, &exec_info, &ir_count, ir_data.data());
				vector<unique_ptr<uint8_t[]>> ir_data_storage;
				for (auto& ir : ir_data) {
					ir_data_storage.emplace_back(make_unique<uint8_t[]>(ir.dataSize));
					ir.pData = ir_data_storage.back().get();
				}
				dev.vulkan_get_pipeline_executable_internal_representation(dev.device, &exec_info, &ir_count, ir_data.data());
				for (const auto& ir : ir_data) {
					file_io::buffer_to_file(identifier + "_ir_" + ir.name + ".txt", (const char*)ir.pData, ir.dataSize);
				}
			}
			
			// query statistics
			uint32_t stats_count = 0;
			dev.vulkan_get_pipeline_executable_statistics(dev.device, &exec_info, &stats_count, nullptr);
			pipeline_info << "stats count: " << stats_count << "\n";
			if (stats_count > 0) {
				vector<VkPipelineExecutableStatisticKHR> stats(stats_count, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR,
					.pNext = nullptr,
				});
				dev.vulkan_get_pipeline_executable_statistics(dev.device, &exec_info, &stats_count, stats.data());
				for (uint32_t stat_idx = 0; stat_idx < stats_count; ++stat_idx) {
					pipeline_info << "\t" << stats[stat_idx].name << " (" << stats[stat_idx].description << "): ";
					switch (stats[stat_idx].format) {
						case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
							pipeline_info << stats[stat_idx].value.b32 << "\n";
							break;
						case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
							pipeline_info << stats[stat_idx].value.i64 << "\n";
							break;
						case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
							pipeline_info << stats[stat_idx].value.u64 << "\n";
							break;
						case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
							pipeline_info << stats[stat_idx].value.f64 << "\n";
							break;
						default:
							break;
					}
				}
			}
			pipeline_info << "\n";
		}
		file_io::string_to_file(identifier + "_info.txt", pipeline_info.str());
	}
	
	// retrieve cache binary
	if (cache) {
		do {
			size_t cache_size = 0;
			vkGetPipelineCacheData(dev.device, *cache, &cache_size, nullptr);
			if (cache_size < sizeof(VkPipelineCacheHeaderVersionOne)) {
				break;
			}
			
			auto cache_data = make_unique<uint32_t[]>((cache_size + 3u) / 4u);
			if (vkGetPipelineCacheData(dev.device, *cache, &cache_size, cache_data.get()) != VK_SUCCESS ||
				cache_size < sizeof(VkPipelineCacheHeaderVersionOne)) {
				log_error("failed to retrieve pipeline cache data ($)", identifier);
				break;
			}
			
			const auto& header = *(const VkPipelineCacheHeaderVersionOne*)cache_data.get();
			if (header.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE ||
				header.headerSize != sizeof(VkPipelineCacheHeaderVersionOne)) {
				log_error("unknown pipeline cache header ($)", identifier);
				break;
			}
			
			// dump the binary itself
			const auto data_size = cache_size - header.headerSize;
			file_io::buffer_to_file(identifier + ".bin", (const char*)cache_data.get() + sizeof(VkPipelineCacheHeaderVersionOne), data_size);
			
			// vendor specific handling
			if (dev.vendor == COMPUTE_VENDOR::NVIDIA && header.vendorID == 0x10de) {
				disassemble_nvidia(identifier, std::span<const uint8_t> { ((const uint8_t*)cache_data.get()) + header.headerSize, data_size });
				
				// no longer need the bin file in NVIDIAs case
#if !defined(__APPLE__)
				{
					error_code ec {};
					filesystem::remove(identifier + ".bin", ec);
				}
#endif
			}
			// TODO: AMD: bin contains ELF, ISA, LLVM IR
		} while (false);
	}
}

} // namespace vulkan_disassembly

#endif
