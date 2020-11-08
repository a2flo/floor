/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/host/elf_binary.hpp>
#include <floor/core/enum_helpers.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/core.hpp>
#include <string_view>

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#include <sys/mman.h>
#else
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup
#endif

#if defined(__APPLE__)
#include <mach/vm_map.h>
#include <mach/vm_prot.h>
#include <mach/mach_init.h>
#endif

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

enum class ELF_SECTION_TYPE : uint32_t {
	UNUSED = 0,
	PROGRAM_DATA = 1,
	SYMBOL_TABLE = 2,
	STRING_TABLE = 3,
	RELOCATION_ENTRIES_ADDEND = 4,
	SYMBOL_HASH_TABLE = 5,
	DYNAMIC_LINKING_INFO = 6,
	NOTES = 7,
	BSS = 8,
	RELOCATION_ENTRIES = 9,
	_RESERVER = 10,
	DYNAMIC_SYMBOL_TABLE = 11,
	// 12
	// 13
	CONSTRUCTOR_ARRAY = 14,
	DESTRUCTOR_ARRAY = 15,
	PRECONSTRUCTOR_ARRAY = 16,
	SECTION_GROUP = 17,
	EXTENDED_SECTION_INDICES = 18,
	DEFINED_TYPES_COUNT = 19,
	
	OS_START = 0x60000000u,
	OS_END = 0x6FFFFFFFu,
	PROCESSOR_START = 0x70000000u,
	PROCESSOR_END = 0x7FFFFFFFu,
	USER_START = 0x80000000u,
	USER_END = 0xFFFFFFFFu,
};

enum class ELF_SECTION_FLAG : uint64_t {
	NONE = (0ull),
	WRITE = (1ull << 0ull),
	ALLOCATE = (1ull << 1ull),
	EXECUTABLE = (1ull << 2ull),
	// (1 << 3) unused
	MERGABLE = (1ull << 4ull),
	STRINGS = (1ull << 5ull),
	INFO_LINK = (1ull << 6ull),
	LINK_ORDER = (1ull << 7ull),
	NON_CONFORMING = (1ull << 8ull),
	GROUP = (1ull << 9ull),
	TLS = (1ull << 10ull),
	COMPRESSED = (1ull << 11ull),
	
	MASK_OS = 0x0FF0'0000ull,
	MASK_PROC = 0xF000'0000ull,
	SPECIAL_ORDER = 0x0400'0000ull,
	EXCLUDE = 0x0800'0000ull,
};
floor_global_enum_ext(ELF_SECTION_FLAG)

struct elf64_section_header_entry_t {
	uint32_t name_offset;
	ELF_SECTION_TYPE type;
	ELF_SECTION_FLAG flags;
	uint64_t address;
	uint64_t offset;
	uint64_t size;
	uint32_t link_index;
	uint32_t extra_info;
	uint64_t alignment;
	uint64_t entry_size;
};
static_assert(sizeof(elf64_section_header_entry_t) == 64u, "invalid ELF64 section header entry size");

struct section_t {
	const elf64_section_header_entry_t* header_ptr { nullptr };
	string name;
	
	void dump(ostream& sstr) const {
		sstr << "section: " << name;
		sstr << ", offset: " << header_ptr->offset;
		sstr << ", size: " << header_ptr->size;
		sstr << ", address: " << header_ptr->address;
		if (header_ptr->alignment > 0) {
			sstr << ", align: " << header_ptr->alignment;
		}
		sstr << ", 0x" << hex << uppercase << (uint64_t)header_ptr->flags << nouppercase << dec;
		if (has_flag<ELF_SECTION_FLAG::ALLOCATE>(header_ptr->flags)) {
			sstr << " alloc";
		}
		if (has_flag<ELF_SECTION_FLAG::WRITE>(header_ptr->flags)) {
			sstr << " w";
		}
		if (has_flag<ELF_SECTION_FLAG::EXECUTABLE>(header_ptr->flags)) {
			sstr << " x";
		}
		if (has_flag<ELF_SECTION_FLAG::MERGABLE>(header_ptr->flags)) {
			sstr << " mrg";
		}
		if (has_flag<ELF_SECTION_FLAG::GROUP>(header_ptr->flags)) {
			sstr << " grp";
		}
		if (has_flag<ELF_SECTION_FLAG::TLS>(header_ptr->flags)) {
			sstr << " tls";
		}
		sstr << "\n";
	}
};

enum class ELF_SYMBOL_TYPE : uint8_t {
	NONE = 0,
	DATA = 1,
	CODE = 2,
	SECTION = 3,
	FILE_NAME = 4,
	COMMON = 5,
	TLS = 6,
	INDIRECT_CODE = 10,
	__MAX_ELF_SYMBOL_TYPE = 15,
};

enum class ELF_SYMBOL_BINDING : uint8_t {
	LOCAL = 0,
	GLOBAL = 1,
	WEAK = 2,
	GNU_UNIQUE = 10,
	__MAX_ELF_SYMBOL_BINDING = 15,
};

enum class ELF_SYMBOL_VISIBILITY : uint8_t {
	DEFAULT = 0,
	INTERNAL = 1,
	HIDDEN = 2,
	PROTECTED = 3,
};

struct elf64_symbol_t {
	uint32_t name_offset;
	struct {
		ELF_SYMBOL_TYPE type : 4;
		ELF_SYMBOL_BINDING binding : 4;
	};
	ELF_SYMBOL_VISIBILITY visibility;
	uint16_t section_header_table_index;
	uint64_t value;
	uint64_t size;
};
static_assert(sizeof(elf64_symbol_t) == 24u, "invalid ELF64 symbol size");

struct symbol_t {
	const elf64_symbol_t* symbol_ptr { nullptr };
	string name;
	
