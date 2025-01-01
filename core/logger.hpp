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

#include <floor/core/essentials.hpp>
#include <string>
#include <sstream>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <cstdint>
using namespace std;

//! floor logging functions, use appropriately
//! note that you don't actually have to use a specific character for %_ to print the
//! correct type (the ostream operator<< is used and the %_ character is ignored - except
//! for $x (lowercase), $X (uppercase) and $Y (uppercase + fill to width) which will print
//! out an integer in hex format)
#define log_error(str, ...) logger::log<logger::compute_arg_count(str)>(logger::LOG_TYPE::ERROR_MSG, __FILE_NAME__, __func__, str, ##__VA_ARGS__)
#define log_warn(str, ...) logger::log<logger::compute_arg_count(str)>(logger::LOG_TYPE::WARNING_MSG, __FILE_NAME__, __func__, str, ##__VA_ARGS__)
#define log_debug(str, ...) logger::log<logger::compute_arg_count(str)>(logger::LOG_TYPE::DEBUG_MSG, __FILE_NAME__, __func__, str, ##__VA_ARGS__)
#define log_msg(str, ...) logger::log<logger::compute_arg_count(str)>(logger::LOG_TYPE::SIMPLE_MSG, "", "", str, ##__VA_ARGS__)
#define log_undecorated(str, ...) logger::log<logger::compute_arg_count(str)>(logger::LOG_TYPE::UNDECORATED, "", "", str, ##__VA_ARGS__)

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
	
	//! computes the required argument count for the specified log string
	static constexpr size_t compute_arg_count(const char* str) {
		size_t arg_count = 0;
		for (size_t i = 0, len = __builtin_strlen(str); i < len; ++i) {
			const auto& ch = str[i];
			// expecting "$" for arguments, "$$" for '$', single '$' at the end of string is an argument
			if (ch == '$') {
				if (i + 1 == len || (i + 1 < len && str[i + 1] != '$')) {
					++arg_count;
					++i;
				} else {
					++i; // skip '$'
				}
			}
		}
		return arg_count;
	}
	
	//! initializes the logger (opens the log files and creates the logger thread)
	static void init(const size_t verbosity,
					 const bool separate_msg_file,
					 const bool append_mode,
					 const bool use_time,
					 const bool use_color,
					 const bool synchronous_logging,
					 const string& log_filename = "",
					 const string& msg_filename = "");
	//! destroys the logger (also makes sure everything has been written to the console and log file)
	static void destroy();
	
	//! flushes the currently stored log messages (blocks until logger thread has run once)
	static void flush();
	
	//! log entry function, this will create a buffer and insert the log msgs start info (type, file name, ...) and
	//! finally call the internal log function (that does the actual logging)
	template <size_t computed_arg_count, typename... Args>
	static void log(const LOG_TYPE type, const char* file, const char* func, const char* str,
					Args&&... args) __attribute__((enable_if(computed_arg_count == sizeof...(Args), "valid"))) {
		stringstream buffer;
		if (!prepare_log(buffer, type, file, func)) {
			return;
		}
		log_internal(buffer, type, str, std::forward<Args>(args)...);
	}
	
	//! fail function when the argument count is incorrect
	template <size_t computed_arg_count, typename... Args>
	static void log(const LOG_TYPE, const char*, const char*, const char*,
					Args&&...) __attribute__((enable_if(computed_arg_count != sizeof...(Args), "invalid"),
											  unavailable("invalid argument count")));
	
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
	
	template <bool is_enum_flag, typename U> struct enum_helper_type {
		using type = U;
	};
	template <typename U> struct enum_helper_type<true, U> {
		using type = underlying_type_t<U>;
	};
	
	//! returns true if the specified character is a supported $ format character
	static constexpr bool is_format_char(const char& ch) {
		return (ch == 'x' || ch == 'X' || ch == 'Y' || ch == '\'');
	}
	
	//! handles the log format
	//! only $x, $X and $Y are supported at the moment, in all other cases the standard ostream operator<< is used!
	template <typename T>
	static void handle_format(stringstream& buffer, const char& format, T&& value) {
		using decayed_type = decay_t<T>;
		using print_type = conditional_t<is_enum_v<decayed_type>,
										 typename enum_helper_type<is_enum_v<decayed_type>, decayed_type>::type,
										 conditional_t<(is_pointer_v<decayed_type> &&
														!is_same_v<decayed_type, char*> &&
														!is_same_v<decayed_type, const char*> &&
														!is_same_v<decayed_type, unsigned char*> &&
														!is_same_v<decayed_type, const unsigned char*>),
														size_t,
														decayed_type>>;
		
		switch (format) {
			case 'x': // lowercase hex value
				buffer << hex << "0x" << (print_type)value << dec;
				break;
			case 'X': // uppercase hex value
				buffer << hex << uppercase << "0x" << (print_type)value << nouppercase << dec;
				break;
			case 'Y': // zero-filled uppercase hex value
				buffer << hex << uppercase << "0x";
				buffer << setw(sizeof(print_type) * 2) << setfill('0');
				buffer << (print_type)value;
				buffer << setfill(' ') << setw(0);
				buffer << nouppercase << dec;
				break;
			case '\'': // thousands separator
				if constexpr (is_integral_v<print_type> && sizeof(print_type) >= 2 && sizeof(print_type) <= 8) {
					// convert to unsigned / uint64_t + deal with sign
					uint64_t print_value {};
					if constexpr (std::is_signed_v<print_type>) {
						if (value < 0) {
							print_value = uint64_t(print_type(-value));
							buffer << '-';
						} else {
							print_value = uint64_t(print_type(value));
						}
					} else {
						print_value = uint64_t(print_type(value));
					}
					
					// count needed ' separators
					uint64_t sep_count = 0, sep_div = 1;
					for (auto tmp = print_value; tmp > 1000ull; tmp /= 1000ull) {
						++sep_count;
						sep_div *= 1000ull;
					}
					
					// print groups
					buffer << ((print_value / sep_div) % 1000ull);
					sep_div /= 1000ull;
					for (uint64_t sep = 0; sep < sep_count; ++sep, sep_div /= 1000ull) {
						buffer << '\'' << setw(3) << setfill('0');
						buffer << ((print_value / sep_div) % 1000ull);
						buffer << setfill(' ') << setw(0);
					}
					break;
				}
				// not an integral value or 8-bit -> print as is
				[[fallthrough]];
			default:
				buffer << (print_type)value;
				break;
		}
	}
	
	//! internal logging function (will be called in the end when there are no more args)
	static void log_internal(stringstream& buffer, const LOG_TYPE& type, const char* str);
	//! internal logging function (entry point and arg iteration)
	template <typename T, typename... Args>
	static void log_internal(stringstream& buffer, const LOG_TYPE& type,
							 const char* str, T&& value, Args&&... args) {
		for (size_t i = 0, len = __builtin_strlen(str); i < len; ++i) {
			const auto ch = str[i];
			if (ch == '$') {
				if (i + 1 == len) {
					// end of string with no format
					handle_format(buffer, '\0', value);
					log_internal(buffer, type, nullptr);
					return;
				} else if (i + 1 < len && str[i + 1] != '$') {
					handle_format(buffer, str[i + 1], value);
					const auto next_char_offset = size_t(is_format_char(str[i + 1]) ? 2 : 1);
					if (i + next_char_offset < len) {
						log_internal(buffer, type, &str[i + next_char_offset], std::forward<Args>(args)...);
					} else {
						log_internal(buffer, type, nullptr);
					}
					return;
				}
			}
			buffer << ch;
		}
	}
	
};
