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

#include <floor/core/core.hpp>
#include <floor/core/unicode.hpp>
#include <floor/core/cpp_ext.hpp>
#include <thread>
#include <chrono>
#include <cassert>

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <strsafe.h>
#include <floor/core/essentials.hpp> // cleanup

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#endif
#endif

#if !defined(__WINDOWS__)
#include <dirent.h>
#endif

#if defined(__x86_64__)
#if defined(__cpuid)
#error "__cpuid should not yet be defined at this point!"
#endif
#include <cpuid.h>
#elif defined(__APPLE__) && defined(__aarch64__)
#include <mach-o/arch.h>
#else
#error "unhandled arch"
#endif

#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#include <mach/mach_host.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

// provided by SDL3
extern "C" SDL_DECLSPEC int SDLCALL SDL_GetSystemRAM();

namespace fl::core {
static std::random_device rd {};
static std::mt19937 gen { rd() };
std::mt19937& _get_gen() {
	return gen;
}

int2 get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	// transform vector (*TMVP)
	const float3 mview_vec = vec * mview; // save tmp result, needed later
	float3 proj_vec = mview_vec * mproj;
	
	// check if point is behind cam / "invisible"
	if(mview_vec.z >= 0.0f) {
		return int2 { std::numeric_limits<int>::min() };
	}
	
	proj_vec *= -1.0f / mview_vec.z;
	
	// and finally: compute window position
	const auto viewportf = viewport.cast<float>();
	return {
		(int)(viewportf[2] * (proj_vec.x * 0.5f + 0.5f) + viewportf[0]),
		(int)(viewportf[3] - viewportf[3] * (proj_vec.y * 0.5f + 0.5f) + viewportf[1]) // flip y
	};
}

float3 get_3d_from_2d(const uint2& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	const matrix4f ipm((mview * mproj).invert());
	const float3 wnd_vec((((float(p.x) - float(viewport[0])) * 2.0f) / float(viewport[2])) - 1.0f,
						 (((float(p.y) - float(viewport[1])) * 2.0f) / float(viewport[3])) - 1.0f,
						 1.0f);
	return (wnd_vec * ipm);
}

std::string find_and_replace(const std::string& str, const std::string& find, const std::string& repl) {
	std::string ret = str;
	find_and_replace(ret, find, repl);
	return ret;
}

void find_and_replace(std::string& str, const std::string& find, const std::string& repl) {
	// consecutive search and replace routine
	const size_t find_len = find.size();
	const size_t replace_len = repl.size();
	if(find_len == 0) return; // replace_len might be 0 (if it's an empty string -> "")
	
	size_t pos = 0, old_pos = 0;
	while((pos = str.find(find, old_pos)) != std::string::npos) {
		str.replace(pos, find_len, repl.c_str(), replace_len);
		old_pos = pos + replace_len;
	}
}

std::string find_and_replace_once(const std::string& str, const std::string& find_str, const std::string& repl_str, const size_t start_pos) {
	std::string ret = str;
	find_and_replace_once(ret, find_str, repl_str, start_pos);
	return ret;
}

void find_and_replace_once(std::string& str, const std::string& find_str, const std::string& repl_str, const size_t start_pos) {
	const size_t find_len = find_str.size();
	const size_t replace_len = repl_str.size();
	if(find_len == 0) return;
	
	size_t pos;
	if((pos = str.find(find_str, start_pos)) != std::string::npos) {
		str.replace(pos, find_len, repl_str.c_str(), replace_len);
	}
}

#define tokenize_algorithm(delim_size, str_type) \
std::vector<str_type> dst; \
size_t pos = 0; \
size_t old_pos = 0; \
if(src.find(delim, pos) != std::string::npos) { \
	while((pos = src.find(delim, old_pos)) != std::string::npos) { \
		dst.emplace_back(src.substr(old_pos, pos - old_pos)); \
		old_pos = pos + delim_size; \
	} \
	dst.emplace_back(src.substr(old_pos, pos - old_pos)); \
} \
else dst.emplace_back(src); \
return dst;

std::vector<std::string> tokenize(const std::string& src, const char& delim) {
	tokenize_algorithm(1, std::string)
}

