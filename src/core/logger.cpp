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

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif
#include <fstream>
#include <chrono>
#include <condition_variable>
#include <floor/core/logger.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/constexpr/const_math.hpp>

#include <SDL3/SDL.h>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

//! enable this to print the thread id in each log message
#define FLOOR_LOG_THREAD_ID 0

namespace fl {
using namespace std::literals;

using log_entry_t = std::pair<logger::LOG_TYPE, std::string>;
static struct logger_state_t {
	std::string log_filename;
	std::string msg_filename;
	std::unique_ptr<std::ofstream> log_file { nullptr };
	std::unique_ptr<std::ofstream> msg_file { nullptr };
	
	std::atomic<uint32_t> err_counter { 0u };
	
	atomic_spin_lock store_lock;
	std::vector<log_entry_t> store GUARDED_BY(store_lock);
	std::vector<log_entry_t> output_store;
	
	std::atomic<logger::LOG_TYPE> verbosity { logger::LOG_TYPE::UNDECORATED };
	bool append_mode { false };
	bool use_time { true };
	bool use_color { true };
	bool use_unicode_color { false };
	bool synchronous { false };
	
	std::mutex run_cv_lock;
	std::condition_variable run_cv;
	std::atomic<bool> initialized { false };
	std::atomic<bool> destroying { false };
} logger_state {};

class logger_thread final : thread_base {
public:
	std::atomic<uint32_t> run_num { 0 };
	
	logger_thread() : thread_base("logger") {
		this->set_thread_delay(0); // never sleep in thread_base
		this->set_yield_after_run(false); // never yield
		this->start();
	}
	~logger_thread() override {
		// finish (kill the logger thread) and run once more to make sure everything has been saved/printed
		set_thread_should_finish();
		logger_state.run_cv.notify_all();
		finish();
		run();
		
		if (logger_state.log_file) {
			logger_state.log_file->close();
			logger_state.log_file.reset();
		}
		if (logger_state.msg_file) {
			logger_state.msg_file->close();
			logger_state.msg_file.reset();
		}
	}
	
