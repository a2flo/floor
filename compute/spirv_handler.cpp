/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#include <floor/compute/spirv_handler.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/file_io.hpp>

namespace spirv_handler {

unique_ptr<uint32_t[]> load_binary(const string& file_name, size_t& code_size) {
	unique_ptr<uint32_t[]> code;
	
	file_io binary(file_name, file_io::OPEN_TYPE::READ_BINARY);
	if(!binary.is_open()) {
		log_error("failed to load spir-v binary (\"%s\")", file_name);
		return {};
	}
	
	code_size = (size_t)binary.get_filesize();
	if(code_size % 4u != 0u) {
		log_error("invalid spir-v binary size %u (\"%s\"): must be a multiple of 4!", code_size, file_name);
		return {};
	}
	
	code = make_unique<uint32_t[]>(code_size / 4u);
	binary.get_block((char*)code.get(), (streamsize)code_size);
	const auto read_size = binary.get_filestream()->gcount();
	if(read_size != (decltype(read_size))code_size) {
		log_error("failed to read spir-v binary (\"%s\"): expected %u bytes, but only read %u bytes", file_name, code_size, read_size);
		return {};
	}
	
	return code;
}

spirv_handler::container load_container(const string& file_name) {
	string data { "" };
	if(!file_io::file_to_string(file_name, data)) {
		log_error("failed to load spir-v container (\"%s\")", file_name);
		return {};
	}
	return load_container_from_memory((const uint8_t*)data.data(), data.size(), file_name);
}

spirv_handler::container load_container_from_memory(const uint8_t* data_ptr_,
													const size_t& data_size_,
													const string identifier) {
	// reasonable size assumption
	if(data_size_ >= 0x80000000) {
		log_error("container too large");
		return {};
	}
	if(data_size_ < 8) {
		log_error("container too small");
		return {};
	}
	const auto data_size = (uint32_t)data_size_; // we ensured this fits into 32-bit
	
	// check header and version
	auto data_ptr = data_ptr_;
	const auto data_end_ptr = data_ptr + data_size;
	if(memcmp(data_ptr, "SPVC", 4) != 0) {
		log_error("invalid spir-v container header%s", identifier.empty() ? ""s : "(in \"" + identifier + "\")");
		return {};
	}
	data_ptr += 4;
	
	if(memcmp(data_ptr, &container_version, sizeof(container_version)) != 0) {
		log_error("invalid spir-v container version%s", identifier.empty() ? ""s : "(in \"" + identifier + "\")");
		return {};
	}
	data_ptr += sizeof(container_version);
	
	uint32_t entry_count = 0;
	memcpy(&entry_count, data_ptr, sizeof(entry_count));
	data_ptr += sizeof(entry_count);
	
	const auto expected_header_entries_size = entry_count * sizeof(uint32_t) * 2;
	uint32_t cur_size = 8; // header
	if(cur_size + expected_header_entries_size > data_size) {
		log_error("invalid header entries size");
		return {};
	}
	cur_size += expected_header_entries_size;
	
	// header entries
	container ret;
	ret.entries.resize(entry_count);
	uint32_t running_offset = 0;
	for(uint32_t i = 0; i < entry_count; ++i) {
		auto& entry = ret.entries[i];
		
		//
		uint32_t function_entry_count = 0;
		memcpy(&function_entry_count, data_ptr, sizeof(function_entry_count));
		data_ptr += sizeof(function_entry_count);
		entry.function_types.resize(function_entry_count);
		entry.function_names.resize(function_entry_count);
		
		//
		entry.data_word_count = 0;
		memcpy(&entry.data_word_count, data_ptr, sizeof(entry.data_word_count));
		data_ptr += sizeof(entry.data_word_count);
		
		// store a word offset into the big spir-v data chunk for easy use later on
		entry.data_offset = running_offset;
		running_offset += entry.data_word_count;
	}
	
	// get the actual spir-v data in one big chunk
	const auto spirv_data_size = running_offset * sizeof(uint32_t);
	if(cur_size + spirv_data_size > data_size) {
		log_error("invalid spir-v data size");
		return {};
	}
	ret.spirv_data = make_unique<uint32_t[]>(running_offset);
	memcpy(ret.spirv_data.get(), data_ptr, spirv_data_size);
	data_ptr += spirv_data_size;
	cur_size += spirv_data_size;
	
	// get the per-entry/module metadata
	for(auto& entry : ret.entries) {
		const auto function_entry_count = (uint32_t)entry.function_types.size();
		
		//
		const auto func_types_size = function_entry_count * sizeof(llvm_toolchain::FUNCTION_TYPE);
		if(cur_size + func_types_size > data_size) {
			log_error("invalid function types size");
			return {};
		}
		memcpy(entry.function_types.data(), data_ptr, func_types_size);
		data_ptr += func_types_size;
		cur_size += func_types_size;
		
		//
		for(uint32_t i = 0; i < function_entry_count; ++i) {
			if(find(data_ptr, data_end_ptr, '\0') == data_end_ptr) {
				log_error("function name has no terminator");
				return {};
			}
			entry.function_names[i] = (const char*)data_ptr; // string is \0 terminated
			
			auto padded_len = (uint32_t)entry.function_names[i].size();
			padded_len += 4u - (padded_len % 4u);
			if(cur_size + padded_len > data_size) {
				log_error("invalid function name size (not padded?)");
				return {};
			}
			data_ptr += padded_len;
			cur_size += padded_len;
		}
	}
	
	// done
	ret.valid = true;
	return ret;
}

} // spirv_handler