	void dump(ostream& sstr, const vector<section_t>& sections) const {
		sstr << "symbol " << name << ": ";
		switch (symbol_ptr->binding) {
			case ELF_SYMBOL_BINDING::LOCAL: sstr << "local"; break;
			case ELF_SYMBOL_BINDING::GLOBAL: sstr << "global"; break;
			case ELF_SYMBOL_BINDING::WEAK: sstr << "weak"; break;
			case ELF_SYMBOL_BINDING::GNU_UNIQUE: sstr << "gnu-unique"; break;
			default: sstr << "<unknown-binding>"; break;
		}
		sstr << " ";
		switch (symbol_ptr->type) {
			case ELF_SYMBOL_TYPE::NONE: sstr << "none"; break;
			case ELF_SYMBOL_TYPE::DATA: sstr << "data"; break;
			case ELF_SYMBOL_TYPE::CODE: sstr << "code"; break;
			case ELF_SYMBOL_TYPE::SECTION: sstr << "section"; break;
			case ELF_SYMBOL_TYPE::FILE_NAME: sstr << "file-name"; break;
			case ELF_SYMBOL_TYPE::COMMON: sstr << "common"; break;
			case ELF_SYMBOL_TYPE::TLS: sstr << "tls"; break;
			case ELF_SYMBOL_TYPE::INDIRECT_CODE: sstr << "indirect-code"; break;
			default: sstr << "<unknown-type>"; break;
		}
		sstr << " ";
		switch (symbol_ptr->visibility) {
			case ELF_SYMBOL_VISIBILITY::DEFAULT: sstr << "(default)"; break;
			case ELF_SYMBOL_VISIBILITY::INTERNAL: sstr << "(internal)"; break;
			case ELF_SYMBOL_VISIBILITY::HIDDEN: sstr << "(hidden)"; break;
			case ELF_SYMBOL_VISIBILITY::PROTECTED: sstr << "(protected)"; break;
		}
		sstr << ", value/offset: " << to_string(symbol_ptr->value);
		sstr << ", size: " << to_string(symbol_ptr->size);
		if (symbol_ptr->section_header_table_index < uint16_t(sections.size())) {
			sstr << ", section: ";
			if (symbol_ptr->section_header_table_index == 0) {
				sstr << "<external>";
			} else {
				const auto& section = sections[symbol_ptr->section_header_table_index];
				sstr << section.name;
			}
		}
		sstr << "\n";
	}
};

//! relocation types specified by the SysV x86-64/AMD64 ABI
//! NOTE: relocation types that are not emitted by LLVM are marked as unused
//! NOTE: currently used types (by nbody - large): R_X86_64_GOTPC64, R_X86_64_GOT64, R_X86_64_GOTOFF64
enum class ELF_RELOCATION_TYPE_X86_64 : uint32_t {
	NONE = 0,
	DIRECT_64 = 1,
	PC32 = 2,
	GOT32 = 3,
	PLT32 = 4,
	COPY [[deprecated("unused")]] = 5,
	GLOB_DAT [[deprecated("unused")]] = 6,
	JUMP_SLOT [[deprecated("unused")]] = 7,
	RELATIVE [[deprecated("unused")]] = 8,
	GOTPCREL = 9,
	DIRECT_ZEXT_32 = 10,
	DIRECT_SEXT_32 = 11,
	DIRECT_ZEXT_16 = 12,
	PC16 = 13,
	DIRECT_ZEXT_8 = 14,
	PC8 = 15,
	DTPMOD64 [[deprecated("unused")]] = 16,
	DTPOFF64 = 17,
	TPOFF64 = 18,
	TLSGD = 19,
	TLSLD = 20,
	DTPOFF32 = 21,
	GOTTPOFF = 22,
	TPOFF32 = 23,
	PC64 = 24,
	GOTOFF64 = 25,
	GOTPC32 = 26,
	GOT64 = 27,
	GOTPCREL64 [[deprecated("unused")]] = 28,
	GOTPC64 = 29,
	GOTPLT64 [[deprecated("unused")]] = 30,
	PLTOFF64 [[deprecated("unused")]] = 31,
	SIZE32 = 32,
	SIZE64 = 33,
	GOTPC32_TLSDESC = 34,
	TLSDESC_CALL = 35,
	TLSDESC [[deprecated("unused")]] = 36,
	IRELATIVE [[deprecated("unused")]] = 37,
	// RELATIVE64 = 38 -- 32-bit only
	// deprecated = 39
	// deprecated = 40
	GOTPCRELX = 41,
	REX_GOTPCRELX = 42,
	__MAX_ELF_RELOCATION_TYPE_X86_64
};

