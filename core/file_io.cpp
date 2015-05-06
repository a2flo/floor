/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/core/file_io.hpp>

/*! there is no function currently
 */
file_io::file_io() {
}

file_io::file_io(const string& filename, const OPEN_TYPE open_type_) {
	open(filename, open_type_);
}

/*! there is no function currently
 */
file_io::~file_io() {
	if(filestream.is_open()) {
		close();
	}
}

/*! opens a input file stream
 *  @param filename the name of the file
 *  @param open_type enum that specifies how we want to open the file (like "r", "wb", etc. ...)
 */
bool file_io::open(const string& filename, OPEN_TYPE open_type_) {
	if(check_open()) {
		log_error("a file is already opened! can't open another file!");
		return false;
	}

	open_type = open_type_;
	switch(open_type) {
		case file_io::OPEN_TYPE::READ:
			filestream.open(filename, fstream::in);
			break;
		case file_io::OPEN_TYPE::READWRITE:
			filestream.open(filename, fstream::in | fstream::out);
			break;
		case file_io::OPEN_TYPE::WRITE:
			filestream.open(filename, fstream::out);
			break;
		case file_io::OPEN_TYPE::READ_BINARY:
			filestream.open(filename, fstream::in | fstream::binary);
			break;
		case file_io::OPEN_TYPE::READWRITE_BINARY:
			filestream.open(filename, fstream::in | fstream::out | fstream::binary);
			break;
		case file_io::OPEN_TYPE::WRITE_BINARY:
			filestream.open(filename, fstream::out | fstream::binary);
			break;
		case file_io::OPEN_TYPE::APPEND:
			filestream.open(filename, fstream::app);
			break;
		case file_io::OPEN_TYPE::APPEND_BINARY:
			filestream.open(filename, fstream::app | fstream::binary);
			break;
		case file_io::OPEN_TYPE::APPEND_READ:
			filestream.open(filename, fstream::in | fstream::app);
			break;
		case file_io::OPEN_TYPE::APPEND_READ_BINARY:
			filestream.open(filename, fstream::in | fstream::app | fstream::binary);
			break;
	}

	if(!filestream.is_open()) {
		log_error("error while loading file %s!", filename);
		filestream.clear();
		return false;
	}
	return true;
}

/*! closes the input file stream
 */
void file_io::close() {
	filestream.close();
	filestream.clear();
}

bool file_io::file_to_buffer(const string& filename, stringstream& buffer) {
	file_io file(filename, file_io::OPEN_TYPE::READ_BINARY);
	if(!file.is_open()) {
		return false;
	}
	
	buffer.seekp(0);
	buffer.seekg(0);
	buffer.clear();
	buffer.str("");
	if(!file.read_file(buffer)) return false;
	file.close();
	return true;
}

bool file_io::file_to_string(const string& filename, string& str) {
	file_io file(filename, file_io::OPEN_TYPE::READ_BINARY);
	if(!file.is_open()) {
		return false;
	}
	
	str.clear();
	if(!file.read_file(str)) return false;
	file.close();
	return true;
}

string file_io::file_to_string(const string& filename) {
	string ret = "";
	file_to_string(filename, ret);
	return ret;
}

/*! reads a line from the current input stream (senseless if we have a binary file)
 *  @param finput a pointer to a char where the line is written to
 */
void file_io::get_line(char* finput, streamsize length) {
	filestream.getline(finput, length);
}

/*! reads a block of -size- bytes
 *  @param data a pointer to a char where the block is written to
 *  @param size the amount of bytes we want to get
 */
void file_io::get_block(char* data, streamsize size) {
	filestream.read(data, size);
}

/*! reads a single char from the current file input stream and returns it
 */
uint8_t file_io::get_char() {
	uint8_t c = '\0'; // must be initialized, since fstream get might fail!
	filestream.get((char&)c);
	return c;
}

/*! reads a single uint16_t from the current file input stream and returns it
 */
uint16_t file_io::get_usint() {
	uint8_t tmp[2] { 0, 0 };
	filestream.read((char*)tmp, 2);
	return (uint16_t)(tmp[0] << 8u) + (uint16_t)tmp[1];
}

uint16_t file_io::get_swapped_usint() {
	uint8_t tmp[2] { 0, 0 };
	filestream.read((char*)tmp, 2);
	return (uint16_t)(tmp[1] << 8u) + (uint16_t)tmp[0];
}

/*! reads a single uint32_t from the current file input stream and returns it
 */
uint32_t file_io::get_uint() {
	uint8_t tmp[4] { 0, 0, 0, 0 };
	filestream.read((char*)tmp, 4);
	return (((uint32_t)tmp[0] << 24u) +
			((uint32_t)tmp[1] << 16u) +
			((uint32_t)tmp[2] << 8u) +
			((uint32_t)tmp[3]));
}

