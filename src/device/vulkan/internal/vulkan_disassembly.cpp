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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/cpp_ext.hpp>
#include "vulkan_disassembly.hpp"
#include "vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/host/elf_binary.hpp>
#include <regex>
#include <filesystem>
#include <cassert>

namespace fl::vulkan_disassembly {

struct file_source_mapping_t {
	//! verbatim source code
	std::string source;
	//! end position of each line in "source"
	std::vector<uint32_t> lines;
};

static safe_mutex source_file_mappings_lock;
static std::unordered_map<std::string, std::unique_ptr<file_source_mapping_t>> source_file_mappings GUARDED_BY(source_file_mappings_lock);

static std::string get_code_line(const file_source_mapping_t& mapping, const uint32_t line) {
	if (line == 0 || line >= mapping.lines.size()) {
		return {};
	}
	
	const auto line_start = mapping.lines[line - 1] + 1, line_end = mapping.lines[line];
	const auto line_length = line_end - line_start;
	return mapping.source.substr(line_start, line_length);
}

static const file_source_mapping_t* load_and_map_source(const std::string& file_name) REQUIRES(!source_file_mappings_lock) {
	{
		GUARD(source_file_mappings_lock);
		const auto mapping_iter = source_file_mappings.find(file_name);
		if (mapping_iter != source_file_mappings.end()) {
			return mapping_iter->second.get();
		}
		// else: need to load it
	}
	
	// load file
	file_source_mapping_t ret {};
	do {
		if (!file_io::file_to_string(file_name, ret.source)) {
			break;
		}
		if (ret.source.empty()) {
			break;
		}
		
		// map source
		// -> we will only need to remove \r characters here (replace \r\n by \n and replace single \r chars by \n)
		ret.lines.emplace_back(0); // line #1 start
		for (auto begin_iter = ret.source.begin(), end_iter = ret.source.end(), iter = begin_iter; iter != end_iter; ++iter) {
			if (*iter == '\n' || *iter == '\r') {
				if (*iter == '\r') {
					auto next_iter = iter + 1;
					if (next_iter != end_iter && *next_iter == '\n') {
						// replace \r\n with single \n (erase \r)
						iter = ret.source.erase(iter); // iter now at '\n'
						// we now have a new end and begin iter
						end_iter = ret.source.end();
						begin_iter = ret.source.begin();
					} else {
						// single \r -> \n replace
						*iter = '\n';
					}
				}
				// else: \n
				
				// add newline position
				ret.lines.emplace_back(std::distance(begin_iter, iter));
			}
		}
		// also insert the "<eof> newline"
		ret.lines.emplace_back(ret.source.size());
	} while (false);
	
	{
		GUARD(source_file_mappings_lock);
		
		// check if some one before us already added this file now
		const auto mapping_iter = source_file_mappings.find(file_name);
		if (mapping_iter != source_file_mappings.end()) {
			return mapping_iter->second.get();
		}
		
		// else: add to map
		if (ret.lines.empty()) {
			source_file_mappings.emplace(file_name, nullptr);
			return nullptr;
		} else {
			auto mapping_obj = std::make_unique<file_source_mapping_t>(std::move(ret));
			auto ret_ptr = mapping_obj.get();
			source_file_mappings.emplace(file_name, std::move(mapping_obj));
			return ret_ptr;
		}
	}
}

static void disassemble_nvidia(const std::string& identifier, std::span<const uint8_t> nv_pipeline_data) REQUIRES(!source_file_mappings_lock) {
	/// format
	// header:
	// [entry count - uint32_t]
	//
	// entry:
	// [shader-hash - 8 bytes]
	// [unknown hash? - 8 bytes]
	// [payload size - uint32_t]
	// [CPKV magic - char[4]]
	// [unknown - uint32_t]
	// [same shader-hash - 8 bytes]
	// [unknown hash? - 8 bytes]
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
		uint8_t shader_hash_0[8];
		uint8_t unknown_hash_0[8];
		uint32_t payload_size_0;
		uint32_t cpkv_magic;
		uint32_t unknown_0;
		uint8_t shader_hash_1[8];
		uint8_t unknown_hash_1[8];
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
		const auto identifier_with_suffix = identifier + (entry_count > 1 ? "_entry_" + std::to_string(entry_idx) : "");
		
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
		std::string zstd_output;
		core::system("zstd -d '" + identifier_with_suffix + ".zstd' -o '" + identifier_with_suffix + ".nvbin' -v --force 2>&1", zstd_output);
		{
			std::error_code ec {};
			std::filesystem::remove(identifier_with_suffix + ".zstd", ec);
		}
		
		// validate decompressed size
		// expecting: "identifier_with_suffix.zstd: N bytes"
		static const std::regex rx_zstd_decompressed_bytes(": ([0-9]+) bytes");
		std::smatch regex_result;
		if (!std::regex_search(zstd_output, regex_result, rx_zstd_decompressed_bytes) || regex_result.empty()) {
			log_error("failed to decompress zstd data in $:\n$", identifier_with_suffix, zstd_output);
			return;
		}
		const auto zstd_decompressed_bytes = strtoull(regex_result[1].str().c_str(), nullptr, 10);
		if (zstd_decompressed_bytes != payload_header.decompressed_size) {
			log_error("unexpected decompressed size (expected $', got $') in $",
					  payload_header.decompressed_size, zstd_decompressed_bytes, identifier_with_suffix);
			return;
		}
		
		// we should now have a "NVDANVVM" binary, consisting of
		//  * the actual GPU code (NVuc)
		//    * note that if debug info is enabled, this also contains line info, but nvdisasm won't print it?
		//  * some unknown junk data? maybe compilation binary info? -> can't determine its size
		//  * LLVM/NVVM bitcode (BC)
		//  * when debug info is available: the full GPU binary (ELF) that contains actual debug info that nvdisasm can print
		auto [nvvm_binary_data, nvvm_binary_size] = file_io::file_to_buffer(identifier_with_suffix + ".nvbin");
		{
			std::error_code ec {};
			std::filesystem::remove(identifier_with_suffix + ".nvbin", ec);
		}
		
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
		
		static constexpr const std::string_view nvda_nvvm_magic { "NVDANVVM" };
		static constexpr const auto nvda_nvvm_magic_length = nvda_nvvm_magic.size();
		static_assert(nvda_nvvm_magic_length == 8u);
		
		if (!nvvm_binary_data || nvvm_binary_size < (nvda_nvvm_magic_length + sizeof(nvuc_header_t))) {
			log_error("failed to load/create NVVM binary or created binary is too small, in $", identifier_with_suffix);
			return;
		}
		if (strncmp((const char*)nvvm_binary_data.get(), nvda_nvvm_magic.data(), nvda_nvvm_magic_length) != 0) {
			log_error("invalid NVDA NVVM binary/header in $", identifier_with_suffix);
			return;
		}
		const auto& nvuc_header = *(const nvuc_header_t*)(nvvm_binary_data.get() + nvda_nvvm_magic_length);
		if (nvuc_header.nvuc_magic != 0x6375564Eu) {
			log_error("invalid NVuc magic in $", identifier_with_suffix);
			return;
		}
		if (nvuc_header.nvuc_size > nvvm_binary_size - nvda_nvvm_magic_length) {
			log_error("NVuc size is smaller than expected (have $' bytes, but header says size is $') in $",
					  nvvm_binary_size - nvda_nvvm_magic_length, nvuc_header.nvuc_size, identifier_with_suffix);
			return;
		}
		
		// dump and disassemble NVuc
		if (!file_io::buffer_to_file(identifier_with_suffix + ".nvuc", (const char*)(nvvm_binary_data.get() + nvda_nvvm_magic_length),
									 nvuc_header.nvuc_size)) {
			log_error("failed to dump zstd pipeline payload in $", identifier_with_suffix);
			return;
		}
		std::string nvdisasm_output;
		core::system("nvdisasm '" + identifier_with_suffix + ".nvuc'", nvdisasm_output);
		if (!file_io::string_to_file(identifier_with_suffix + ".nvucdis", nvdisasm_output)) {
			log_error("failed to write disassembled NVuc data in $", identifier_with_suffix);
			return;
		}
		
		// dump and disassemble LLVM/NVVM bitcode
		// NOTE: I currently don't see a direct way to get at the BC offset, so we need to find it
		const auto post_nvuc_start_ptr = (nvvm_binary_data.get() + nvda_nvvm_magic_length + nvuc_header.nvuc_size);
		const auto post_nvuc_end_ptr = (nvvm_binary_data.get() + nvvm_binary_size);
		static constexpr const uint8_t bc_magic[4] { 'B', 'C', 0xC0, 0xDE };
		const auto bc_start_ptr = std::search(post_nvuc_start_ptr, post_nvuc_end_ptr, &bc_magic[0], &bc_magic[3] + 1);
		if (bc_start_ptr == post_nvuc_end_ptr) {
			log_error("failed to find NVVM bitcode start in $", identifier_with_suffix);
			return;
		}
		
		// similarly, there is no good way to determine the end of the BC data without parsing/reading it,
		// we do know however:
		//  * BC must be aligned to 4 bytes and end on a 4-byte zero value
		//  * it always ends on "nvsass-nvidia-spirvSPIR-V"
		static constexpr const std::string_view bc_end_marker { "nvsass-nvidia-spirvSPIR-V" };
		auto bc_end_ptr = std::search(bc_start_ptr, post_nvuc_end_ptr, bc_end_marker.begin(), bc_end_marker.end());
		if (bc_end_ptr == post_nvuc_end_ptr) {
			log_error("failed to find NVVM bitcode end in $", identifier_with_suffix);
			return;
		}
		bc_end_ptr += bc_end_marker.size() + 1 /* implicit \0 */;
		assert(bc_start_ptr < bc_end_ptr);
		auto bc_size = std::distance(bc_start_ptr, bc_end_ptr);
		bc_size += (bc_size % 4u != 0u ? (4u - (bc_size % 4u)) : 0u) + 4u; // alignment + zero val
		const std::span<const char> bitcode((const char*)bc_start_ptr, (size_t)bc_size);
		
		if (!file_io::buffer_to_file(identifier_with_suffix + ".bc", bitcode.data(), bitcode.size_bytes())) {
			log_error("failed to dump NVVM bitcode in $", identifier_with_suffix);
			return;
		}
		
		// can just use the llvm-dis from the floor toolchain here
		core::system(floor::get_vulkan_dis() + " -o '" + identifier_with_suffix + ".ll' '" + identifier_with_suffix + ".bc'");
		{
			std::error_code ec {};
			std::filesystem::remove(identifier_with_suffix + ".bc", ec);
		}
		
		// check if there is an ELF file at the end (this is the case when we have debug info)
		static constexpr const std::string_view elf_magic { "\177ELF" };
		auto elf_start_ptr = std::search(bc_end_ptr, post_nvuc_end_ptr, elf_magic.begin(), elf_magic.end());
		if (elf_start_ptr == post_nvuc_end_ptr) {
			// ignore, there is no ELF
			return;
		}
		assert(elf_start_ptr < post_nvuc_end_ptr);
		const auto max_elf_size = size_t(std::distance(elf_start_ptr, post_nvuc_end_ptr));
		if (max_elf_size < sizeof(elf64_header_t)) {
			log_error("found ELF header, but remaing binary size is too small in $", identifier_with_suffix);
			return;
		}
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align)
		const auto& elf_header = *(const elf64_header_t*)elf_start_ptr;
FLOOR_POP_WARNINGS()
		if (elf_header.header_size != sizeof(elf64_header_t)) {
			log_error("invalid ELF header in $", identifier_with_suffix);
			return;
		}
		const auto min_elf_size = (elf_header.program_header_offset > elf_header.section_header_table_offset ?
								   elf_header.program_header_offset +
								   elf_header.program_header_table_entry_count * elf_header.program_header_table_entry_size :
								   elf_header.section_header_table_offset +
								   elf_header.section_header_table_entry_count * elf_header.section_header_table_entry_size);
		if (min_elf_size > max_elf_size) {
			log_error("NVIDIA ELF GPU binary data is smaller than expected @ entry #$ (require at least $' bytes, got $') in $",
					  entry_idx, min_elf_size, max_elf_size, identifier_with_suffix);
			return;
		}
		
