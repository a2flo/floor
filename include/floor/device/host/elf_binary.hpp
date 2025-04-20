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

#include <floor/device/host/host_common.hpp>

namespace fl {

//! the 64-bit ELF header
struct elf64_header_t {
	char magic[4];
	uint8_t bitness;
	uint8_t endianness;
	uint8_t ident_version;
	uint8_t os_abi;
	uint8_t os_abi_version;
	uint8_t _padding_0[7];
	
	uint16_t type;
	uint16_t machine;
	uint32_t elf_version;
	uint64_t entry_point;
	uint64_t program_header_offset;
	uint64_t section_header_table_offset;
	uint32_t flags;
	uint16_t header_size;
	uint16_t program_header_table_entry_size;
	uint16_t program_header_table_entry_count;
	uint16_t section_header_table_entry_size;
	uint16_t section_header_table_entry_count;
	uint16_t section_names_index;
};
static_assert(sizeof(elf64_header_t) == 64u, "invalid ELF64 header size");

} // namespace fl

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/core/aligned_ptr.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/core/flat_map.hpp>

namespace fl {

struct section_t;
struct relocation_t;
struct symbol_t;

class elf_binary {
public:
	elf_binary(const std::string& file_name);
	elf_binary(const uint8_t* binary_data, const size_t binary_size);
	
	//! returns true if this is a valid ELF binary
	bool is_valid() const {
		return valid;
	}
	
	//! returns all function names inside this binary
	const std::vector<std::string>& get_function_names() const;
	
	//! per execution instance IDs and sizes
	struct instance_ids_t {
		uint3 instance_global_idx;
		uint3 instance_global_work_size;
		uint3 instance_local_idx;
		uint3 instance_local_work_size;
		uint3 instance_group_idx;
		uint3 instance_group_size;
		uint32_t instance_work_dim { 0 };
		uint32_t instance_local_linear_idx { 0 };
	};
	
	//! execution instance
	struct instance_t {
		//! IDs/sizes for this instance
		instance_ids_t ids;
		//! available function name -> function pointer map
		fl::flat_map<std::string, const void*> functions;
		
		//! resets this instance to its initial state (so it can be executed again)
		void reset(const uint3& global_work_size,
				   const uint3& local_work_size,
				   const uint3& group_size,
				   const uint32_t& work_dim);
		
	protected:
		friend elf_binary;
		//! pointer to the allocated r/w / BSS memory for this instance
		uint8_t* rw_memory { nullptr };
		//! size of the r/w / BSS memory in bytes
		size_t rw_memory_size { 0u };
	};
	//! returns the instance for the specified instance index
	instance_t* get_instance(const uint32_t instance_idx);
	
protected:
	std::unique_ptr<uint8_t[]> binary;
	size_t binary_size { 0 };
	bool valid { false };
	
	//! ELF binary info
	//! NOTE: valid as long as "binary" is valid
	struct elf_info_t;
	std::shared_ptr<elf_info_t> info;
	
	//! internal execution instance
	struct internal_instance_t {
		//! public/external execution instance info
		instance_t external_instance;
		//! global offset table
		aligned_ptr<uint64_t> GOT;
		//! number of entries in the global offset table
		uint64_t GOT_entry_count { 1ull };
		//! current global offset table index
		uint64_t GOT_index { 1ull };
		//! (optional) allocated read-only memory for this instance
		//! NOTE: this is only allocated/set when read-only data must be relocated
		aligned_ptr<uint8_t> ro_memory;
		//! allocated r/w / BSS memory for this instance
		aligned_ptr<uint8_t> rw_memory;
		//! allocated executable memory for this instance
		aligned_ptr<uint8_t> exec_memory;
		//! section -> mapped address/pointer
		std::unordered_map<const section_t*, const uint8_t*> section_map;
		
		//! initializes the GOT with the specified amount of entries (+internal entries)
		void init_GOT(const uint64_t& entry_count);
		//! allocate "count" new GOT entries, returns the start index of the allocation in "GOT"
		uint64_t allocate_GOT_entries(const uint64_t& count);
	};
	
	//! internal ELF binary initializer (called from constructor)
	void init_elf();
	
	//! parses the ELF binary
	bool parse_elf();
	
	//! maps the read-only parts of the binary into memory (if it is the same for all instances)
	bool map_global_ro_memory();
	
	//! instantiates the specified instance, returns true on success
	bool instantiate(const uint32_t instance_idx);
	
	//! perform relocations in exec memory and optionally rodata memory
	bool perform_relocations(internal_instance_t& instance,
							 instance_t& ext_instance,
							 const std::vector<relocation_t>& relocations,
							 aligned_ptr<uint8_t>& memory);
	
	//! tries to resolve the symbol specified by "sym", in the specified "instance" + "ext_instance"
	const void* resolve_symbol(internal_instance_t& instance, instance_t& ext_instance, const symbol_t& sym);
	
	//! tries to resolve the section specified by "sym", in the specified "instance"
	const void* resolve_section(internal_instance_t& instance, const symbol_t& sym);
	
	//! tries to resolve the symbol in the specified "relocation", in the specified "instance" + "ext_instance"
	const void* resolve(internal_instance_t& instance, instance_t& ext_instance, const relocation_t& relocation);
	
};

} // namespace fl

#endif
