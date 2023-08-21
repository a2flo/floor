/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif
#include <floor/core/logger.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/core/cpp_headers.hpp>
#include <floor/constexpr/const_math.hpp>

#include <SDL2/SDL.h>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

//! enable this to print the thread id in each log message
#define FLOOR_LOG_THREAD_ID 0

static string log_filename, msg_filename;
static unique_ptr<ofstream> log_file { nullptr }, msg_file { nullptr };
static atomic<uint32_t> log_err_counter { 0u };
static atomic<logger::LOG_TYPE> log_verbosity { logger::LOG_TYPE::UNDECORATED };
static bool log_append_mode { false };
static safe_mutex log_store_lock;
static vector<pair<logger::LOG_TYPE, string>> log_store GUARDED_BY(log_store_lock), log_output_store;
static bool log_use_time { true };
static bool log_use_color { true };
static bool log_use_unicode_color { false };
static atomic<bool> log_initialized { false };
static atomic<bool> log_destroying { false };

class logger_thread final : thread_base {
public:
	atomic<uint32_t> run_num { 0 };
	
	logger_thread() : thread_base("logger") {
		this->set_thread_delay(20); // lower to 20ms
		this->start();
	}
	~logger_thread() override {
		// finish (kill the logger thread) and run once more to make sure everything has been saved/printed
		finish();
		run();
		
		if(log_file != nullptr) {
			log_file->close();
			log_file.reset();
		}
		if(msg_file != nullptr) {
			msg_file->close();
			msg_file.reset();
		}
	}
	
	void run() override REQUIRES(!log_store_lock);
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
	
	if(log_output_store.empty()) {
		++run_num;
		return;
	}
	
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
		// finally: output
		if(entry.first != logger::LOG_TYPE::ERROR_MSG) {
			cout << entry.second;
		}
		else cerr << entry.second;
		
		if(entry.second[0] == 0x1B) {
			// strip the color information when writing to the log file
			entry.second.erase(0, 5);
			entry.second.erase(5, 3);
		}
		
		// if "separate msg file logging" is enabled and the log type is "msg", log to the msg file
		if(entry.first == logger::LOG_TYPE::SIMPLE_MSG && msg_file != nullptr) {
			*msg_file << entry.second;
		}
		// else: just output to the standard log file
		else *log_file << entry.second;
	}
	cout.flush();
	cerr.flush();
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
	
	// update #runs counter
	++run_num;
}

void logger::init(const size_t verbosity,
				  const bool separate_msg_file,
				  const bool append_mode,
				  const bool use_time,
				  const bool use_color,
				  const string& log_filename_,
				  const string& msg_filename_) {
	// only allow single init
	if(log_initialized.exchange(true)) {
		return;
	}
	
	// always call destroy on program exit
	atexit([] { logger::destroy(); });
	
	// if either is empty, use the default log/msg file name, with special treatment on iOS
	if(log_filename_.empty() || msg_filename_.empty()) {
#if defined(FLOOR_IOS)
		// we can/must store the log in a writable path (-> pref path)
		const auto pref_path = darwin_helper::get_pref_path();
		if(pref_path != "") {
			log_filename = (log_filename_.empty() ? pref_path + "log.txt"s : log_filename_);
			msg_filename = (msg_filename_.empty() ? pref_path + "msg.txt"s : msg_filename_);
		}
		
		// check if still empty and just try the default
		if(log_filename.empty() && log_filename_.empty()) log_filename = "log.txt";
		if(msg_filename.empty() && msg_filename_.empty()) msg_filename = "msg.txt";
#else
		log_filename = (log_filename_.empty() ? "log.txt" : log_filename_);
		msg_filename = (msg_filename_.empty() ? "msg.txt" : msg_filename_);
#endif
	}
	else {
		// neither is empty, use the specified paths/names
		log_filename = log_filename_;
		msg_filename = msg_filename_;
	}
	
	log_file = make_unique<ofstream>(log_filename, (append_mode ? ofstream::app | ofstream::out : ofstream::out));
	if(!log_file->is_open()) {
		cerr << "LOG ERROR: couldn't open log file (" << log_filename << ")!" << endl;
	}
	
	if(separate_msg_file && verbosity >= (size_t)logger::LOG_TYPE::SIMPLE_MSG) {
		msg_file = make_unique<ofstream>(msg_filename, (append_mode ? ofstream::app | ofstream::out : ofstream::out));
		if(!msg_file->is_open()) {
			cerr << "LOG ERROR: couldn't open msg log file (" << msg_filename << ")!" << endl;
		}
	}
	
	log_verbosity = (logger::LOG_TYPE)verbosity;
	log_append_mode = append_mode;
	log_use_time = use_time;
	log_use_color = use_color;
#if defined(__WINDOWS__)
	if (log_use_color) {
		// disable color in Windows cmd/powershell
		const auto session_name_cstr = SDL_getenv("SESSIONNAME");
		const auto term_cstr = SDL_getenv("TERM");
		const string session_name = (session_name_cstr ? session_name_cstr : "");
		const string term = (term_cstr ? term_cstr : "");
		log_use_color = !(session_name == "Console" && term.empty());
	}
#endif
#if defined(__APPLE__)
	log_use_unicode_color = darwin_helper::is_running_in_debugger();
#endif
	
	// create+start the logger thread
	log_thread = make_unique<logger_thread>();
}