		const std::span<const char> elf((const char*)elf_start_ptr, max_elf_size);
		if (!file_io::buffer_to_file(identifier_with_suffix + ".elf", elf.data(), elf.size_bytes())) {
			log_error("failed to dump ELF GPU binary in $", identifier_with_suffix);
			return;
		}
		
		// disassemble, now with debug info
		std::string elf_nvdisasm_output;
		core::system("nvdisasm -c -gi '" + identifier_with_suffix + ".elf'", elf_nvdisasm_output);
		
		// even with -gi/--print-line-info-inline, nvdisasm won't print the actual source code into the file
		// -> fix this manually
		
		// search for: //## File "/abs/file/path.cpp", line 69
		auto disasm_lines = core::tokenize(elf_nvdisasm_output, '\n');
		for (auto line_iter = disasm_lines.begin(); line_iter != disasm_lines.end(); ++line_iter) {
			static const std::regex line_info_rx("//## File \"([^\"]+)\", line ([0-9]+)");
			
			std::smatch line_info_result;
			if (!regex_search(*line_iter, line_info_result, line_info_rx) || line_info_result.size() < 3) {
				continue;
			}
			
			const auto file_name = line_info_result[1];
			const auto line_number = stou(line_info_result[2]);
			const auto mapping = load_and_map_source(file_name);
			if (mapping == nullptr) {
				continue;
			}
			
			auto code_line = get_code_line(*mapping, line_number);
			line_iter = disasm_lines.insert(++line_iter, std::move(code_line));
		}
		