std::vector<std::string> tokenize(const std::string& src, const std::string& delim) {
	const size_t delim_size = delim.size();
	tokenize_algorithm(delim_size, std::string)
}

std::vector<std::string_view> tokenize_sv(const std::string_view& src, const char& delim) {
	tokenize_algorithm(1, std::string_view)
}

std::vector<std::string_view> tokenize_sv(const std::string_view& src, const std::string_view& delim) {
	const size_t delim_size = delim.size();
	tokenize_algorithm(delim_size, std::string_view)
}

void str_to_lower_inplace(std::string& str) {
	transform(str.begin(), str.end(), str.begin(), ::tolower);
}

void str_to_upper_inplace(std::string& str) {
	transform(str.begin(), str.end(), str.begin(), ::toupper);
}

std::string str_to_lower(const std::string& str) {
	std::string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ::tolower);
	return ret;
}

std::string str_to_upper(const std::string& str) {
	std::string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ::toupper);
	return ret;
}

std::string strip_path(const std::string& in_path) {
	// TODO: use std::filesystem for this (canonical path?)
	std::string path = in_path;
	size_t pos = 0, erase_pos;
	while (path.size() >= 3 && (pos = path.find("../", 0)) != std::string::npos && pos > 1) {
		erase_pos = path.rfind('/', pos - 2);
#if defined(__WINDOWS__)
		if (erase_pos == std::string::npos) {
			erase_pos = path.rfind('\\', pos - 2);
		}
#endif
		if (erase_pos != std::string::npos) {
			path.erase(erase_pos + 1, pos + 2 - erase_pos);
		}
	}
	
#if defined(__WINDOWS__) // additional windows path handling
	pos = 0;
	while (path.size() >= 3 && (pos = path.find("..\\", 0)) != std::string::npos && pos > 1) {
		erase_pos = path.rfind('/', pos - 2);
		if (erase_pos == std::string::npos) {
			erase_pos = path.rfind('\\', pos - 2);
		}
		if (erase_pos != std::string::npos) {
			path.erase(erase_pos + 1, pos + 2 - erase_pos);
		}
	}
#endif
	
	if (!path.empty() && path[path.size() - 1] != '/' && path[path.size() - 1] != '\\') {
		pos = path.rfind('/');
		if (pos == std::string::npos) {
			pos = path.rfind('\\');
		}
		if (pos == std::string::npos) {
			path = "/"; // sth is wrong?
		} else {
			path = path.substr(0, pos + 1);
		}
	}
	
	return path;
}

std::string strip_filename(const std::string& in_path) {
	std::string filename = in_path;
	const auto slash_pos = in_path.rfind('/');
	if(slash_pos != std::string::npos) {
		filename = filename.substr(slash_pos + 1);
	}
	
#if defined(__WINDOWS__)
	const auto backslash_pos = in_path.rfind('\\');
	if(backslash_pos != std::string::npos) {
		filename = filename.substr(backslash_pos + 1);
	}
#endif
	
	return filename;
}

std::string trim(const std::string& str) {
	if(str.length() == 0) return "";
	
	std::string ret = str;
	size_t pos = 0;
	while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n' || ret[pos] == '\0')) {
		ret.erase(pos, 1);
	}
	
	if(ret.length() > 0) {
		pos = ret.length()-1;
		while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n' || ret[pos] == '\0')) {
			ret.erase(pos, 1);
			if(pos > 0) pos--;
		}
	}
	return ret;
}

std::string escape_string(const std::string& str) {
	if(str.size() == 0) return str;
	
	std::string ret = str;
	size_t pos = 0;
	for(;;) {
		if((pos = ret.find_first_of("\'\"\\", pos)) == std::string::npos) break;
		ret.insert(pos, "\\");
		pos += 2;
	}
	return ret;
}