//! relocation types specified by the SysV ARM64/AArch64 ABI
enum class ELF_RELOCATION_TYPE_ARM64 : uint32_t {
	NONE = 0,
	__MIN_ELF_RELOCATION_TYPE_ARM64 = 0x101,
	ABS64 = 0x101,
	ABS32 = 0x102,
	ABS16 = 0x103,
	PREL64 = 0x104,
	PREL32 = 0x105,
	PREL16 = 0x106,
	MOVW_UABS_G0 = 0x107,
	MOVW_UABS_G0_NC = 0x108,
	MOVW_UABS_G1 = 0x109,
	MOVW_UABS_G1_NC = 0x10A,
	MOVW_UABS_G2 = 0x10B,
	MOVW_UABS_G2_NC = 0x10C,
	MOVW_UABS_G3 = 0x10D,
	MOVW_SABS_G0 = 0x10E,
	MOVW_SABS_G1 = 0x10F,
	MOVW_SABS_G2 = 0x110,
	LD_PREL_LO19 = 0x111,
	ADR_PREL_LO21 = 0x112,
	ADR_PREL_PG_HI21 = 0x113,
	ADR_PREL_PG_HI21_NC = 0x114,
	ADD_ABS_LO12_NC = 0x115,
	LDST8_ABS_LO12_NC = 0x116,
	TSTBR14 = 0x117,
	CONDBR19 = 0x118,
	JUMP26 = 0x11A,
	CALL26 = 0x11B,
	LDST16_ABS_LO12_NC = 0x11C,
	LDST32_ABS_LO12_NC = 0x11D,
	LDST64_ABS_LO12_NC = 0x11E,
	MOVW_PREL_G0 = 0x11F,
	MOVW_PREL_G0_NC = 0x120,
	MOVW_PREL_G1 = 0x121,
	MOVW_PREL_G1_NC = 0x122,
	MOVW_PREL_G2 = 0x123,
	MOVW_PREL_G2_NC = 0x124,
	MOVW_PREL_G3 = 0x125,
	LDST128_ABS_LO12_NC = 0x12B,
	MOVW_GOTOFF_G0 = 0x12C,
	MOVW_GOTOFF_G0_NC = 0x12D,
	MOVW_GOTOFF_G1 = 0x12E,
	MOVW_GOTOFF_G1_NC = 0x12F,
	MOVW_GOTOFF_G2 = 0x130,
	MOVW_GOTOFF_G2_NC = 0x131,
	MOVW_GOTOFF_G3 = 0x132,
	GOTREL64 = 0x133,
	GOTREL32 = 0x134,
	GOT_LD_PREL19 = 0x135,
	LD64_GOTOFF_LO15 = 0x136,
	ADR_GOT_PAGE = 0x137,
	LD64_GOT_LO12_NC = 0x138,
	LD64_GOTPAGE_LO15 = 0x139,
	TLSGD_ADR_PREL21 = 0x200,
	TLSGD_ADR_PAGE21 = 0x201,
	TLSGD_ADD_LO12_NC = 0x202,
	TLSGD_MOVW_G1 = 0x203,
	TLSGD_MOVW_G0_NC = 0x204,
	TLSLD_ADR_PREL21 = 0x205,
	TLSLD_ADR_PAGE21 = 0x206,
	TLSLD_ADD_LO12_NC = 0x207,
	TLSLD_MOVW_G1 = 0x208,
	TLSLD_MOVW_G0_NC = 0x209,
	TLSLD_LD_PREL19 = 0x20A,
	TLSLD_MOVW_DTPREL_G2 = 0x20B,
	TLSLD_MOVW_DTPREL_G1 = 0x20C,
	TLSLD_MOVW_DTPREL_G1_NC = 0x20D,
	TLSLD_MOVW_DTPREL_G0 = 0x20E,
	TLSLD_MOVW_DTPREL_G0_NC = 0x20F,
	TLSLD_ADD_DTPREL_HI12 = 0x210,
	TLSLD_ADD_DTPREL_LO12 = 0x211,
	TLSLD_ADD_DTPREL_LO12_NC = 0x212,
	TLSLD_LDST8_DTPREL_LO12 = 0x213,
	TLSLD_LDST8_DTPREL_LO12_NC = 0x214,
	TLSLD_LDST16_DTPREL_LO12 = 0x215,
	TLSLD_LDST16_DTPREL_LO12_NC = 0x216,
	TLSLD_LDST32_DTPREL_LO12 = 0x217,
	TLSLD_LDST32_DTPREL_LO12_NC = 0x218,
	TLSLD_LDST64_DTPREL_LO12 = 0x219,
	TLSLD_LDST64_DTPREL_LO12_NC = 0x21A,
	TLSIE_MOVW_GOTTPREL_G1 = 0x21B,
	TLSIE_MOVW_GOTTPREL_G0_NC = 0x21C,
	TLSIE_ADR_GOTTPREL_PAGE21 = 0x21D,
	TLSIE_LD64_GOTTPREL_LO12_NC = 0x21E,
	TLSIE_LD_GOTTPREL_PREL19 = 0x21F,
	TLSLE_MOVW_TPREL_G2 = 0x220,
	TLSLE_MOVW_TPREL_G1 = 0x221,
	TLSLE_MOVW_TPREL_G1_NC = 0x222,
	TLSLE_MOVW_TPREL_G0 = 0x223,
	TLSLE_MOVW_TPREL_G0_NC = 0x224,
	TLSLE_ADD_TPREL_HI12 = 0x225,
	TLSLE_ADD_TPREL_LO12 = 0x226,
	TLSLE_ADD_TPREL_LO12_NC = 0x227,
	TLSLE_LDST8_TPREL_LO12 = 0x228,
	TLSLE_LDST8_TPREL_LO12_NC = 0x229,
	TLSLE_LDST16_TPREL_LO12 = 0x22A,
	TLSLE_LDST16_TPREL_LO12_NC = 0x22B,
	TLSLE_LDST32_TPREL_LO12 = 0x22C,
	TLSLE_LDST32_TPREL_LO12_NC = 0x22D,
	TLSLE_LDST64_TPREL_LO12 = 0x22E,
	TLSLE_LDST64_TPREL_LO12_NC = 0x22F,
	TLSDESC_LD_PREL19 = 0x230,
	TLSDESC_ADR_PREL21 = 0x231,
	TLSDESC_ADR_PAGE21 = 0x232,
	TLSDESC_LD64_LO12 = 0x233,
	TLSDESC_ADD_LO12 = 0x234,
	TLSDESC_OFF_G1 = 0x235,
	TLSDESC_OFF_G0_NC = 0x236,
	TLSDESC_LDR = 0x237,
	TLSDESC_ADD = 0x238,
	TLSDESC_CALL = 0x239,
	TLSLE_LDST128_TPREL_LO12 = 0x23A,
	TLSLE_LDST128_TPREL_LO12_NC = 0x23B,
	TLSLD_LDST128_DTPREL_LO12 = 0x23C,
	TLSLD_LDST128_DTPREL_LO12_NC = 0x23D,
	COPY = 0x400,
	GLOB_DAT = 0x401,
	JUMP_SLOT = 0x402,
	RELATIVE = 0x403,
	TLS_DTPREL64 = 0x404,
	TLS_DTPMOD64 = 0x405,
	TLS_TPREL64 = 0x406,
	TLSDESC = 0x407,
	IRELATIVE = 0x408,
	__MAX_ELF_RELOCATION_TYPE_ARM64
};

struct elf64_relocation_addend_entry_t {
	uint64_t offset;
	struct {
		union {
			ELF_RELOCATION_TYPE_X86_64 type_x86_64;
			ELF_RELOCATION_TYPE_ARM64 type_arm64;
		};
		uint32_t symbol_index;
	};
	int64_t addend;
};
static_assert(sizeof(elf64_relocation_addend_entry_t) == 24u, "invalid ELF64 relocation addend entry size");

struct relocation_t {
	const elf64_relocation_addend_entry_t* reloc_ptr { nullptr };
	const symbol_t* symbol_ptr { nullptr };
	
	void dump(ostream& sstr, const vector<section_t>& sections, const vector<symbol_t>& symbols) const {
		sstr << "reloc: symbol ";
		if (reloc_ptr->symbol_index < symbols.size()) {
			sstr << symbols[reloc_ptr->symbol_index].name;
			if (reloc_ptr->symbol_index == 0) {
				sstr << "(NULL)";
			}
		} else {
			sstr << "<invalid-symbol-idx>";
		}
		sstr << ", type: " << (underlying_type_t<ELF_RELOCATION_TYPE_X86_64>)reloc_ptr->type_x86_64;
		sstr << ", add: " << reloc_ptr->addend;
		bool found_section = false;
		for (const auto& section : sections) {
			if (reloc_ptr->offset >= section.header_ptr->offset &&
				reloc_ptr->offset < (section.header_ptr->offset + section.header_ptr->size)) {
				sstr << ", section: " << section.name;
				found_section = true;
				break;
			}
		}
		if (!found_section) {
			sstr << ", section: <unknown>";
		}
		sstr << ", offset: " << reloc_ptr->offset;
		sstr << "\n";
	}
};

struct elf_binary::elf_info_t {
	const elf64_header_t& header;
	const elf64_section_header_entry_t* section_headers { nullptr };
	vector<const elf64_section_header_entry_t*> section_header_entries;
	vector<section_t> sections;
	vector<symbol_t> symbols;
	vector<relocation_t> exec_relocations;
	vector<relocation_t> rodata_relocations;
	//! NOTE: this is only allocated/set when read-only data must _not_ be relocated (is global for all instances)
	aligned_ptr<uint8_t> ro_memory;
	bool relocate_rodata { false };
	vector<string> function_names;
	bool parsed_successfully { false };
	//! rodata section -> mapped address/pointer
	//! NOTE: this only exists when read-only data is global (is not relocated)
	unordered_map<const section_t*, const uint8_t*> ro_section_map;
	