		std::string disasm_output_with_code_lines;
		for (auto line_iter = disasm_lines.begin(); line_iter != disasm_lines.end(); ++line_iter) {
			disasm_output_with_code_lines += *line_iter + '\n';
		}
		
		if (!file_io::string_to_file(identifier_with_suffix + ".elfdis", disasm_output_with_code_lines)) {
			log_error("failed to write disassembled ELF data in $", identifier_with_suffix);
			return;
		}
	}
}

void disassemble(const vulkan_device& dev, const std::string& identifier, const VkPipeline& pipeline,
				 const VkPipelineCache* cache) REQUIRES(!source_file_mappings_lock) {
	// query initial props
	const VkPipelineInfoKHR exec_props_query_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
		.pNext = nullptr,
		.pipeline = pipeline,
	};
	uint32_t exec_props_count = 0;
	vkGetPipelineExecutablePropertiesKHR(dev.device, &exec_props_query_info, &exec_props_count, nullptr);
	if (exec_props_count > 0) {
		std::vector<VkPipelineExecutablePropertiesKHR> exec_props(exec_props_count, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR,
			.pNext = nullptr,
		});
		vkGetPipelineExecutablePropertiesKHR(dev.device, &exec_props_query_info, &exec_props_count, exec_props.data());
		
		std::stringstream pipeline_info;
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
			vkGetPipelineExecutableInternalRepresentationsKHR(dev.device, &exec_info, &ir_count, nullptr);
			pipeline_info << "IR count: " << ir_count << "\n";
			if (ir_count > 0) {
				std::vector<VkPipelineExecutableInternalRepresentationKHR> ir_data(ir_count, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR,
					.pNext = nullptr
				});
				vkGetPipelineExecutableInternalRepresentationsKHR(dev.device, &exec_info, &ir_count, ir_data.data());
				std::vector<std::unique_ptr<uint8_t[]>> ir_data_storage;
				for (auto& ir : ir_data) {
					ir_data_storage.emplace_back(std::make_unique<uint8_t[]>(ir.dataSize));
					ir.pData = ir_data_storage.back().get();
				}
				vkGetPipelineExecutableInternalRepresentationsKHR(dev.device, &exec_info, &ir_count, ir_data.data());
				for (const auto& ir : ir_data) {
					file_io::buffer_to_file(identifier + "_ir_" + ir.name + ".txt", (const char*)ir.pData, ir.dataSize);
				}
			}
			
			// query statistics
			uint32_t stats_count = 0;
			vkGetPipelineExecutableStatisticsKHR(dev.device, &exec_info, &stats_count, nullptr);
			pipeline_info << "stats count: " << stats_count << "\n";
			if (stats_count > 0) {
				std::vector<VkPipelineExecutableStatisticKHR> stats(stats_count, {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR,
					.pNext = nullptr,
				});
				vkGetPipelineExecutableStatisticsKHR(dev.device, &exec_info, &stats_count, stats.data());
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
			
			auto cache_data = std::make_unique<uint32_t[]>((cache_size + 3u) / 4u);
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
			if (dev.vendor == VENDOR::NVIDIA && header.vendorID == 0x10de) {
				disassemble_nvidia(identifier, std::span<const uint8_t> { ((const uint8_t*)cache_data.get()) + header.headerSize, data_size });
				
				// no longer need the bin file in NVIDIAs case
				{
					std::error_code ec {};
					std::filesystem::remove(identifier + ".bin", ec);
				}
			}
			// TODO: AMD: bin contains ELF, ISA, LLVM IR
		} while (false);
	}
}

} // namespace fl::vulkan_disassembly

#endif // FLOOR_NO_VULKAN
