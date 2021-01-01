/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_ASIO_ERROR_HANDLER_HPP__
#define __FLOOR_ASIO_ERROR_HANDLER_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_NET)

// even w/o exception support, std::exception has to be provided
#include <exception>
#include <string>
#include <utility>
using namespace std;

//! this error handler is used instead of normal c++ exception handling to handle any exceptions/errors that
//! occur when using asio functions. this makes it possible to use asio/net functionality on targets that
//! don't support c++ exceptions.
//! NOTE: each thread has its own error stack (via tls)
//! NOTE: when using asio functionality directly, errors must be handled, otherwise the error stack will overflow
namespace asio_error_handler {
	//! asio exception/error object
	struct error {
		//! flag if this is actually an error or not
		bool is_error { false };
		//! original error/exception message (trimmed at 255 chars + \0)
		char error_msg[256] {};
		
		//! original error/exception message
		const char* what() const {
			return error_msg;
		}
		
		//! flag if this is actually an error or not
		bool operator()() const {
			return is_error;
		}
	};
	
	//! this is called through asios throw_exception function and shouldn't be called directly
	void handle_exception(const exception& exc);
	
	//! returns and clears the error on top of the error stack (the latest error)
	error get_error();
	
	//! returns the error on top of the error stack (the latest error),
	// check "is_error" flag (or operator()) if it's actually an error or not
	error peek_error();
	
	//! returns true if there is at least one unhandled error
	bool is_error();
	
	//! handles all current errors on the stack and returns a string of all error messages (one per line)
	string handle_all();
	
}

//! need to implement this (+forward) when not using exceptions
namespace asio {
	namespace detail {
		template <typename exception_class> void throw_exception(const exception_class& exc) {
			asio_error_handler::handle_exception(exc);
		}
	}
}

#endif

#endif
