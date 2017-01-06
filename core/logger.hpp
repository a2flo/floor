/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_LOGGER_HPP__
#define __FLOOR_LOGGER_HPP__

#include <string>
#include <sstream>
#include <type_traits>
#include <iostream>
#include <iomanip>
using namespace std;

//! floor logging functions, use appropriately
//! note that you don't actually have to use a specific character for %_ to print the
//! correct type (the ostream operator<< is used and the %_ character is ignored - except
//! for %x (lowercase), %X (uppercase) and %Y (uppercase + fill to width) which will print
//! out an integer in hex format)
#define log_error(...) logger::log(logger::LOG_TYPE::ERROR_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_warn(...) logger::log(logger::LOG_TYPE::WARNING_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_debug(...) logger::log(logger::LOG_TYPE::DEBUG_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_msg(...) logger::log(logger::LOG_TYPE::SIMPLE_MSG, "", "", __VA_ARGS__)
#define log_undecorated(...) logger::log(logger::LOG_TYPE::UNDECORATED, "", "", __VA_ARGS__)

class logger {
public:
	//! the kind of log message (this determines the output stream and potentially output formatting)
	enum class LOG_TYPE : uint32_t {
		ERROR_MSG	= 1u, //!< error message
		WARNING_MSG	= 2u, //!< warning message
		DEBUG_MSG	= 3u, //!< debug message
		SIMPLE_MSG	= 4u, //!< simple message
		UNDECORATED	= 5u, //!< message with no prefix (undecorated)
	};
	
	//! initializes the logger (opens the log files and creates the logger thread)
	static void init(const size_t verbosity,
					 const bool separate_msg_file,
					 const bool append_mode,
					 const bool use_time,
					 const bool use_color,
					 const string log_filename = "",
					 const string msg_filename = "");
	//! destroys the logger (also makes sure everything has been written to the console and log file)
	static void destroy();
	
	//! flushes the currently stored log messages (blocks until logger thread has run once)
	static void flush();
	
	// log entry function, this will create a buffer and insert the log msgs start info (type, file name, ...) and
	// finally call the internal log function (that does the actual logging)
	template<typename... Args> static void log(const LOG_TYPE type, const char* file, const char* func, const char* str, Args&&... args) {
		stringstream buffer;
		if(!prepare_log(buffer, type, file, func)) return;
		log_internal(buffer, type, str, std::forward<Args>(args)...);
	}
	
	//! sets the logger verbosity to the specific LOG_TYPE verbosity level
	static void set_verbosity(const LOG_TYPE& verbosity);
	//! returns the current logger verbosity level
	static LOG_TYPE get_verbosity();
	
	//! returns true if the logger was initialized
	static bool is_initialized();
	
protected:
	// static class
	logger(const logger&) = delete;
	~logger() = delete;
	logger& operator=(const logger&) = delete;
	
	//! handles the formatting of log messages
	static bool prepare_log(stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func);
	
	//
	template <bool is_enum_flag, typename U> struct enum_helper_type {
		typedef U type;
	};
	template <typename U> struct enum_helper_type<true, U> {
		typedef underlying_type_t<U> type;
	};
	//! handles the log format
	//! only %x, %X and %Y are supported at the moment, in all other cases the standard ostream operator<< is used!
	template <typename T> static void handle_format(stringstream& buffer, const char& format, T&& value) {
		typedef decay_t<T> decayed_type;
		typedef conditional_t<is_enum<decayed_type>::value,
							  typename enum_helper_type<is_enum<decayed_type>::value, decayed_type>::type,
							  conditional_t<(is_pointer<decayed_type>::value &&
											 !is_same<decayed_type, char*>::value &&
											 !is_same<decayed_type, const char*>::value &&
											 !is_same<decayed_type, unsigned char*>::value &&
											 !is_same<decayed_type, const unsigned char*>::value),
											 size_t,
											 decayed_type>> print_type;
		
		switch(format) {
			case 'x':
				buffer << hex << "0x" << (print_type)value << dec;
				break;
			case 'X':
				buffer << hex << uppercase << "0x" << (print_type)value << nouppercase << dec;
				break;
			case 'Y':
				buffer << hex << uppercase << "0x";
				buffer << setw(sizeof(T) * 2) << setfill('0');
				buffer << (print_type)value;
				buffer << setfill(' ')<< setw(0);
				buffer << nouppercase << dec;
				break;
			default:
				buffer << (print_type)value;
				break;
		}
	}
	
	//! internal logging function (will be called in the end when there are no more args)
	static void log_internal(stringstream& buffer, const LOG_TYPE& type, const char* str);
	//! internal logging function (entry point and arg iteration)
	template<typename T, typename... Args> static void log_internal(stringstream& buffer, const LOG_TYPE& type,
																	const char* str, T&& value, Args&&... args) {
		while(*str) {
			if(*str == '%' && *(++str) != '%') {
				handle_format(buffer, *str, value);
				log_internal(buffer, type, ++str, std::forward<Args>(args)...);
				return;
			}
			buffer << *str++;
		}
		cerr << "LOG ERROR: unused extra arguments specified in: \"" << buffer.str() << "\"!" << endl;
	}
	
};

#endif
