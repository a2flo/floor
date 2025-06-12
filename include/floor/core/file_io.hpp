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

#include <floor/core/platform.hpp>
#include <floor/core/aligned_ptr.hpp>
#include <fstream>
#include <span>

namespace fl {

//! file input/output
class file_io {
public:
	enum class OPEN_TYPE {
		READ,
		READWRITE,
		WRITE,
		READ_BINARY,
		READWRITE_BINARY,
		WRITE_BINARY,
		APPEND,
		APPEND_BINARY,
		APPEND_READ,
		APPEND_READ_BINARY
	};
	
	file_io() = default;
	file_io(const std::string& filename, const OPEN_TYPE open_type = OPEN_TYPE::READWRITE_BINARY);
	file_io(file_io&& fio) = default;
	~file_io();
	file_io& operator=(file_io&& fio) = default;
	
	enum class FILE_TYPE : uint32_t {
		//! any file
		NONE,
		//! any folder
		DIR,
	};
	
	static bool file_to_buffer(const std::string& filename, std::stringstream& buffer);
	static std::pair<std::unique_ptr<uint8_t[]>, size_t> file_to_buffer(const std::string& filename);
	static std::pair<aligned_ptr<uint8_t>, size_t> file_to_buffer_aligned(const std::string& filename);
	static std::pair<aligned_ptr<uint8_t>, size_t> file_to_buffer_uncached(const std::string& filename);
	static bool file_to_string(const std::string& filename, std::string& str);
	static std::string file_to_string(const std::string& filename);
	
	//! not recommended unless filesize can't be determined
	static bool file_to_string_poll(const std::string& filename, std::string& str);
	static std::string file_to_string_poll(const std::string& filename);
	
	static bool string_to_file(const std::string& filename, const std::string& str);
	static bool buffer_to_file(const std::string& filename, const char* data, const size_t& size);
	static bool buffer_to_file(const std::string& filename, const std::span<const uint8_t> data);
	
	//! opens the "filename" file with the specified "open_type" (read, write-binary, ...)
	bool open(const std::string& filename, OPEN_TYPE open_type);
	void close();
	//! returns the filesize
	long long int get_filesize();
	std::fstream* get_filestream();
	//! seeks to offset
	void seek(size_t offset);
	//! returns the current file offset
	std::streampos get_current_offset();
	
	// file input:
	bool read_file(std::stringstream& buffer);
	bool read_file(std::string& str);
	//! reads a line from the current input stream (pointless if we have a binary file)
	void get_line(char* finput, std::streamsize length);
	//! reads a block of "size" bytes
	void get_block(char* data, std::streamsize size);
	void get_terminated_block(std::string& str, const uint8_t terminator);
	std::string get_terminated_block(const uint8_t terminator);
	//! reads a single char from the current file input stream and returns it
	uint8_t get_char();
	//! reads a single uint16_t from the current file input stream and returns it
	uint16_t get_usint();
	//! reads a single uint32_t from the current file input stream and returns it
	uint32_t get_uint();
	//! reads a single uint64_t from the current file input stream and returns it
	uint64_t get_ullint();
	uint16_t get_swapped_usint();
	uint32_t get_swapped_uint();
	uint64_t get_swapped_ullint();
	//! reads a single float from the current file input stream and returns it
	float get_float();
	void seek_read(size_t offset);
	std::streampos get_current_read_offset();
	
	// file output:
	void write_file(const std::string& str);
	//! writes a block to the current file (offset)
	void write_block(const char* data, size_t size, bool check_size = false);
	//! writes a block to the current file (offset)
	void write_block(const void* data, size_t size, bool check_size = false);
	template <typename T> void write(const T& data) {
		write_block((const void*)&data, sizeof(T));
	}
	void write_terminated_block(const std::string& str, const uint8_t terminator);
	void write_char(const uint8_t& ch);
	void write_usint(const uint16_t& usi);
	void write_uint(const uint32_t& ui);
	void write_ullint(const uint64_t& ulli);
	void write_float(const float& f);
	void seek_write(size_t offset);
	std::streampos get_current_write_offset();
	
	//! returns true if the file specified by filename exists
	static bool is_file(const std::string& filename);
	static bool is_directory(const std::string& dirname);
	//! checks if we reached the end of file
	bool eof() const;
	bool good() const;
	bool fail() const;
	bool bad() const;
	bool is_open() const;
	
	static bool create_directory(const std::string& dirname);
	
	//! reads all data as binary from "filename" and returns it as a vector of the specified "data_type"
	template <typename data_type>
	static std::optional<std::vector<data_type>> file_to_vector(const std::string& filename) {
		file_io file(filename, file_io::OPEN_TYPE::READ_BINARY);
		if (!file.is_open()) {
			return {};
		}
		
		const auto size = (size_t)file.get_filesize();
		const size_t readable_count = size / sizeof(data_type); // drop last bytes if they don't fit
		const size_t readable_size = readable_count * sizeof(data_type);
		std::vector<data_type> ret(readable_count);
		if (ret.size() != readable_count) {
			return {};
		}
		
		file.get_filestream()->read((char*)&ret[0], (std::streamsize)readable_size);
		const auto read_size = file.get_filestream()->gcount();
		if (read_size != (decltype(read_size))readable_size) {
			log_error("expected $ bytes, but only read $ bytes", readable_size, read_size);
			return {};
		}
		file.close();
		
		return ret;
	}
	
protected:
	OPEN_TYPE open_type { OPEN_TYPE::READ_BINARY };
	std::string filename;
	std::fstream filestream;
	
	//! checks if a file is already opened - if so, return true, otherwise false
	bool check_open();
	
};

} // namespace fl
