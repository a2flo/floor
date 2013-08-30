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

#include "logger.h"
#include <thread>

#if defined(__APPLE__) || defined(WIN_UNIXENV)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#if !defined(OCLRASTER_IOS)
#define OCLRASTER_LOG_FILENAME "log.txt"
#else
#define OCLRASTER_LOG_FILENAME "/tmp/oclraster_log.txt"
#endif

static ofstream* log_file = new ofstream(OCLRASTER_LOG_FILENAME);
static atomic<unsigned int> err_counter { 0 };
static mutex output_lock;

void logger::init() {
	if(!log_file->is_open()) {
		cout << "LOG ERROR: couldn't open log file!" << endl;
	}
}

void logger::destroy() {
	log_file->close();
	delete log_file;
	log_file = nullptr;
}

void logger::prepare_log(stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func) {
	if(type != logger::LOG_TYPE::NONE) {
		switch(type) {
			case LOG_TYPE::ERROR_MSG:
				buffer << "\033[31m";
				break;
			case LOG_TYPE::DEBUG_MSG:
				buffer << "\033[32m";
				break;
			case LOG_TYPE::SIMPLE_MSG:
				buffer << "\033[34m";
				break;
			case LOG_TYPE::NONE: break;
		}
		buffer << logger::type_to_str(type);
		switch(type) {
			case LOG_TYPE::ERROR_MSG:
			case LOG_TYPE::DEBUG_MSG:
			case LOG_TYPE::SIMPLE_MSG:
				buffer << "\033[m";
				break;
			case LOG_TYPE::NONE: break;
		}
		if(type == LOG_TYPE::ERROR_MSG) buffer << " #" << err_counter++ << ":";
		buffer << " ";
		// prettify file string (aka strip path)
		string file_str = file;
		size_t slash_pos = string::npos;
		if((slash_pos = file_str.rfind("/")) == string::npos) slash_pos = file_str.rfind("\\");
		file_str = (slash_pos != string::npos ? file_str.substr(slash_pos+1, file_str.size()-slash_pos-1) : file_str);
		buffer << file_str;
		buffer << ": " << func << "(): ";
	}
}

void logger::_log(stringstream& buffer, const char* str) {
	// this is the final log function
	while(*str) {
		if(*str == '%' && *(++str) != '%') {
			cout << "LOG ERROR: invalid log format, missing arguments!" << endl;
			cout.flush();
		}
		buffer << *str++;
	}
	buffer << endl;
	
	// finally: output
	while(!output_lock.try_lock()) {
		this_thread::yield();
	}
	string bstr(buffer.str());
	cout << bstr;
	cout.flush();
	if(bstr[0] != 0x1B) {
		*log_file << bstr;
	}
	else {
		bstr.erase(0, 5);
		bstr.erase(7, 3);
		*log_file << bstr;
	}
	log_file->flush();
	output_lock.unlock();
}