	//! contains all "internal" execution instances for this binary
	vector<internal_instance_t> instances;
	
	bool is_valid() const {
		if (section_headers == nullptr || section_header_entries.empty() || sections.empty() || symbols.empty()) {
			return false;
		}
		return parsed_successfully;
	}
};

void elf_binary::instance_t::reset(const uint3& global_work_size,
								   const uint3& local_work_size,
								   const uint3& group_size,
								   const uint32_t& work_dim) {
	ids.instance_global_idx = {};
	ids.instance_global_work_size = global_work_size;
	ids.instance_local_idx = {};
	ids.instance_local_work_size = local_work_size;
	ids.instance_group_idx = {};
	ids.instance_group_size = group_size;
	ids.instance_work_dim = work_dim;
	ids.instance_local_linear_idx = 0;
	
	// reset r/w memory (aka BSS, aka local memory)
	if (rw_memory && rw_memory_size > 0) {
		memset(rw_memory, 0, rw_memory_size);
	}
}

void elf_binary::internal_instance_t::init_GOT(const uint64_t& entry_count) {
	GOT_entry_count = 1ull + entry_count;
	GOT.resize(GOT_entry_count, 0);
	// first address/entry always points to the GOT itself
	GOT[0] = (uint64_t)&GOT[0];
}

uint64_t elf_binary::internal_instance_t::allocate_GOT_entries(const uint64_t& count) {
	if (count + GOT_index > GOT_entry_count) {
		log_error("allocation of $ GOT entries would create more GOT entries than previously defined", count);
		return ~0ull;
	}
	
	const auto ret_start_idx = GOT_index;
	GOT_index += count;
	return ret_start_idx;
}

elf_binary::elf_binary(const uint8_t* binary_data, const size_t binary_size_) : binary_size(binary_size_) {
	if (binary_size == 0) {
		return;
	}
	binary = make_unique<uint8_t[]>(binary_size);
	memcpy(binary.get(), binary_data, binary_size);
	
	init_elf();
}

elf_binary::elf_binary(const string& file_name) {
	auto [bin, bin_size] = file_io::file_to_buffer(file_name);
	if (!bin || bin_size == 0) {
		return;
	}
	binary = move(bin);
	binary_size = bin_size;
	
	init_elf();
}

void elf_binary::init_elf() {
	// parse all ELF things
	if (!parse_elf()) {
		return;
	}
	
	// map global r/o memory
	if (!map_global_ro_memory()) {
		return;
	}
	
	// create an instance for each CPU
	const auto cpu_count = core::get_hw_thread_count();
	if (cpu_count == 0) {
		return;
	}
	info->instances.resize(cpu_count);
	// TODO: multi-threaded init
	for (uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		if (!instantiate(cpu_idx)) {
			log_error("ELF binary instantiation for instance index %u failed", cpu_idx);
			return;
		}
	}
	
	valid = true;
}

const vector<string>& elf_binary::get_function_names() const {
	if (!info || !valid) {
		static const vector<string> empty {};
		return empty;
	}
	return info->function_names;
}

elf_binary::instance_t* elf_binary::get_instance(const uint32_t instance_idx) {
	if (!info || !valid || instance_idx >= info->instances.size()) {
		return nullptr;
	}
	return &info->instances[instance_idx].external_instance;
}

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align)