uint32_t file_io::get_swapped_uint() {
	uint8_t tmp[4] { 0, 0, 0, 0 };
	filestream.read((char*)tmp, 4);
	return (((uint32_t)tmp[3] << 24u) +
			((uint32_t)tmp[2] << 16u) +
			((uint32_t)tmp[1] << 8u) +
			((uint32_t)tmp[0]));
}

/*! reads a single uint64_t from the current file input stream and returns it
 */
uint64_t file_io::get_ullint() {
	uint8_t tmp[8] { 0, 0, 0, 0, 0, 0, 0, 0 };
	filestream.read((char*)tmp, 8);
	return (((uint64_t)tmp[0] << 56ull) +
			((uint64_t)tmp[1] << 48ull) +
			((uint64_t)tmp[2] << 40ull) +
			((uint64_t)tmp[3] << 32ull) +
			((uint64_t)tmp[4] << 24ull) +
			((uint64_t)tmp[5] << 16ull) +
			((uint64_t)tmp[6] << 8ull) +
			((uint64_t)tmp[7]));
}

uint64_t file_io::get_swapped_ullint() {
	uint8_t tmp[8] { 0, 0, 0, 0, 0, 0, 0, 0 };
	filestream.read((char*)tmp, 8);
	return (((uint64_t)tmp[7] << 56ull) +
			((uint64_t)tmp[6] << 48ull) +
			((uint64_t)tmp[5] << 40ull) +
			((uint64_t)tmp[4] << 32ull) +
			((uint64_t)tmp[3] << 24ull) +
			((uint64_t)tmp[2] << 16ull) +
			((uint64_t)tmp[1] << 8ull) +
			((uint64_t)tmp[0]));
}

/*! reads a single float from the current file input stream and returns it
 */
float file_io::get_float() {
	float f = 0.0f;
	filestream.read((char*)&f, 4);
	return f;
}

/*! returns the filesize
 */
long long int file_io::get_filesize() {
	// get current get pointer position
	streampos cur_position = filestream.tellg();

	// get the file size
	filestream.seekg(0, ios::end);
	long long int size = filestream.tellg();
	filestream.seekg(0, ios::beg);

	// reset get pointer position
	filestream.seekg(cur_position, ios::beg);

	// return file size
	return size;
}

/*! seeks to offset
 *  @param offset the offset we you want to seek the file
 */
void file_io::seek(size_t offset) {
	if(open_type != OPEN_TYPE::WRITE && open_type != OPEN_TYPE::WRITE_BINARY &&
	   open_type != OPEN_TYPE::APPEND && open_type != OPEN_TYPE::APPEND_BINARY) {
		seek_read(offset);
	}
	else seek_write(offset);
}

void file_io::seek_read(size_t offset) {
	filestream.seekg((long long int)offset, ios::beg);
}

void file_io::seek_write(size_t offset) {
	filestream.seekp((long long int)offset, ios::beg);
}

/*! returns the current file offset
 */
streampos file_io::get_current_offset() {
	if(open_type != OPEN_TYPE::WRITE && open_type != OPEN_TYPE::WRITE_BINARY &&
	   open_type != OPEN_TYPE::APPEND && open_type != OPEN_TYPE::APPEND_BINARY) {
		return get_current_read_offset();
	}
	return get_current_write_offset();
}

streampos file_io::get_current_read_offset() {
	return filestream.tellg();
}

streampos file_io::get_current_write_offset() {
	return filestream.tellp();
}

void file_io::write_file(const string& str) {
	filestream.write(str.data(), (streamsize)str.size());
}

/*! writes a block to the current file (offset)
 *  @param data a pointer to the block we want to write
 *  @param size the size of the block
 */
void file_io::write_block(const char* data, size_t size, bool check_size) {
	if(check_size) {
		filestream.write(data, (streamsize)strlen(data));
		if(strlen(data) < size) {
			size_t x = size - strlen(data);
			for(size_t i = 0; i < x; i++) {
				filestream.put(0x00);
			}
		}
	}
	else {
		filestream.write(data, (streamsize)size);
	}
}

void file_io::write_block(const void* data, size_t size, bool check_size) {
	write_block((const char*)data, size, check_size);
}

void file_io::write_char(const uint8_t& ch) {
	filestream.put((char&)ch);
}

void file_io::write_usint(const uint16_t& usi) {
	const char chui[2] {
		(char)((usi & 0xFF00u) >> 8u),
		(char)(usi & 0xFFu)
	};
	filestream.write(chui, 2);
}

void file_io::write_uint(const uint32_t& ui) {
	const char chui[4] {
		(char)((ui & 0xFF000000u) >> 24u),
		(char)((ui & 0xFF0000u) >> 16u),
		(char)((ui & 0xFF00u) >> 8u),
		(char)(ui & 0xFFu)
	};
	filestream.write(chui, 4);
}