std::map<std::string, file_io::FILE_TYPE> get_file_list(const std::string& directory,
											  const std::string file_extension,
											  const bool always_get_folders) {
	std::map<std::string, file_io::FILE_TYPE> file_list;
	std::string dir_str = directory;
	size_t pos{ 0 };

	// get file list (os specific)
#if defined(__WINDOWS__)
	struct _finddata_t c_file;
	intptr_t h_file;
	
	dir_str += "*";
	if((h_file = _findfirst(dir_str.c_str(), &c_file)) != -1L) {
		do {
			const std::string name = c_file.name;
			const bool is_folder = (c_file.attrib & _A_SUBDIR);
			if(file_extension != "" && (!is_folder || (is_folder && !always_get_folders))) {
				if((pos = name.rfind(".")) != std::string::npos) {
					if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
				}
				else continue; // no extension
			}
			
			if(is_folder) file_list[name] = file_io::FILE_TYPE::DIR;
			else file_list[name] = file_io::FILE_TYPE::NONE;
		}
		while(_findnext(h_file, &c_file) == 0);
		
		_findclose(h_file);
	}
#else // works under Linux, macOS, FreeBSD (and probably all other posix systems)
	struct dirent** namelist = nullptr;
	
	dir_str += ".";
	int n = scandir(dir_str.c_str(), &namelist, nullptr, alphasort);
	
	for(int j = 1; j < n; j++) {
#if !defined(__APPLE__)
		// TODO: this probably needs some conversion as well, but there is no way to find out using only posix functions ...
		const std::string name = namelist[j]->d_name;
#else
		const std::string name = unicode::utf8_decomp_to_precomp(namelist[j]->d_name);
#endif
		const bool is_folder = (namelist[j]->d_type == 4);
		if(!file_extension.empty() && (!is_folder || (is_folder && !always_get_folders))) {
			if((pos = name.rfind('.')) != std::string::npos) {
				if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
			}
			else continue; // no extension
		}

		// TODO: use sys/stat.h instead (glibc has some issues where DT_DIR is not defined or recursively-self-defined ...)
		// note: 4 == DT_DIR
		if(is_folder) file_list[name] = file_io::FILE_TYPE::DIR;
		else file_list[name] = file_io::FILE_TYPE::NONE;
	}
	
	if(namelist != nullptr) {
		for(int i = 0; i < n; i++) {
			free(namelist[i]);
		}
		free(namelist);
	}
#endif
	
	return file_list;
}

void compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal) {
	normal = (v2 - v1).cross(v3 - v1);
	normal.normalize();
}

void compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
									 float3& normal, float3& binormal, float3& tangent,
									 const float2& t1, const float2& t2, const float2& t3) {
	// compute deltas
	const float delta_x1 = t2.x - t1.x;
	const float delta_y1 = t2.y - t1.y;
	const float delta_x2 = t3.x - t1.x;
	const float delta_y2 = t3.y - t1.y;
	
	// normal
	const float3 edge1(v2 - v1);
	const float3 edge2(v3 - v1);
	normal = edge1.crossed(edge2);
	normal.normalize();
	
	// binormal
	binormal = (edge1 * delta_x2) - (edge2 * delta_x1);
	binormal.normalize();
	
	// tangent
	tangent = (edge1 * delta_y2) - (edge2 * delta_y1);
	tangent.normalize();
	
	// adjust
	float3 txb = tangent.crossed(binormal);
	if(normal.dot(txb) > 0.0f) {
		tangent *= -1.0f;
	}
	else binormal *= -1.0f;
}

void system(const std::string& cmd) {
	std::string str_dump;
	core::system(cmd, str_dump);
}

void system(const std::string& cmd, std::string& output) {
	static constexpr size_t buffer_size = 8192;
	std::string buffer(buffer_size + 1, 0);
	
#if defined(_MSC_VER)
#define popen _popen
#define pclose _pclose
#endif
	FILE* sys_pipe = popen(
#if !defined(__WINDOWS__)
						   cmd.c_str()
#else // always wrap commands in quotes, b/c #windowsproblems
						   ("\"" + cmd + "\"").c_str()
#endif
						   , "r");
	if (!sys_pipe) {
		log_error("failed to execute system command: $", cmd);
		return;
	}
	for (;;) {
		const auto read_count = fread(buffer.data(), 1u, buffer_size, sys_pipe);
		if (read_count == 0) {
			break;
		}
		output.append(buffer.data(), read_count);
		memset(buffer.data(), 0, read_count); // size+1 is always 0
		if (feof(sys_pipe) != 0 || ferror(sys_pipe) != 0) {
			break;
		}
	}
	pclose(sys_pipe);
}