bool elf_binary::parse_elf() {
#if !defined(FLOOR_NO_EXCEPTIONS)
	try
#endif
	{
		if (binary_size < sizeof(elf64_header_t)) {
			log_error("invalid binary size");
			return false;
		}
		
		
		// get + check the header
		const auto& header = *(const elf64_header_t*)binary.get();
		if (memcmp(header.magic, "\177ELF", 4u) != 0) {
			log_error("invalid ELF magic");
			return false;
		}
		if (header.bitness != 2) {
			log_error("only 64-bit ELF is supported");
			return false;
		}
		if (header.endianness != 1) {
			log_error("only little endian is supported");
			return false;
		}
		if (header.ident_version != 1) {
			log_error("ELF ident version must be 1");
			return false;
		}
		// don't care about os_abi/os_abi_version
		if (header.type != 1) {
			log_error("ELF object type must be REL/relocatable");
			return false;
		}
		if (header.machine != 0x3E) {
			// TODO: ARM (0xB7)
			log_error("only AMD64 is supported right now");
			return false;
		}
		if (header.elf_version != 1) {
			log_error("ELF version must be 1");
			return false;
		}
		if (header.header_size != sizeof(elf64_header_t)) {
			log_error("invalid ELF header size");
			return false;
		}
		if (header.section_header_table_entry_size != sizeof(elf64_section_header_entry_t)) {
			log_error("invalid section header table entry size");
			return false;
		}
		
		// TODO: ensure unused header fields are 0 / actually unused / not required
		
		// create the info object
		info = make_shared<elf_info_t>(elf_info_t {
			.header = header,
		});
		
		// check program header offsets + sizes
		// NOTE: we might have no entries at all -> can ignore offsets and counts
		optional<size2> program_header_range;
		if (header.program_header_table_entry_count > 0) {
			if (header.program_header_offset < sizeof(elf64_header_t) ||
				header.program_header_offset >= binary_size) {
				log_error("invalid program header offset");
				return false;
			}
			
			const auto program_headers_size = size_t(header.program_header_table_entry_count) * size_t(header.program_header_table_entry_size);
			if (header.program_header_offset + program_headers_size > binary_size) {
				log_error("program headers are out-of-bounds");
				return false;
			}
			program_header_range = { header.program_header_offset, header.program_header_offset + program_headers_size};
		}
		// TODO: actually reject anything that has a program header? we don't use/need one?
		
		// check section header offsets + sizes
		// NOTE: this may not be zero
		if (header.section_header_table_entry_count == 0) {
			log_error("section header count is 0");
			return false;
		}
		if (header.section_header_table_offset < sizeof(elf64_header_t) ||
			header.section_header_table_offset >= binary_size) {
			log_error("invalid section header offset");
			return false;
		}
		const auto section_headers_size = size_t(header.section_header_table_entry_count) * size_t(header.section_header_table_entry_size);
		if (header.section_header_table_offset + section_headers_size > binary_size) {
			log_error("section headers are out-of-bounds");
			return false;
		}
		if (header.section_names_index >= header.section_header_table_entry_count) {
			log_error("section names index is out-of-bounds");
			return false;
		}
		
		// check that program headers and section headers are disjunct
		if (program_header_range) {
			if (header.section_header_table_offset == program_header_range->x) {
				log_error("section and program header overlap");
				return false;
			}
			if (header.section_header_table_offset < program_header_range->x) {
				if ((header.section_header_table_offset + section_headers_size) > program_header_range->x) {
					log_error("section and program header overlap");
					return false;
				}
			} else {
				// header.section_header_table_offset > program_header_range->x
				if (program_header_range->y > header.section_header_table_offset) {
					log_error("section and program header overlap");
					return false;
				}
			}
		}
		
		// handle section header entries
		info->section_headers = (const elf64_section_header_entry_t*)&binary[header.section_header_table_offset];
		info->section_header_entries.resize(header.section_header_table_entry_count);
		for (uint32_t i = 0; i < header.section_header_table_entry_count; ++i) {
			info->section_header_entries[i] = (const elf64_section_header_entry_t*)(binary.get() + header.section_header_table_offset +
																					i * sizeof(elf64_section_header_entry_t));
		}
		
		// handle sections and symbols
		const auto& section_name_header = info->section_headers[header.section_names_index];
		if (section_name_header.type != ELF_SECTION_TYPE::STRING_TABLE) {
			log_error("invalid section name header");
			return false;
		}
		if (section_name_header.name_offset + section_name_header.size > binary_size) {
			log_error("section names out-of-bounds");
			return false;
		}
		info->sections.resize(header.section_header_table_entry_count);
		const string_view names { (const char*)&binary[section_name_header.offset], section_name_header.size };
		for (uint32_t i = 0; i < header.section_header_table_entry_count; ++i) {
			auto& section = info->sections[i];
			section.header_ptr = &info->section_headers[i];
			
			const auto names_start = names.substr(section.header_ptr->name_offset);
			const auto term_pos = names_start.find('\0');
			if (term_pos == string::npos) {
				log_error("didn't find section name terminator");
				return false;
			}
			section.name = string(names_start.substr(0, term_pos));
			
			// if we found the symbol table, also extract all symbols and their names
			if (section.header_ptr->type == ELF_SECTION_TYPE::SYMBOL_TABLE) {
				if (section.header_ptr->entry_size != sizeof(elf64_symbol_t)) {
					log_error("invalid symbol table entry size");
					return false;
				}
				if ((section.header_ptr->size % sizeof(elf64_symbol_t)) != 0u) {
					log_error("incomplete symbol table");
					return false;
				}
				if (section.header_ptr->offset + section.header_ptr->size > binary_size) {
					log_error("symbol table is out-of-bounds");
					return false;
				}
				const auto symbols_start = (const elf64_symbol_t*)&binary[section.header_ptr->offset];
				for (uint64_t sym_idx = 0, sym_count = section.header_ptr->size / sizeof(elf64_symbol_t); sym_idx < sym_count; ++sym_idx) {
					symbol_t sym {
						.symbol_ptr = &symbols_start[sym_idx]
					};
					
					const auto sym_names_start = names.substr(sym.symbol_ptr->name_offset);
					const auto sym_term_pos = sym_names_start.find('\0');
					if (sym_term_pos == string::npos) {
						log_error("didn't find symbol name terminator");
						return false;
					}
					sym.name = string(sym_names_start.substr(0, sym_term_pos));
					info->symbols.emplace_back(move(sym));
				}
			}
		}
		
		// read relocations
		// NOTE: can only do this after all symbols have been read
		for (const auto& section : info->sections) {
			// if we found the relocation entries, extract them all
			if (section.header_ptr->type == ELF_SECTION_TYPE::RELOCATION_ENTRIES_ADDEND) {
				if (section.header_ptr->entry_size != sizeof(elf64_relocation_addend_entry_t)) {
					log_error("invalid relocation addend entry size");
					return false;
				}
				if ((section.header_ptr->size % sizeof(elf64_relocation_addend_entry_t)) != 0u) {
					log_error("incomplete relocation addend table");
					return false;
				}
				if (section.header_ptr->offset + section.header_ptr->size > binary_size) {
					log_error("relocation addend table is out-of-bounds");
					return false;
				}
				
				// we only support relocations in the .text/exec and .rodata/read-only section
				vector<relocation_t>* relocations = nullptr;
				if (section.name == ".rela.text") {
					relocations = &info->exec_relocations;
				} else if (section.name == ".rela.rodata") {
					relocations = &info->rodata_relocations;
					// signal that we need to relocate read-only data (-> need rodata per instance)
					info->relocate_rodata = true;
				} else {
					log_error("relocations section %s is not supported", section.name);
					return false;
				}
				
				const auto relocs_start = (const elf64_relocation_addend_entry_t*)&binary[section.header_ptr->offset];
				for (uint64_t rel_idx = 0, rel_count = section.header_ptr->size / sizeof(elf64_relocation_addend_entry_t); rel_idx < rel_count; ++rel_idx) {
					relocation_t reloc {
						.reloc_ptr = &relocs_start[rel_idx],
					};
					
					if (reloc.reloc_ptr->symbol_index >= info->symbols.size()) {
						log_error("relocation symbol index is out-of-bounds");
						return false;
					}
					reloc.symbol_ptr = &info->symbols[reloc.reloc_ptr->symbol_index];
					
					relocations->emplace_back(move(reloc));
				}
			} else if (section.header_ptr->type == ELF_SECTION_TYPE::RELOCATION_ENTRIES) {
				log_error("relocations without addend are not supported by the ABI");
				return false;
			}
		}
		
		// validate that we only have sections that we want and can handle
		for (size_t i = 0, count = info->sections.size(); i < count; ++i) {
			const auto& section = info->sections[i];
			const auto& sec_header = *section.header_ptr;
			
			if (has_flag<ELF_SECTION_FLAG::COMPRESSED>(sec_header.flags)) {
				log_error("compressed sections are not supported");
				return false;
			}
			if (has_flag<ELF_SECTION_FLAG::WRITE>(sec_header.flags) &&
				has_flag<ELF_SECTION_FLAG::EXECUTABLE>(sec_header.flags)) {
				log_error("a section may not be both writable and executable");
				return false;
			}
			
			if (sec_header.type == ELF_SECTION_TYPE::UNUSED) {
				if (sec_header.size > 0) {
					log_error("'unused' section must have a size of 0");
					return false;
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::STRING_TABLE) {
				if (section.name != ".strtab") {
					log_error("invalid string table section name");
					return false;
				}
				if (i != header.section_names_index) {
					log_error("invalid duplicate string table section");
					return false;
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::PROGRAM_DATA) {
				const auto is_rodata = (section.name.find(".rodata") == 0);
				const auto is_exec = (section.name.find(".text") == 0);
				if (!is_rodata && !is_exec) {
					log_error("invalid program data section name");
					return false;
				}
				if (has_flag<ELF_SECTION_FLAG::WRITE>(sec_header.flags)) {
					log_error("program data section must be non-writable");
					return false;
				}
				if (is_exec) {
					if (!has_flag<ELF_SECTION_FLAG::EXECUTABLE>(sec_header.flags)) {
						log_error("program data section must be executable");
						return false;
					}
				} else if (is_rodata) {
					if (has_flag<ELF_SECTION_FLAG::EXECUTABLE>(sec_header.flags)) {
						log_error("read-only program data section must be non-executable");
						return false;
					}
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::RELOCATION_ENTRIES_ADDEND) {
				if (section.name.find(".rela") != 0) {
					log_error("invalid relocation entries section name");
					return false;
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::SECTION_GROUP) {
				if (section.name.find(".group") != 0) {
					log_error("invalid group section name");
					return false;
				}
				if (sec_header.flags != ELF_SECTION_FLAG::NONE) {
					log_error("invalid group section flags");
					return false;
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::BSS) {
				if (section.name.find(".bss") != 0) {
					log_error("invalid BSS section name");
					return false;
				}
				if (has_flag<ELF_SECTION_FLAG::EXECUTABLE>(sec_header.flags)) {
					log_error("BSS section must be non-executable");
					return false;
				}
			} else if (sec_header.type == ELF_SECTION_TYPE::SYMBOL_TABLE) {
				if (section.name.find(".symtab") != 0) {
					log_error("invalid symbol table section name");
					return false;
				}
				if (sec_header.flags != ELF_SECTION_FLAG::NONE) {
					log_error("invalid symbol table section flags");
					return false;
				}
			} else {
				log_error("invalid or unhandled section: %s (type %X)", section.name, sec_header.type);
				return false;
			}
		}
		
		// get all function names
		for (const auto& sym : info->symbols) {
			if (sym.name.empty() || !(sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::GLOBAL && sym.symbol_ptr->type == ELF_SYMBOL_TYPE::CODE)) {
				continue;
			}
			if (!has_flag<ELF_SECTION_FLAG::EXECUTABLE>(info->sections[sym.symbol_ptr->section_header_table_index].header_ptr->flags)) {
				continue;
			}
			info->function_names.emplace_back(sym.name);
		}
		
		info->parsed_successfully = true;
	}
#if !defined(FLOOR_NO_EXCEPTIONS)
	catch (exception& exc) {
		log_error("error during ELF parsing: %s", exc.what());
		return false;
	}
#endif
	
	return true;
}

static bool map_ro_memory(aligned_ptr<uint8_t>& ro_memory,
						  const vector<section_t>& sections,
						  const uint8_t* binary,
						  unordered_map<const section_t*, const uint8_t*>& section_map) {
	// find all read-only sections that need to be allocated:
	// section -> offset
	vector<pair<const section_t*, uint64_t>> ro_sections;
	for (const auto& section : sections) {
		if (!has_flag<ELF_SECTION_FLAG::ALLOCATE>(section.header_ptr->flags)) {
			continue;
		}
		
		const auto is_writable = has_flag<ELF_SECTION_FLAG::WRITE>(section.header_ptr->flags);
		const auto is_exec = has_flag<ELF_SECTION_FLAG::EXECUTABLE>(section.header_ptr->flags);
		if (!is_writable && !is_exec) {
			if (section.name == ".rodata") {
				// always place .rodata section at the front, since we might need to perform relocations on it
				ro_sections.insert(ro_sections.begin(), pair { &section, 0u });
			} else {
				ro_sections.emplace_back(pair { &section, 0u });
			}
		}
	}
	
	// allocate all read-only sections
	uint64_t ro_size = 0;
	for (auto& section : ro_sections) {
		const auto& sec = *section.first->header_ptr;
		if (ro_size % sec.alignment != 0u) {
			// alignment padding
			ro_size += sec.alignment - (ro_size % sec.alignment);
		}
		section.second = ro_size;
		ro_size += sec.size;
	}
	
	ro_memory = make_aligned_ptr<uint8_t>(ro_size);
	memset(ro_memory.get(), 0, ro_memory.allocation_size());
	for (auto& section : ro_sections) {
		const auto& sec = *section.first->header_ptr;
		const auto& offset = section.second;
		memcpy(ro_memory.get() + offset, &binary[sec.offset], sec.size);
		section_map.emplace(section.first, ro_memory.get() + offset);
	}
	if (!ro_memory.pin()) {
		log_error("failed to pin read-only memory: %s", strerror(errno));
		return false;
	}
	
	return true;
}

bool elf_binary::map_global_ro_memory() {
	if (info->relocate_rodata) {
		// nothing to do here
		return true;
	}
	
	if (!map_ro_memory(info->ro_memory, info->sections, binary.get(), info->ro_section_map)) {
		return false;
	}
	
	// no longer need to modify memory -> can set to read-only now
	if (!info->ro_memory.set_protection(decltype(info->ro_memory)::PAGE_PROTECTION::READ_ONLY)) {
		log_error("failed to set read-only memory protection");
		return false;
	}
	
	return true;
}

bool elf_binary::instantiate(const uint32_t instance_idx) {
	if (!info || !info->is_valid()) {
		log_error("parsed ELF info is invalid");
		return false;
	}
	if (instance_idx >= info->instances.size()) {
		log_error("instance index is out-of-bounds: %u", instance_idx);
		return false;
	}
	
	auto& instance = info->instances[instance_idx];
	auto& ext_instance = instance.external_instance;
	
#if 0 // debug info
	uint64_t req_memory = 0;
	for (const auto& section : info->sections) {
		if (has_flag<ELF_SECTION_FLAG::ALLOCATE>(section.header_ptr->flags)) {
			log_undecorated("section %s is in memory, using %u bytes (addr: %u)",
							section.name, section.header_ptr->size, section.header_ptr->address);
			req_memory += section.header_ptr->size;
		}
	}
	for (const auto& section : info->sections) {
		section.dump(cout);
	}
	for (const auto& rela : info->relocations) {
		rela.dump(cout, info->sections, info->symbols);
	}
	for (const auto& sym : info->symbols) {
		sym.dump(cout, info->sections);
		if (!sym.name.empty() && sym.symbol_ptr->section_header_table_index == 0 &&
			(sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::GLOBAL || sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::WEAK)) {
			// -> external
#if !defined(__WINDOWS__)
			const auto ext_sym_ptr = dlsym(RTLD_DEFAULT, sym.name.c_str());
			cout << "\t-> external symbol ptr: 0x" << hex << uppercase << to_string(uint64_t(ext_sym_ptr)) << nouppercase << dec << endl;
#else
			log_error("not implemented yet");
			return false;
#endif
		}
	}
#endif
	
#if !defined(__WINDOWS__)
	// allocate/map read-only memory (if necessary)
	if (info->relocate_rodata) {
		if (!map_ro_memory(instance.ro_memory, info->sections, binary.get(), instance.section_map)) {
			log_error("failed to map read-only memory for instance");
			return false;
		}
	}
	// else: use the global read-only memory / section
	else {
		// add pre-existing global read-only sections
		for (const auto& ro_section : info->ro_section_map) {
			instance.section_map.emplace(ro_section.first, ro_section.second);
		}
	}
	
	// find all read-write and exec sections that need to be allocated:
	// section -> offset
	vector<pair<const section_t*, uint64_t>> rw_sections;
	vector<pair<const section_t*, uint64_t>> exec_sections;
	for (const auto& section : info->sections) {
		if (!has_flag<ELF_SECTION_FLAG::ALLOCATE>(section.header_ptr->flags)) {
			continue;
		}
		
		const auto is_writable = has_flag<ELF_SECTION_FLAG::WRITE>(section.header_ptr->flags);
		const auto is_exec = has_flag<ELF_SECTION_FLAG::EXECUTABLE>(section.header_ptr->flags);
		if (is_writable && !is_exec) {
			rw_sections.emplace_back(pair { &section, 0u });
		} else if (!is_writable && is_exec) {
			exec_sections.emplace_back(pair { &section, 0u });
		}
	}
	
	// we should have exactly one exec section and one/zero BSS
	if (exec_sections.size() != 1) {
		log_error("must have exactly one exec section");
		return false;
	}
	if (rw_sections.size() > 1) {
		log_error("must have zero or one BSS / read-write section");
		return false;
	}
	
	// allocate read-write/BSS section
	if (!rw_sections.empty()) {
		const auto& sec = *rw_sections.front().first->header_ptr;
		instance.rw_memory = make_aligned_ptr<uint8_t>(sec.size);
		memset(instance.rw_memory.get(), 0, instance.rw_memory.allocation_size());
		if (!instance.rw_memory.pin()) {
			log_error("failed to pin read-write/BSS memory: %s", strerror(errno));
			return false;
		}
		if (!instance.rw_memory.set_protection(decltype(instance.rw_memory)::PAGE_PROTECTION::READ_WRITE)) {
			log_error("failed to set read-write/BSS memory protection");
			return false;
		}
		instance.section_map.emplace(rw_sections.front().first, instance.rw_memory.get());
		
		ext_instance.rw_memory = instance.rw_memory.get();
		ext_instance.rw_memory_size = instance.rw_memory.allocation_size();
	}
	
	// allocate read-exec section
	{
		const auto& sec = *exec_sections.front().first->header_ptr;
		instance.exec_memory = make_aligned_ptr<uint8_t>(sec.size);
		memcpy(instance.exec_memory.get(), &binary[sec.offset], sec.size);
		if (sec.size < instance.exec_memory.allocation_size()) {
			memset(instance.exec_memory.get() + sec.size, 0, instance.exec_memory.allocation_size() - sec.size);
		}
		if (!instance.exec_memory.pin()) {
			log_error("failed to pin exec memory: %s", strerror(errno));
			return false;
		}
		// NOTE: delay rx protection to after we've done the relocations
		instance.section_map.emplace(exec_sections.front().first, instance.exec_memory.get());
	}
	
	// can now get the function pointers
	for (const auto& sym : info->symbols) {
		if (sym.name.empty() || !(sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::GLOBAL && sym.symbol_ptr->type == ELF_SYMBOL_TYPE::CODE)) {
			continue;
		}
		if (&info->sections[sym.symbol_ptr->section_header_table_index] != exec_sections[0].first) {
			continue;
		}
		ext_instance.functions.insert(sym.name, instance.exec_memory.get() + sym.symbol_ptr->value);
	}
	
	// relocate
	const auto get_external_symbol_ptr = [](const string& name) {
		return dlsym(RTLD_DEFAULT, name.c_str());
	};
	
	const auto resolve_symbol = [&instance, &ext_instance, &get_external_symbol_ptr](const symbol_t& sym) -> const void* {
		const void* ext_sym_ptr = nullptr;
		
		// TODO: handle floor specific symbols that need to be handled per execution later on
		// TODO: retrieve all allowed external functions/ptrs before loading a binary
		if (sym.name == "floor_global_idx") {
			ext_sym_ptr = &ext_instance.ids.instance_global_idx;
		} else if (sym.name == "floor_global_work_size") {
			ext_sym_ptr = &ext_instance.ids.instance_global_work_size;
		} else if (sym.name == "floor_local_idx") {
			ext_sym_ptr = &ext_instance.ids.instance_local_idx;
		} else if (sym.name == "floor_local_work_size") {
			ext_sym_ptr = &ext_instance.ids.instance_local_work_size;
		} else if (sym.name == "floor_group_idx") {
			ext_sym_ptr = &ext_instance.ids.instance_group_idx;
		} else if (sym.name == "floor_group_size") {
			ext_sym_ptr = &ext_instance.ids.instance_group_size;
		} else if (sym.name == "floor_work_dim") {
			ext_sym_ptr = &ext_instance.ids.instance_work_dim;
		} else if (sym.name == "global_barrier" ||
				   sym.name == "local_barrier" ||
				   sym.name == "barrier" ||
				   sym.name == "image_barrier") {
			ext_sym_ptr = get_external_symbol_ptr("host_compute_device_barrier");
		} else if (sym.name == "_GLOBAL_OFFSET_TABLE_") {
			if (instance.GOT.empty()) {
				log_error("GOT is empty");
				return nullptr;
			}
			ext_sym_ptr = &instance.GOT[0];
		} else {
			// normal symbol
			ext_sym_ptr = get_external_symbol_ptr(sym.name);
		}
		if (ext_sym_ptr == nullptr) {
			log_error("external symbol %u could not be resolved", sym.name);
			return nullptr;
		}
		return ext_sym_ptr;
	};
	
	const auto resolve_section = [this, &instance](const symbol_t& sym) -> const void* {
		if (sym.symbol_ptr->section_header_table_index >= info->sections.size()) {
			log_error("section index is out-of-bounds: %u", sym.symbol_ptr->section_header_table_index);
			return nullptr;
		}
		const auto& sec = info->sections[sym.symbol_ptr->section_header_table_index];
		const auto sec_iter = instance.section_map.find(&sec);
		if (sec_iter == instance.section_map.end()) {
			log_error("failed to find section: %u", sym.symbol_ptr->section_header_table_index);
			return nullptr;
		}
		return sec_iter->second;
	};
	
	const auto resolve = [resolve_symbol, resolve_section](const relocation_t& relocation) -> const void* {
		const void* resolved_ptr = nullptr;
		const auto& reloc = *relocation.reloc_ptr;
		if (reloc.symbol_index != 0) {
			// symbol relocation
			if (relocation.symbol_ptr) {
				const auto& sym = *relocation.symbol_ptr;
				if (sym.symbol_ptr->section_header_table_index == 0 &&
					(sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::GLOBAL || sym.symbol_ptr->binding == ELF_SYMBOL_BINDING::WEAK)) {
					// -> external
					resolved_ptr = resolve_symbol(sym);
				} else {
					if (sym.symbol_ptr->type == ELF_SYMBOL_TYPE::SECTION || sym.symbol_ptr->type == ELF_SYMBOL_TYPE::CODE) {
						resolved_ptr = resolve_section(sym);
					} else {
						log_error("non-external symbol for relocation: %s", sym.name);
						return nullptr;
					}
				}
			} else {
				log_error("invalid symbol index for relocation: %u", reloc.symbol_index);
				return nullptr;
			}
		} else {
			log_error("section relocation not implemented yet");
			return nullptr;
		}
		return resolved_ptr;
	};
	
	// figure out how many GOT entries we need
	uint64_t got_entry_count = 0;
	for (const auto& relocation : info->exec_relocations) {
#if !defined(FLOOR_IOS)
		if (relocation.reloc_ptr->type_x86_64 == ELF_RELOCATION_TYPE_X86_64::GOT64) {
			++got_entry_count;
		}
#else
		// TODO: implement this
		break;
#endif
	}
	for (const auto& relocation : info->rodata_relocations) {
#if !defined(FLOOR_IOS)
		if (relocation.reloc_ptr->type_x86_64 == ELF_RELOCATION_TYPE_X86_64::GOT64) {
			++got_entry_count;
		}
#else
		// TODO: implement this
		break;
#endif
	}
	instance.init_GOT(got_entry_count);
	
	// perform relocations in exec memory and optionally rodata memory
	const auto perform_relocations = [&instance, &resolve](const vector<relocation_t>& relocations, aligned_ptr<uint8_t>& memory) {
		for (const auto& relocation : relocations) {
			const auto& reloc = *relocation.reloc_ptr;
#if !defined(FLOOR_IOS)
			switch (reloc.type_x86_64) {
				case ELF_RELOCATION_TYPE_X86_64::GOT64: /* G (GOT offset) + Addend */ {
					if (reloc.addend != 0) {
						// TODO: handle/implement this
						log_error("addend not handled yet for GOT64");
						return false;
					}
					
					const auto GOT_offset = instance.allocate_GOT_entries(1);
					const void* resolved_ptr = resolve(relocation);
					if (resolved_ptr == nullptr) {
						log_error("failed to resolve symbol");
						return false;
					}
					
					// update GOT entry
					instance.GOT[GOT_offset] = (uint64_t)resolved_ptr;
					const auto value = int64_t(GOT_offset * sizeof(uint64_t)) + reloc.addend;
					
					// relocate in code
					if (reloc.offset + sizeof(uint64_t) > memory.allocation_size()) {
						log_error("relocation offset is out-of-bounds: %u", reloc.offset);
						return false;
					}
					memcpy(memory.get() + reloc.offset, &value, sizeof(value));
					break;
				}
				case ELF_RELOCATION_TYPE_X86_64::GOTPC64: /* GOT - P (place/offset) + Addend */ {
					// NOTE: specified symbol is ignored for this
					const auto GOT_start_ptr = (int64_t)&instance.GOT[0];
					const auto place = int64_t(memory.get() + reloc.offset);
					const auto ptr_value = (GOT_start_ptr + reloc.addend) - place;
					
					// relocate in code
					if (reloc.offset + sizeof(uint64_t) > memory.allocation_size()) {
						log_error("relocation offset is out-of-bounds: %u", reloc.offset);
						return false;
					}
					memcpy(memory.get() + reloc.offset, &ptr_value, sizeof(ptr_value));
					break;
				}
				case ELF_RELOCATION_TYPE_X86_64::GOTOFF64: /* L (PLT place) - GOT + Addend */ {
					const auto resolved_ptr = (const uint8_t*)resolve(relocation);
					if (resolved_ptr == nullptr) {
						log_error("failed to resolve symbol");
						return false;
					}
					
					const auto GOT_start_ptr = (const uint8_t*)&instance.GOT[0];
					const auto ptr_value = uint64_t(resolved_ptr - GOT_start_ptr + reloc.addend);
					
					// relocate in code
					if (reloc.offset + sizeof(uint64_t) > memory.allocation_size()) {
						log_error("relocation offset is out-of-bounds: %u", reloc.offset);
						return false;
					}
					memcpy(memory.get() + reloc.offset, &ptr_value, sizeof(ptr_value));
					break;
				}
				case ELF_RELOCATION_TYPE_X86_64::PC32: /* Symbol + Addend - P (place/offset) */ {
					const auto resolved_ptr = (const uint8_t*)resolve(relocation);
					if (resolved_ptr == nullptr) {
						log_error("failed to resolve symbol");
						return false;
					}
					
					const auto place = int64_t(memory.get() + reloc.offset);
					const auto offset_value = int32_t(int64_t(resolved_ptr) + reloc.addend - place);
					
					// relocate in code
					if (reloc.offset + sizeof(uint64_t) > memory.allocation_size()) {
						log_error("relocation offset is out-of-bounds: %u", reloc.offset);
						return false;
					}
					memcpy(memory.get() + reloc.offset, &offset_value, sizeof(offset_value));
					break;
				}
				default:
					log_error("unhandled relocation type: %u", reloc.type_x86_64);
					return false;
			}
#else
			log_error("ARM relocation not implemented yet");
			return false;
#endif
		}
		return true;
	};
	if (!perform_relocations(info->exec_relocations, instance.exec_memory)) {
		return false;
	}
	if (info->relocate_rodata) {
		if (!perform_relocations(info->rodata_relocations, instance.ro_memory)) {
			return false;
		}
	}
	
#else // TODO: Windows
	(void)ext_instance;
	log_error("not implemented yet");
	return false;
#endif

#if !defined(__WINDOWS__) // TODO: remove this once Windows is supported, right now this is unreachable
	// can now set the protection on the read-exec and read-only sections
	{
		if (!instance.exec_memory.set_protection(decltype(instance.exec_memory)::PAGE_PROTECTION::READ_EXEC)) {
			log_error("failed to set exec memory protection");
			return false;
		}
		
#if defined(__APPLE__)
		// on Apple platforms, we also need to call vm_protect to set/seal the max protection
		if (vm_protect(mach_task_self(), (vm_address_t)instance.exec_memory.get(), instance.exec_memory.allocation_size(), 1,
					   VM_PROT_READ | VM_PROT_EXECUTE) != KERN_SUCCESS) {
			log_error("failed to set exec memory protection (mach)");
			return false;
		}
#endif
		
		if (info->relocate_rodata) {
			if (!instance.ro_memory.set_protection(decltype(instance.ro_memory)::PAGE_PROTECTION::READ_ONLY)) {
				log_error("failed to set read-only memory protection");
				return false;
			}
		}
	}
	
	return true;
#endif
}

FLOOR_POP_WARNINGS()

#endif