void file_io::write_ullint(const uint64_t& ulli) {
	const char chui[8] {
		(char)((ulli & 0xFF00000000000000ull) >> 56ull),
		(char)((ulli & 0xFF000000000000ull) >> 48ull),
		(char)((ulli & 0xFF0000000000ull) >> 40ull),
		(char)((ulli & 0xFF00000000ull) >> 32ull),
		(char)((ulli & 0xFF000000ull) >> 24ull),
		(char)((ulli & 0xFF0000ull) >> 16ull),
		(char)((ulli & 0xFF00ull) >> 8ull),
		(char)(ulli & 0xFFull)
	};
	filestream.write(chui, 8);
}

void file_io::write_float(const float& f) {
	filestream.write((char*)&f, 4);
}

/*! returns true if the file specified by filename exists
 *  @param filename the name of the file
 */
bool file_io::is_file(const string& filename) {
	if(filename.empty() || filename[filename.length()-1] == '/') return false;
	
	fstream file(filename, fstream::in | fstream::binary);
	if(!file.is_open()) {
		return false;
	}
	file.close();
	return true;
}

bool file_io::is_directory(const string& dirname) {
	if(dirname.empty()) return false;
	
#if !defined(_MSC_VER)
	const auto dir = opendir(dirname.c_str());
	if(dir != nullptr) {
		closedir(dir);
		return true;
	}
	return false;
#else
	const auto attr = GetFileAttributesA(dirname.c_str());
	if(attr == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#endif
}

/*! checks if a file is already opened - if so, return true, otherwise false
 */
bool file_io::check_open() {
	if(filestream.is_open()) {
		filestream.clear();
		return true;
	}
	return false;
}

/*! checks if we reached the end of file
 */
bool file_io::eof() const {
	return filestream.eof();
}

bool file_io::good() const {
	return filestream.good();
}

bool file_io::fail() const {
	return filestream.fail();
}

bool file_io::bad() const {
	return filestream.bad();
}

bool file_io::is_open() const {
	return filestream.is_open();
}

void file_io::get_terminated_block(string& str, const uint8_t terminator) {
	for(uint8_t c = get_char(); c != terminator; c = get_char()) {
		str += (char&)c;
	}
}

string file_io::get_terminated_block(const uint8_t terminator) {
	string str = "";
	for(uint8_t c = get_char(); c != terminator; c = get_char()) {
		str += (char&)c;
	}
	return str;
}

void file_io::write_terminated_block(const string& str, const uint8_t terminator) {
	write_block(str.c_str(), (unsigned int)str.length());
	filestream.put((char&)terminator);
}

fstream* file_io::get_filestream() {
	return &(filestream);
}

bool file_io::read_file(stringstream& buffer) {
	auto size_ll = get_filesize();
	if(size_ll < 0) return false;
	
#if defined(PLATFORM_X32)
	if(size_ll > 0x7FFFFFFF) {
		log_error("file size %u is too large for this platform - will only read 2GB now!", size_ll);
		size_ll = 0x7FFFFFFF;
	}
#endif
	
	const unsigned long long int size = (unsigned long long int)size_ll;
	auto data = make_unique<char[]>(size + 1u);
	if(data == nullptr) return false;
	
	memset(data.get(), 0, size_t(size + 1u));
	filestream.read(data.get(), streamsize(size_ll));
	const auto read_size = filestream.gcount();
	if(read_size != (decltype(read_size))size_ll) {
		log_error("expected %u bytes, but only read %u bytes", size_ll, read_size);
		return false;
	}
	filestream.seekg(0, ios::beg);
	filestream.seekp(0, ios::beg);
	filestream.clear();
	buffer.write(data.get(), streamsize(size_ll));
	return true;
}

bool file_io::read_file(string& str) {
	const size_t size = (size_t)get_filesize();
	str.resize(size);
	if(str.size() != size) return false;
	filestream.read(&str.front(), (streamsize)size);
	const auto read_size = filestream.gcount();
	if(read_size != (decltype(read_size))size) {
		log_error("expected %u bytes, but only read %u bytes", size, read_size);
		return false;
	}
	filestream.seekg(0, ios::beg);
	filestream.seekp(0, ios::beg);
	filestream.clear();
	return true;
}

bool file_io::string_to_file(const string& filename, const string& str) {
	file_io file(filename, file_io::OPEN_TYPE::WRITE_BINARY);
	if(!file.is_open()) {
		return false;
	}
	file.write_file(str);
	file.close();
	return true;
}

bool file_io::buffer_to_file(const string& filename, const char* buffer, const size_t& size) {
	file_io file(filename, file_io::OPEN_TYPE::WRITE_BINARY);
	if(!file.is_open()) {
		return false;
	}
	file.write_block(buffer, size);
	file.close();
	return true;
}
