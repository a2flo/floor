/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_OPTION_HANDLER_HPP__
#define __FLOOR_OPTION_HANDLER_HPP__

#include <string>
#include <functional>
#include <utility>
#include <floor/core/logger.hpp>
using namespace std;

template <class option_context> class option_handler {
public:
	//! parses the command line options and sets everything up inside the ctx
	static void parse_options(char* argv[], option_context& option_ctx) {
		static const auto& const_options = options;
		
		// parse compiler options
		auto arg_ptr = argv;
		for(; const auto arg = *arg_ptr; ++arg_ptr) {
			// all options must start with "-"
			if(arg[0] != '-') break;
			// special arg: "-": stdin/not an option
			else if(arg[1] == '\0') { // "-"
				break;
			}
			// special arg: "--": explicit end of options
			else if(arg[1] == '-' && arg[2] == '\0') {
				++arg_ptr;
				break;
			}
			
			// for all other options: call the registered function and provide the current context
			try {
				// if there is no function registered for an option, this will print an error and abort further parsing
				const auto opt_iter = find_if(cbegin(const_options), cend(const_options), [&arg](const auto& elem) {
					return (elem.first == arg);
				});
				if(opt_iter == const_options.end()) {
					log_error("unknown argument \"$\"", arg);
					return;
				}
				opt_iter->second(option_ctx, arg_ptr);
			} catch(...) {
				log_error("caught unknown exception");
			}
		}
		
		// handle additional options after "--"
		for(; const auto arg = *arg_ptr; ++arg_ptr) {
			option_ctx.additional_options += " ";
			option_ctx.additional_options += arg;
		}
	}
	
	// use these to register any external options
	//! function callback typedef for an option (hint: use lambda expressions).
	//! NOTE: you have complete r/w access to the option context to set everything up
	typedef function<void(option_context&, char**& arg_ptr)> option_function;
	
	//! adds a single options
	static void add_option(string option, option_function func) {
		options.emplace_back(make_pair(option, func));
	}
	
	//! adds multiple options at once (NOTE: use braces { ... })
	static void add_options(initializer_list<pair<const string, option_function>> options_) {
		for(const auto& opt : options_) {
			options.emplace_back(opt);
		}
	}
	
protected:
	// static class
	option_handler() = delete;
	~option_handler() = delete;
	option_handler& operator=(const option_handler&) = delete;
	
	//! NOTE: this _must_ be instantiated/set by the user of this class
	static vector<pair<string, option_handler<option_context>::option_function>> options;
	
};

#endif