void set_random_seed(const uint32_t& seed) {
	gen.seed(seed);
}

std::string encode_url(const std::string& url) {
	std::stringstream result;
	for(auto citer = url.begin(); citer != url.end(); citer++) {
		switch(*citer) {
			case '!':
			case '#':
			case '$':
			case ';':
			case ':':
			case '=':
			case '?':
			case '@':
				result << *citer;
				break;
			default:
				if((*citer >= 'a' && *citer <= 'z') ||
				   (*citer >= 'A' && *citer <= 'Z') ||
				   (*citer >= '0' && *citer <= '9') ||
				   (*citer >= '&' && *citer <= '*') ||
				   (*citer >= ',' && *citer <= '/') ||
				   (*citer >= '[' && *citer <= '`') ||
				   (*citer >= '{' && *citer <= '~')) {
					result << *citer;
					break;
				}
				result << '%' << std::uppercase << std::hex << (*citer & 0xFF);
				break;
		}
	}
	return result.str();
}

static inline char bits_to_hex(const uint8_t& bits) {
	switch(bits) {
		default: break;
		case 0x0: return '0';
		case 0x1: return '1';
		case 0x2: return '2';
		case 0x3: return '3';
		case 0x4: return '4';
		case 0x5: return '5';
		case 0x6: return '6';
		case 0x7: return '7';
		case 0x8: return '8';
		case 0x9: return '9';
		case 0xA: return 'A';
		case 0xB: return 'B';
		case 0xC: return 'C';
		case 0xD: return 'D';
		case 0xE: return 'E';
		case 0xF: return 'F';
	}
	return '0';
}

std::string str_hex_escape(const std::string& str) {
	std::string ret;
	ret.reserve(str.size() * 4);
	
	for(const auto& ch : str) {
		ret.push_back('\\');
		ret.push_back('x');
		const uint8_t& uch = *(const uint8_t*)&ch;
		ret.push_back(bits_to_hex((uch >> 4) & 0xF));
		ret.push_back(bits_to_hex(uch & 0xF));
	}
	
	return ret;
}

std::string cptr_hex_escape(const char* data, const size_t size) {
	std::string ret;
	const uint8_t* ch_ptr = (const uint8_t*)data;
	if(size != 0) {
		ret.reserve(size * 4);
		for(size_t i = 0; i < size; ++i, ++ch_ptr) {
			ret.push_back('\\');
			ret.push_back('x');
			ret.push_back(bits_to_hex((*ch_ptr >> 4) & 0xF));
			ret.push_back(bits_to_hex(*ch_ptr & 0xF));
		}
	}
	else {
		for(; *ch_ptr != '\0'; ++ch_ptr) {
			ret.push_back('\\');
			ret.push_back('x');
			ret.push_back(bits_to_hex((*ch_ptr >> 4) & 0xF));
			ret.push_back(bits_to_hex(*ch_ptr & 0xF));
		}
	}
	return ret;
}

uint32_t unix_timestamp() {
	return (uint32_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t unix_timestamp_ms() {
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t unix_timestamp_us() {
	return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t get_total_system_memory() {
	return uint64_t(SDL_GetSystemRAM()) * 1024ull * 1024ull;
}

uint64_t get_free_system_memory() {
	uint64_t free_mem = 0u;
#if defined(__APPLE__)
	vm_statistics64_data_t vm_stats {};
	auto host_info_cnt = HOST_VM_INFO64_COUNT;
	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &host_info_cnt) == KERN_SUCCESS) {
		free_mem = uint64_t(vm_stats.free_count) * aligned_ptr<int>::page_size;
	}
#elif defined(__linux__)
	struct sysinfo info {};
	if (sysinfo(&info) == 0) {
		free_mem = uint64_t(info.freeram) * uint64_t(info.mem_unit);
	}
#elif defined(__WINDOWS__)
	MEMORYSTATUSEX mem_status {};
	mem_status.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&mem_status) > 0) {
		free_mem = uint64_t(mem_status.ullAvailPhys);
	}
#endif
	return free_mem;
}