	void run() override REQUIRES(!logger_state.store_lock);
};
static std::unique_ptr<logger_thread> log_thread;

static inline void write_log_entry(log_entry_t&& entry) {
	if (entry.first != logger::LOG_TYPE::ERROR_MSG) {
		std::cout << entry.second;
	} else {
		std::cerr << entry.second;
	}
	
	if (entry.second[0] == 0x1B) {
		// strip the color information when writing to the log file
		entry.second.erase(0, 5);
		entry.second.erase(5, 3);
	}
	
	// if "separate msg file logging" is enabled and the log type is "msg", log to the msg file
	if (entry.first == logger::LOG_TYPE::SIMPLE_MSG && logger_state.msg_file) {
		*logger_state.msg_file << entry.second;
	}
	// else: just output to the standard log file
	else {
		*logger_state.log_file << entry.second;
	}
}

void logger_thread::run() {
	{
		// wait until work is triggered (notified) or timeout after 500ms
		std::unique_lock<std::mutex> run_lock_guard(logger_state.run_cv_lock);
		logger_state.run_cv.wait_for(run_lock_guard, 500ms);
	}
	
	// swap the (empty) log output store/queue with the (probably non-empty) log output store
	// note that this is a constant complexity operation, which makes log writing+output almost non-interrupting
	{
		GUARD(logger_state.store_lock);
		logger_state.output_store.swap(logger_state.store);
	}
	
	if (logger_state.output_store.empty()) {
		++run_num;
		return;
	}
	
	// in append mode, close the file and reopen it in append mode
	if (logger_state.append_mode) {
		if (logger_state.log_file->is_open()) {
			logger_state.log_file->close();
			logger_state.log_file->clear();
		}
		logger_state.log_file->open(logger_state.log_filename, std::ofstream::app | std::ofstream::out);
		
		if (logger_state.msg_file) {
			if (logger_state.msg_file->is_open()) {
				logger_state.msg_file->close();
				logger_state.msg_file->clear();
			}
			logger_state.msg_file->open(logger_state.msg_filename, std::ofstream::app | std::ofstream::out);
		}
	}
	
	// write all log store entries
	for (auto& entry : logger_state.output_store) {
		// finally: output
		write_log_entry(std::move(entry));
	}
	std::cout.flush();
	std::cerr.flush();
	logger_state.log_file->flush();
	if (logger_state.msg_file) {
		logger_state.msg_file->flush();
	}
	
	// in append mode, always close the file after writing to it
	if (logger_state.append_mode) {
		logger_state.log_file->close();
		logger_state.log_file->clear();
		if (logger_state.msg_file) {
			logger_state.msg_file->close();
			logger_state.msg_file->clear();
		}
	}
	
	// now that everything has been written, clear the output store
	logger_state.output_store.clear();
	
	// update #runs counter
	++run_num;
}

void logger::init(const size_t verbosity,
				  const bool separate_msg_file,
				  const bool append_mode,
				  const bool use_time,
				  const bool use_color,
				  const bool synchronous_logging,
				  const std::string& log_filename_,
				  const std::string& msg_filename_) {
	// only allow single init
	if (logger_state.initialized.exchange(true)) {
		return;
	}
	
	// always call destroy on program exit
	atexit([] { logger::destroy(); });
	
	// if either is empty, use the default log/msg file name, with special treatment on iOS
	if (log_filename_.empty() || msg_filename_.empty()) {
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		// we can/must store the log in a writable path (-> pref path)
		const auto pref_path = darwin_helper::get_pref_path();
		if (pref_path != "") {
			logger_state.log_filename = (log_filename_.empty() ? pref_path + "log.txt"s : log_filename_);
			logger_state.msg_filename = (msg_filename_.empty() ? pref_path + "msg.txt"s : msg_filename_);
		}
		
		// check if still empty and just try the default
		if (logger_state.log_filename.empty() && log_filename_.empty()) {
			logger_state.log_filename = "log.txt";
		}
		if (logger_state.msg_filename.empty() && msg_filename_.empty()) {
			logger_state.msg_filename = "msg.txt";
		}
#else
		logger_state.log_filename = (log_filename_.empty() ? "log.txt" : log_filename_);
		logger_state.msg_filename = (msg_filename_.empty() ? "msg.txt" : msg_filename_);
#endif
	} else {
		// neither is empty, use the specified paths/names
		logger_state.log_filename = log_filename_;
		logger_state.msg_filename = msg_filename_;
	}
	
	logger_state.log_file = std::make_unique<std::ofstream>(logger_state.log_filename, (append_mode ? std::ofstream::app | std::ofstream::out : std::ofstream::out));
	if (!logger_state.log_file->is_open()) {
		std::cerr << "LOG ERROR: couldn't open log file (" << logger_state.log_filename << ")!" << std::endl;
	}
	
	if (separate_msg_file && verbosity >= (size_t)logger::LOG_TYPE::SIMPLE_MSG) {
		logger_state.msg_file = std::make_unique<std::ofstream>(logger_state.msg_filename, (append_mode ? std::ofstream::app | std::ofstream::out : std::ofstream::out));
		if (!logger_state.msg_file->is_open()) {
			std::cerr << "LOG ERROR: couldn't open msg log file (" << logger_state.msg_filename << ")!" << std::endl;
		}
	}
	
	logger_state.verbosity = (logger::LOG_TYPE)verbosity;
	logger_state.append_mode = append_mode;
	logger_state.use_time = use_time;
	logger_state.use_color = use_color;
	logger_state.synchronous = synchronous_logging;
#if defined(__WINDOWS__)
	if (logger_state.use_color) {
		// disable color in Windows cmd/powershell
		const auto session_name_cstr = SDL_getenv("SESSIONNAME");
		const auto term_cstr = SDL_getenv("TERM");
		const std::string session_name = (session_name_cstr ? session_name_cstr : "");
		const std::string term = (term_cstr ? term_cstr : "");
		logger_state.use_color = !(session_name == "Console" && term.empty());
	}
#endif
#if defined(__APPLE__)
	logger_state.use_unicode_color = darwin_helper::is_running_in_debugger();
#endif
	
	// create+start the logger thread
	log_thread = std::make_unique<logger_thread>();
}

void logger::destroy() {
	if (!logger_state.initialized) {
		return;
	}
	
	// only allow single destroy
	if (logger_state.destroying.exchange(true)) {
		return;
	}
	
	log_msg("killing logger ...");
	log_thread = nullptr;
	
	logger_state.initialized = false;
	logger_state.destroying = false;
}

void logger::flush() {
	if (!log_thread) {
		return;
	}
	const uint32_t cur_run = log_thread->run_num;
	while (cur_run == log_thread->run_num) {
		std::this_thread::yield();
	}
}

bool logger::prepare_log(std::stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func) {
	// check verbosity level and leave or continue accordingly
	if (logger_state.verbosity < type) {
		return false;
	}
	
	if (type == logger::LOG_TYPE::UNDECORATED) {
		return true;
	}
	
	if (logger_state.use_color && logger_state.use_unicode_color) {
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
				buffer << (logger_state.use_color ? "\033[31m" : "") << "[ERR]";
				break;
			case LOG_TYPE::WARNING_MSG:
				buffer << (logger_state.use_color ? "\033[33m" : "") << "[WRN]";
				break;
			case LOG_TYPE::DEBUG_MSG:
				buffer << (logger_state.use_color ? "\033[32m" : "") << "[DBG]";
				break;
			case LOG_TYPE::SIMPLE_MSG:
				buffer << (logger_state.use_color ? "\033[34m" : "") << "[MSG]";
				break;
			case LOG_TYPE::UNDECORATED: break;
		}
		if (logger_state.use_color && type != LOG_TYPE::UNDECORATED) {
			buffer << "\033[m";
		}
	}
	
