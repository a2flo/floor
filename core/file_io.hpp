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

#ifndef __FLOOR_FILE_IO_HPP__
#define __FLOOR_FILE_IO_HPP__

#include <floor/core/platform.hpp>

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
	
	file_io();
	file_io(const string& filename, const OPEN_TYPE open_type = OPEN_TYPE::READWRITE_BINARY);
	file_io(file_io&& fio) = default;
	~file_io();
	file_io& operator=(file_io&& fio) = default;
	
	enum class FILE_TYPE : unsigned int {
		NONE,			//!< any file or folder
		DIR,			//!< any folder
		TEXT,			//!< *.txt
		IMAGE,			//!< *.png
		XML,			//!< *.xml
		OPENCL,			//!< *.cl *.clh *.h
		A2E_MODEL,		//!< *.a2m
		A2E_ANIMATION,	//!< *.a2a
		A2E_MATERIAL,	//!< *.a2mat
		A2E_UI,			//!< *.a2eui
	};
	
	static bool file_to_buffer(const string& filename, stringstream& buffer);
	static bool file_to_string(const string& filename, string& str);
	static string file_to_string(const string& filename);
	
	//! not recommended unless filesize can't be determined
	static bool file_to_string_poll(const string& filename, string& str);
	static string file_to_string_poll(const string& filename);
	
	static bool string_to_file(const string& filename, const string& str);
	static bool buffer_to_file(const string& filename, const char* buffer, const size_t& size);

	bool open(const string& filename, OPEN_TYPE open_type);
	void close();
	long long int get_filesize();
	fstream* get_filestream();
	void seek(size_t offset);
	streampos get_current_offset();
	
	// file input:
	bool read_file(stringstream& buffer);
	bool read_file(string& str);
	void get_line(char* finput, streamsize length);
	void get_block(char* data, streamsize size);
	void get_terminated_block(string& str, const uint8_t terminator);
	string get_terminated_block(const uint8_t terminator);
	uint8_t get_char();
	uint16_t get_usint();
	uint32_t get_uint();
	uint64_t get_ullint();
	uint16_t get_swapped_usint();
	uint32_t get_swapped_uint();
	uint64_t get_swapped_ullint();
	float get_float();
	void seek_read(size_t offset);
	streampos get_current_read_offset();
	
	// file output:
	void write_file(const string& str);
	void write_block(const char* data, size_t size, bool check_size = false);
	void write_block(const void* data, size_t size, bool check_size = false);
	template <typename T> void write(const T& data) {
		write_block((const void*)&data, sizeof(T));
	}
	void write_terminated_block(const string& str, const uint8_t terminator);
	void write_char(const uint8_t& ch);
	void write_usint(const uint16_t& usi);
	void write_uint(const uint32_t& ui);
	void write_ullint(const uint64_t& ulli);
	void write_float(const float& f);
	void seek_write(size_t offset);
	streampos get_current_write_offset();

	//
	static bool is_file(const string& filename);
	static bool is_directory(const string& dirname);
	bool eof() const;
	bool good() const;
	bool fail() const;
	bool bad() const;
	bool is_open() const;
	
	//! reads all data as binary from "filename" and returns it as a vector of the specified "data_type"
	template <typename data_type>
	static optional<vector<data_type>> file_to_vector(const string& filename) {
		file_io file(filename, file_io::OPEN_TYPE::READ_BINARY);
		if (!file.is_open()) {
			return {};
		}
		
		const size_t size = (size_t)file.get_filesize();
		const size_t readable_count = size / sizeof(data_type); // drop last bytes if they don't fit
		const size_t readable_size = readable_count * sizeof(data_type);
		vector<data_type> ret(readable_count);
		if (ret.size() != readable_count) {
			return {};
		}
		
		file.get_filestream()->read((char*)&ret[0], (streamsize)readable_size);
		const auto read_size = file.get_filestream()->gcount();
		if (read_size != (decltype(read_size))readable_size) {
			log_error("expected %u bytes, but only read %u bytes", readable_size, read_size);
			return {};
		}
		file.close();
		
		return ret;
	}

protected:
	OPEN_TYPE open_type { OPEN_TYPE::READ_BINARY };
	string filename { "" };
	fstream filestream;

	bool check_open();

};

#endif
