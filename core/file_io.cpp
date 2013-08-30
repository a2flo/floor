/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#include "file_io.h"

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
		oclr_error("a file is already opened! can't open another file!");
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
		oclr_error("error while loading file %s!", filename);
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
	file_io file(filename, file_io::OPEN_TYPE::READ);
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
	file_io file(filename, file_io::OPEN_TYPE::READ);
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
void file_io::get_line(char* finput, unsigned int length) {
	filestream.getline(finput, length);
}

/*! reads a block of -size- bytes
 *  @param data a pointer to a char where the block is written to
 *  @param size the amount of bytes we want to get
 */
void file_io::get_block(char* data, size_t size) {
	filestream.read(data, size);
}

/*! reads a single char from the current file input stream and returns it
 */
char file_io::get_char() {
	char c = '\0'; // must be initialized, since fstream get might fail!
	filestream.get(c);
	return c;
}

/*! reads a single unsigned short int from the current file input stream and returns it
 */
unsigned short int file_io::get_usint() {
	unsigned char tmp[2] { 0, 0 };
	unsigned short int us = 0;
	filestream.read((char*)tmp, 2);
	us = (tmp[0] << 8) + tmp[1];
	return us;
}

/*! reads a single unsigned int from the current file input stream and returns it
 */
unsigned int file_io::get_uint() {
	unsigned char tmp[4] { 0, 0, 0, 0 };
	unsigned int u = 0;
	filestream.read((char*)tmp, 4);
	u = (tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + tmp[3];
	return u;
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
uint64_t file_io::get_filesize() {
	// get current get pointer position
	streampos cur_position = filestream.tellg();

	// get the file size
	filestream.seekg(0, ios::end);
	uint64_t size = filestream.tellg();
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
	filestream.seekg(offset, ios::beg);
}

void file_io::seek_write(size_t offset) {
	filestream.seekp(offset, ios::beg);
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

void file_io::write_file(string& str) {
	filestream.write(&str.front(), str.size());
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
		filestream.write(data, size);
	}
}

void file_io::write_char(const unsigned char& ch) {
	filestream.write((char*)&ch, 1);
}

void file_io::write_uint(const unsigned int& ui) {
	const char chui[4] = {
		(char)((ui & 0xFF000000) >> 24),
		(char)((ui & 0xFF0000) >> 16),
		(char)((ui & 0xFF00) >> 8),
		(char)(ui & 0xFF)
	};
	filestream.write(chui, 4);
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

void file_io::get_terminated_block(string& str, const char terminator) {
	for(char c = get_char(); c != terminator; c = get_char()) {
		str += c;
	}
}

void file_io::write_terminated_block(const string& str, const char terminator) {
	write_block(str.c_str(), (unsigned int)str.length());
	filestream.put(terminator);
}

fstream* file_io::get_filestream() {
	return &(filestream);
}

bool file_io::read_file(stringstream& buffer) {
	const size_t size = (size_t)get_filesize();
	char* data = new char[size+1];
	if(data == nullptr) return false;
	memset(data, 0, size+1);
	filestream.read(data, size);
	filestream.seekg(0, ios::beg);
	filestream.seekp(0, ios::beg);
	filestream.clear();
	buffer.write(data, size);
	delete [] data;
	return true;
}

bool file_io::read_file(string& str) {
	const size_t size = (size_t)get_filesize();
	str.resize(size);
	if(str.size() != size) return false;
	filestream.read(&str.front(), size);
	filestream.seekg(0, ios::beg);
	filestream.seekp(0, ios::beg);
	filestream.clear();
	return true;
}

bool file_io::string_to_file(const string& filename, string& str) {
	file_io file(filename, file_io::OPEN_TYPE::WRITE);
	if(!file.is_open()) {
		return false;
	}
	file.write_file(str);
	file.close();
	return true;
}
