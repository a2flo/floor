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

#include <floor/core/essentials.hpp>
#include <floor/device/toolchain.hpp>

namespace fl::spirv_handler {
	//! loads a spir-v binary from the file specified by file_name,
	//! returning the read code + setting code_size to the amount of bytes
	std::unique_ptr<uint32_t[]> load_binary(const std::string& file_name, size_t& code_size);
	
	// #### SPIR-V container file format ####
	// ## header
	// char[4]: identifier "SPVC"
	// uint32_t: version (currently 2)
	// uint32_t: entry_count
	//
	// ## header entries [entry_count]
	// uint32_t: function_entry_count
	// uint32_t: SPIR-V module word count (word == uint32_t)
	//
	// ## module entries [entry_count]
	// uint32_t[header_entry[i].word_count]: SPIR-V module
	//
	// ## additional metadata [entry_count]
	// uint32_t[function_entry_count]: function types
	// char[function_entry_count][]: function names (always \0 terminated, with \0 padding to achieve
	//                                               4-byte/uint32_t alignment)
	
	struct container {
		struct entry {
			std::vector<toolchain::FUNCTION_TYPE> function_types;
			std::vector<std::string> function_names;
			uint32_t data_offset;
			uint32_t data_word_count;
		};
		std::vector<entry> entries;
		std::unique_ptr<uint32_t[]> spirv_data;
		bool valid { false };
	};
	static constexpr const uint32_t container_version { 2u };
	
	//! loads a SPIR-V container file and processes it into a usable 'container' object
	container load_container(const std::string& file_name);
	container load_container_from_memory(const uint8_t* data_ptr,
										 const size_t& data_size,
										 const std::string identifier = "");

} // fl::spirv_handler