#if defined(__WINDOWS__)
uint4 get_windows_version() {
	// only do this once
	static const uint4 version = []() -> uint4 {
		// dynamically load version.dll (don't want to link it)
		auto version_handle = LoadLibrary("version.dll");
		const auto win_ver_fail = [&version_handle]() -> uint4 {
			if (version_handle != nullptr) {
				FreeLibrary(version_handle);
			}
			return { 0, 0, 0, 0 };
		};
		if (!version_handle) {
			return win_ver_fail();
		}
		
		// get needed function pointers
		using get_file_version_info_fptr = int /* bool */ (*)(const char* /* filename */, const uint32_t /* handle */, const uint32_t /* len */, void* /* data */);
		using get_file_version_info_size_fptr = uint32_t /* size */ (*)(const char* /* filename */, uint32_t* /* handle */);
		using ver_query_value_fptr = int /* bool */ (*)(const void* /* block */, const char* /* sub_block */, void* /* ret_buffer */, uint32_t* /* ret_len */);
		auto get_file_version_info = (get_file_version_info_fptr)(void*)GetProcAddress(version_handle, "GetFileVersionInfoA");
		auto get_file_version_info_size = (get_file_version_info_size_fptr)(void*)GetProcAddress(version_handle, "GetFileVersionInfoSizeA");
		auto ver_query_value = (ver_query_value_fptr)(void*)GetProcAddress(version_handle, "VerQueryValueA");
		if (get_file_version_info == nullptr || get_file_version_info_size == nullptr || ver_query_value == nullptr) {
			return win_ver_fail();
		}
		
		// get the full kernel32.dll version info
		uint32_t dummy_handle = 0;
		const auto kernel_version_info_size = get_file_version_info_size("kernel32.dll", &dummy_handle);
		if (kernel_version_info_size == 0) {
			return win_ver_fail();
		}
		
		auto kernel_version_info = std::make_unique<uint8_t[]>(kernel_version_info_size);
		if (!get_file_version_info("kernel32.dll", 0 /* dummy handle */, kernel_version_info_size, kernel_version_info.get())) {
			return win_ver_fail();
		}
		
		// query the version info we're actually interested in
		struct fixed_file_info_t {
			uint32_t signature;
			uint32_t struc_version;
			uint32_t file_version_ms;
			uint32_t file_version_ls;
			uint32_t product_version_ms;
			uint32_t product_version_ls;
			uint32_t file_flags_mask;
			uint32_t file_flags;
			uint32_t file_os;
			uint32_t file_type;
			uint32_t file_subtype;
			uint32_t file_date_ms;
			uint32_t file_date_ls;
		};
		fixed_file_info_t* fixed_file_info = nullptr;
		uint32_t fixed_file_info_len = 0;
		if (!ver_query_value(kernel_version_info.get(), "\\", &fixed_file_info, &fixed_file_info_len)) {
			return win_ver_fail();
		}
		if (fixed_file_info_len == 0) {
			return win_ver_fail();
		}
		
		// all okay, parse the version info
		return {
			(fixed_file_info->product_version_ms >> 16u) & 0xFFFFu,
			(fixed_file_info->product_version_ms & 0xFFFFu),
			(fixed_file_info->product_version_ls >> 16u) & 0xFFFFu,
			(fixed_file_info->product_version_ls & 0xFFFFu)
		};
	}();
	return version;
}
#endif

bool cpu_has_fma() {
#if defined(__x86_64__)
	int eax, ebx, ecx, edx;
	__cpuid(1, eax, ebx, ecx, edx);
	return (ecx & bit_FMA) > 0;
#else
	return true; // armv8
#endif
}

bool cpu_has_avx() {
#if defined(__x86_64__)
	int eax, ebx, ecx, edx;
	__cpuid(1, eax, ebx, ecx, edx);
	return (ecx & bit_AVX) > 0;
#else
	return false;
#endif
}

bool cpu_has_avx2() {
#if defined(__x86_64__)
	uint32_t eax { 0 }, ebx { 0 }, ecx { 0 }, edx { 0 };
	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) == 1) {
		return (ebx & 0x00000020) > 0;
	}