	if (logger_state.use_time) {
		buffer << "[";
		char time_str[64] { '\0' };
		const auto system_now = std::chrono::system_clock::now();
		const auto cur_time = std::chrono::system_clock::to_time_t(system_now);
		struct tm local_time {};
#if !defined(__WINDOWS__)
		if (localtime_r(&cur_time, &local_time)) {
			strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
		}
#else
		if (localtime_s(&local_time, &cur_time)) {
			strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
		}
#endif
		buffer << time_str;
		buffer << ".";
		buffer << std::setw(const_math::int_width(std::chrono::system_clock::period::den) - 1);
		buffer << system_now.time_since_epoch().count() % std::chrono::system_clock::period::den << std::setw(0);
		buffer << "]";
	}
	
#if FLOOR_LOG_THREAD_ID
	buffer << "[" << std::this_thread::get_id() << "]";
#endif
	
	buffer << " ";
	
	if (type == LOG_TYPE::ERROR_MSG) {
		buffer << "#" << logger_state.err_counter++ << ": ";
	}
	
	if (type != logger::LOG_TYPE::SIMPLE_MSG) {
		buffer << (file ? file : "") << ": " << (func ? func : "") << "(): ";
	}
	
	return true;
}

void logger::log_internal(std::stringstream& buffer, const LOG_TYPE& type, const char* str) REQUIRES(!logger_state.store_lock) {
	// this is the final log function
	if (str) {
		while (*str) {
			buffer << *str++;
		}
	}
	buffer << std::endl;
	
	// add string to log store/queue
	uint32_t cur_run = 0;
	{
		GUARD(logger_state.store_lock);
		logger_state.store.emplace_back(type, buffer.str());
		if (logger_state.synchronous && log_thread) {
			// similar to flush(): query current run number here ...
			cur_run = log_thread->run_num;
		}
	}
	// trigger logger thread
	logger_state.run_cv.notify_all();
	if (logger_state.synchronous && log_thread) {
		// ... then wait until it has changed
		while (cur_run == log_thread->run_num) {
			std::this_thread::yield();
		}
	}
}

void logger::set_verbosity(const LOG_TYPE& verbosity) {
	logger_state.verbosity = verbosity;
}

logger::LOG_TYPE logger::get_verbosity() {
	return logger_state.verbosity;
}

bool logger::is_initialized() {
	return logger_state.initialized;
}

} // namespace fl
