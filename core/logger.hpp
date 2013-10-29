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

#ifndef __FLOOR_LOGGER_HPP__
#define __FLOOR_LOGGER_HPP__

#include <string>
#include <sstream>
#include <type_traits>
#include <iostream>
using namespace std;

//! floor logging functions, use appropriately
//! note that you don't actually have to use a specific character for %_ to print the
//! correct type (the ostream operator<< is used and the %_ character is ignored - except
//! for %x and %X which will print out an integer in hex format)
#define log_error(...) logger::log(logger::LOG_TYPE::ERROR_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_debug(...) logger::log(logger::LOG_TYPE::DEBUG_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_msg(...) logger::log(logger::LOG_TYPE::SIMPLE_MSG, __FILE__, __func__, __VA_ARGS__)
#define log_undecorated(...) logger::log(logger::LOG_TYPE::UNDECORATED, "", "", __VA_ARGS__)

class logger {
public:
	//! the kind of log message (this determines the output stream and potentially output formatting)
	enum class LOG_TYPE : size_t {
		ERROR_MSG	= 1, //!< error message
		DEBUG_MSG	= 2, //!< debug message
		SIMPLE_MSG	= 3, //!< simple message
		UNDECORATED	= 4, //!< message with no prefix (undecorated)
	};
	
	//! initializes the logger (opens the log files and creates the logger thread)
	static void init(const size_t verbosity, const bool separate_msg_file, const bool append_mode,
					 const string log_filename, const string msg_filename);
	//! destroys the logger (also makes sure everything has been written to the console and log file)
	static void destroy();
	
	// log entry function, this will create a buffer and insert the log msgs start info (type, file name, ...) and
	// finally call the internal log function (that does the actual logging)
	template<typename... Args> static void log(const LOG_TYPE type, const char* file, const char* func, const char* str, Args&&... args) {
		stringstream buffer;
		if(!prepare_log(buffer, type, file, func)) return;
		log_internal(buffer, type, str, std::forward<Args>(args)...);
	}
	
protected:
	// static class
	logger(const logger& l) = delete;
	~logger() = delete;
	logger& operator=(const logger& l) = delete;
	
	//! handles the formatting of log messages
	static bool prepare_log(stringstream& buffer, const LOG_TYPE& type, const char* file, const char* func);
	
	//
	template <bool is_enum_flag, typename U> struct enum_helper_type {
		typedef U type;
	};
	template <typename U> struct enum_helper_type<true, U> {
		typedef typename underlying_type<U>::type type;
	};
	//! handles the log format
	//! only %x and %X are supported at the moment, in all other cases the standard ostream operator<< is used!
	template <typename T> static void handle_format(stringstream& buffer, const char& format, T&& value) {
		typedef typename decay<T>::type decayed_type;
		typedef typename conditional<is_enum<decayed_type>::value,
									 typename enum_helper_type<is_enum<decayed_type>::value, decayed_type>::type,
									 typename conditional<is_pointer<decayed_type>::value &&
														  !is_same<decayed_type, char*>::value &&
														  !is_same<decayed_type, const char*>::value &&
														  !is_same<decayed_type, unsigned char*>::value &&
														  !is_same<decayed_type, const unsigned char*>::value,
														  size_t,
														  decayed_type>::type>::type print_type;
		
		switch(format) {
			case 'x':
				buffer << hex << "0x" << (print_type)value << dec;
				break;
			case 'X':
				buffer << hex << uppercase << "0x" << (print_type)value << nouppercase << dec;
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
		cout << "LOG ERROR: unused extra arguments specified in: \"" << buffer.str() << "\"!" << endl;
	}
	
};

#endif