#endif
	return false;
}

bool cpu_has_avx512() {
#if defined(__x86_64__)
	uint32_t eax { 0 }, ebx { 0 }, ecx { 0 }, edx { 0 };
	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) == 1) {
		return (ebx & 0x00010000) > 0;
	}
#endif
	return false;
}

bool cpu_has_avx512_tier_5() {
#if defined(__x86_64__)
	bool has_first_features = false;
	{
		uint32_t eax { 0 }, ebx { 0 }, ecx { 0 }, edx { 0 };
		if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) == 1) {
			has_first_features = ((ebx & 0x100000) > 0 && // IFMA
								  (ecx & 0x2) > 0 && // VBMI
								  (ecx & 0x40) > 0 && // VBMI2
								  (ecx & 0x200) > 0 && // VAES
								  (ecx & 0x1000) > 0 && // BITALG
								  (ecx & 0x400) > 0 && // VPCLMULQDQ
								  (ecx & 0x100) > 0 && // GFNI
								  (ecx & 0x800) > 0 && // VNNI
								  (ecx & 0x4000) > 0); // VPOPCNTDQ
		}
	}
	if (has_first_features) {
		uint32_t eax { 0 }, ebx { 0 }, ecx { 0 }, edx { 0 };
		if (__get_cpuid_count(7, 1, &eax, &ebx, &ecx, &edx) == 1) {
			return ((eax & 0x20) > 0); // BF16
		}
	}
#endif
	return false;
}

std::string create_tmp_file_name(const std::string prefix, const std::string suffix) {
	std::seed_seq seed {
		rd(),
		(uint32_t)(uintptr_t)&prefix,
		unix_timestamp()
	};
	std::mt19937 twister { seed };
	twister.discard(32);
	
	static constexpr const char chars[] {
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
	};
	std::uniform_int_distribution<> dist(0, std::size(chars) - 1);
	
	std::string ret {
#if !defined(__WINDOWS__)
		"/tmp/" +
#endif
		prefix
	};
	
	static constexpr const int random_char_count { 16 };
	for(int i = 0; i < random_char_count; ++i) {
		ret += chars[dist(twister)];
	}
	
	ret += suffix;
	return ret;
}

std::string to_file_name(const std::string& str) {
	std::string ret = str;
	for(auto& ch : ret) {
		if((ch >= 'A' && ch <= 'Z') ||
		   (ch >= 'a' && ch <= 'z') ||
		   (ch >= '0' && ch <= '9') ||
		   (ch >= '#' && ch <= '.') ||
		   (ch >= ':' && ch <= '@') ||
		   (ch >= '{' && ch <= '~') ||
		   ch == '!' || ch == '[' ||
		   ch == ']' || ch == '^' ||
		   ch == '_' || ch == '`') {
			continue;
		}
		ch = '_';
	}
	return ret;
}

std::string expand_path_with_env(const std::string& in) {
#if defined(__WINDOWS__)
	static thread_local char expanded_path[32768]; // 32k is the max
	const auto expanded_size = ExpandEnvironmentStringsA(in.c_str(), expanded_path, 32767);
	if(expanded_size == 0 || expanded_size == 32767) {
		log_error("failed to expand file path: $", in);
		return in;
	}
	return std::string(expanded_path, std::min(uint32_t(expanded_size - 1), uint32_t(std::size(expanded_path) - 1)));
#else
	return in;
#endif
}

#if defined(__WINDOWS__)
std::string get_system_error() {
	// ref: https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-the-last-error-code
	const auto err = GetLastError();

	std::string msg;
	LPVOID err_msg = nullptr;
	if (const auto len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
										FORMAT_MESSAGE_FROM_SYSTEM |
										FORMAT_MESSAGE_IGNORE_INSERTS,
										nullptr, err,
										MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
										(LPSTR)&err_msg, 0, nullptr); len > 0) {
		msg = std::string_view { (const char*)err_msg, len };
		msg += " (" + std::to_string(err) + ")";
	} else {
		msg = std::to_string(err);
	}
	if (err_msg) {
		LocalFree(err_msg);
	}
	return msg;
}
#endif

} // namespace fl::core
