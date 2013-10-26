/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#include "logger.hpp"
#include "threading/thread_base.hpp"
#include "core/cpp_headers.hpp"

#if defined(__APPLE__) || defined(WIN_UNIXENV)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

static string log_filename = "log.txt", msg_filename = "msg.txt";
static unique_ptr<ofstream> log_file { nullptr }, msg_file { nullptr };
static atomic<unsigned int> log_err_counter { 0u };
static logger::LOG_TYPE log_verbosity { logger::LOG_TYPE::UNDECORATED };
static bool log_append_mode { false };
static vector<pair<logger::LOG_TYPE, string>> log_store, log_output_store;
static mutex log_store_lock;

class logger_thread : thread_base {
public:
	logger_thread() : thread_base("logger") {
		this->set_thread_delay(20); // lower to 20ms
		this->start();
	}
	virtual ~logger_thread() {
		// finish (kill the logger thread) and run once more to make sure everything has been saved/printed
		finish();
		run();
	}
	
	virtual void run();
};
static unique_ptr<logger_thread> log_thread;

void logger_thread::run() {
	// swap the (empty) log output store/queue with the (probably non-empty) log output store
	// note that this is a constant complexity operation, which makes log writing+output almost non-interrupting
	while(!log_store_lock.try_lock()) {
		this_thread::yield();
	}
	log_output_store.swap(log_store);
	log_store_lock.unlock();
	
	if(log_output_store.empty()) return;
	
	// in append mode, close the file and reopen it in append mode
	if(log_append_mode) {
		if(log_file->is_open()) {
			log_file->close();
			log_file->clear();
		}
		log_file->open(log_filename, ofstream::app | ofstream::out);
		
		if(msg_file != nullptr) {
			if(msg_file->is_open()) {
				msg_file->close();
				msg_file->clear();
			}
			msg_file->open(msg_filename, ofstream::app | ofstream::out);
		}
	}
	
	// write all log store entries
	for(auto& entry : log_output_store) {
		// TODO: color config setting + timestamp setting?
		
		// finally: output
		cout << entry.second;
		if(entry.second[0] == 0x1B) {
			// strip the color information when writing to the log file
			entry.second.erase(0, 5);
			entry.second.erase(7, 3);
		}
		
		// if "separate msg file logging" is enabled and the log type is "msg", log to the msg file
		if(entry.first == logger::LOG_TYPE::SIMPLE_MSG && msg_file != nullptr) {
			*msg_file << entry.second;
		}
		// else: just output to the standard log file
		else *log_file << entry.second;
	}
	cout.flush();
	log_file->flush();
	if(msg_file != nullptr) {
		msg_file->flush();
	}
	
	// in append mode, always close the file after writing to it
	if(log_append_mode) {
		log_file->close();
		log_file->clear();
		if(msg_file != nullptr) {
			msg_file->close();
			msg_file->clear();
		}
	}
	
	// now that everything has been written, clear the output store
	log_output_store.clear();
}

void logger::init(const size_t verbosity, const bool separate_msg_file, const bool append_mode,
				  const string log_filename_, const string msg_filename_) {
	log_filename = log_filename_;
	log_file = make_unique<ofstream>(log_filename);
	if(!log_file->is_open()) {
		cout << "LOG ERROR: couldn't open log file!" << endl;
	}
	
	if(separate_msg_file && verbosity >= (size_t)logger::LOG_TYPE::SIMPLE_MSG) {
		msg_filename = msg_filename_;
		msg_file = make_unique<ofstream>(msg_filename);
		if(!msg_file->is_open()) {
			cout << "LOG ERROR: couldn't open msg log file!" << endl;
		}
	}
	
	log_verbosity = (logger::LOG_TYPE)verbosity;
	log_append_mode = append_mode;
	
	// create+start the logger thread
	log_thread = make_unique<logger_thread>();
}

void logger::destroy() {
	log_thread.reset(nullptr);
	if(log_file != nullptr) {
		log_file->close();
		log_file.reset(nullptr);
	}
	if(msg_file != nullptr) {
		msg_file->close();
		msg_file.reset(nullptr);
	}
}

bool logger::prepare_log(stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func) {
	// check verbosity level and leave or continue accordingly
	if(log_verbosity < type) {
		return false;
	}
	
	if(type != logger::LOG_TYPE::UNDECORATED) {
		switch(type) {
			case LOG_TYPE::ERROR_MSG:
				buffer << "\033[31m[ERROR]\033[m";
				buffer << " #" << log_err_counter++ << ":";
				break;
			case LOG_TYPE::DEBUG_MSG:
				buffer << "\033[32m[DEBUG]\033[m";
				break;
			case LOG_TYPE::SIMPLE_MSG:
				buffer << "\033[34m[ MSG ]\033[m";
				break;
			case LOG_TYPE::UNDECORATED: break;
		}
		buffer << " ";
		// prettify file string (aka strip path)
		string file_str = file;
		size_t slash_pos = string::npos;
		if((slash_pos = file_str.rfind("/")) == string::npos) slash_pos = file_str.rfind("\\");
		file_str = (slash_pos != string::npos ? file_str.substr(slash_pos+1, file_str.size()-slash_pos-1) : file_str);
		buffer << file_str;
		buffer << ": " << func << "(): ";
	}
	return true;
}

void logger::log_internal(stringstream& buffer, const LOG_TYPE& type, const char* str) {
	// this is the final log function
	while(*str) {
		if(*str == '%' && *(++str) != '%') {
			cout << "LOG ERROR: invalid log format, missing arguments!" << endl;
			cout.flush();
		}
		buffer << *str++;
	}
	buffer << endl;
	
	// add string to log store/queue
	while(!log_store_lock.try_lock()) {
		this_thread::yield();
	}
	log_store.emplace_back(type, buffer.str());
	log_store_lock.unlock();
}