void logger::destroy() {
	if(!log_initialized) return;
	
	// only allow single destroy
	if(log_destroying.exchange(true)) {
		return;
	}
	
	log_msg("killing logger ...");
	log_thread = nullptr;
	
	log_initialized = false;
	log_destroying = false;
}

void logger::flush() {
	if(!log_thread) return;
	const uint32_t cur_run = log_thread->run_num;
	while(cur_run == log_thread->run_num) {
		this_thread::yield();
	}
}

bool logger::prepare_log(stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func) {
	// check verbosity level and leave or continue accordingly
	if(log_verbosity < type) {
		return false;
	}
	
	if(type != logger::LOG_TYPE::UNDECORATED) {
		if (log_use_color && log_use_unicode_color) {
			switch (type) {
				case LOG_TYPE::ERROR_MSG:
					buffer << "[🔴]";
					break;
				case LOG_TYPE::WARNING_MSG:
					buffer << "[🟡]";
					break;
				case LOG_TYPE::DEBUG_MSG:
					buffer << "[🟢]";
					break;
				case LOG_TYPE::SIMPLE_MSG:
					buffer << "[🔵]";
					break;
				case LOG_TYPE::UNDECORATED: break;
			}
		} else {
			switch (type) {
				case LOG_TYPE::ERROR_MSG:
					buffer << (log_use_color ? "\033[31m" : "") << "[ERR]";
					break;
				case LOG_TYPE::WARNING_MSG:
					buffer << (log_use_color ? "\033[33m" : "") << "[WRN]";
					break;
				case LOG_TYPE::DEBUG_MSG:
					buffer << (log_use_color ? "\033[32m" : "") << "[DBG]";
					break;
				case LOG_TYPE::SIMPLE_MSG:
					buffer << (log_use_color ? "\033[34m" : "") << "[MSG]";
					break;
				case LOG_TYPE::UNDECORATED: break;
			}
			if (log_use_color && type != LOG_TYPE::UNDECORATED) {
				buffer << "\033[m";
			}
		}
		
		if(log_use_time) {
			buffer << "[";
			char time_str[64];
			const auto system_now = chrono::system_clock::now();
			const auto cur_time = chrono::system_clock::to_time_t(system_now);
#if !defined(_MSC_VER)
			struct tm* local_time = localtime(&cur_time);
			strftime(time_str, sizeof(time_str), "%H:%M:%S", local_time);
#else
			struct tm local_time;
			localtime_s(&local_time, &cur_time);
			strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
#endif
			buffer << time_str;
			buffer << ".";
			buffer << setw(const_math::int_width(chrono::system_clock::period::den));
			buffer << system_now.time_since_epoch().count() % chrono::system_clock::period::den << setw(0);
			buffer << "]";
		}
		
#if FLOOR_LOG_THREAD_ID
		buffer << "[" << this_thread::get_id() << "]";
#endif
		
		buffer << " ";
		
		if(type == LOG_TYPE::ERROR_MSG) {
			buffer << "#" << log_err_counter++ << ": ";
		}
		
		if(type != logger::LOG_TYPE::SIMPLE_MSG) {
			// prettify file string (aka strip path)
			string file_str = file;
			size_t slash_pos = string::npos;
			if((slash_pos = file_str.rfind('/')) == string::npos) slash_pos = file_str.rfind('\\');
			file_str = (slash_pos != string::npos ? file_str.substr(slash_pos+1, file_str.size()-slash_pos-1) : file_str);
			buffer << file_str;
			buffer << ": " << func << "(): ";
		}
	}
	return true;
}

void logger::log_internal(stringstream& buffer, const LOG_TYPE& type, const char* str) REQUIRES(!log_store_lock) {
	// this is the final log function
	if (str != nullptr) {
		while (*str) {
			buffer << *str++;
		}
	}
	buffer << endl;
	
	// add string to log store/queue
	while(!log_store_lock.try_lock()) {
		this_thread::yield();
	}
	log_store.emplace_back(type, buffer.str());
	log_store_lock.unlock();
}

void logger::set_verbosity(const LOG_TYPE& verbosity) {
	log_verbosity = verbosity;
}

logger::LOG_TYPE logger::get_verbosity() {
	return log_verbosity;
}

bool logger::is_initialized() {
	return log_initialized;
}
