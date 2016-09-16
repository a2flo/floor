/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_SPIRV_HANDLER_HPP__
#define __FLOOR_SPIRV_HANDLER_HPP__

#include <floor/core/essentials.hpp>
#include <floor/compute/llvm_toolchain.hpp>

class spirv_handler {
public:
	//! loads a spir-v binary from the file specified by file_name,
	//! returning the read code + setting code_size to the amount of bytes
	static unique_ptr<uint32_t[]> load_binary(const string& file_name, size_t& code_size);
	
	// #### SPIR-V container file format ####
	// ## header
	// char[4]: identfifier "SPVC"
	// uint32_t: version (currently 1)
	// uint32_t: entry_count
	//
	// ## header entries [entry_count]
	// uint32_t: function_entry_count
	// uint32_t[function_entry_count]: function types
	// char[function_entry_count][]: function names (all \0 terminated)
	// uint32_t: SPIR-V binary word count (word == uint32_t)
	//
	// ## binary entries [entry_count]
	// uint32_t[header_entry[i].word_count]: SPIR-V binary
	
	struct container {
		struct entry {
			vector<llvm_toolchain::function_info::FUNCTION_TYPE> function_types;
			vector<string> function_names;
			uint32_t data_offset;
			uint32_t data_word_count;
		};
		vector<entry> entries;
		unique_ptr<uint32_t[]> spirv_data;
		bool valid { false };
	};
	static constexpr const uint32_t container_version { 1u };
	
	//! loads a SPIR-V container file and processes it into a usable 'container' object
	static container load_container(const string& file_name);
	
protected:
	// static class
	spirv_handler(const spirv_handler&) = delete;
	~spirv_handler() = delete;
	spirv_handler& operator=(const spirv_handler&) = delete;
	
};

#endif
