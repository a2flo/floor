/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/net/asio_error_handler.hpp>

#if !defined(FLOOR_NO_NET)

#include <floor/core/logger.hpp>

struct asio_error_stack {
	// keep the last 16 errors in a ring buffer
	static constexpr const size_t size { 16 };
	size_t idx { 0 };
	size_t unhandled { 0 };
	asio_error_handler::error errors[size];
	
	void add(const char* error_msg) {
		errors[idx].is_error = true;
		strncpy(errors[idx].error_msg, error_msg, sizeof(asio_error_handler::error::error_msg));
		errors[idx].error_msg[sizeof(asio_error_handler::error::error_msg) - 1] = '\0';
		idx = (idx + 1u) % size;
		if(unhandled < size) ++unhandled;
		else {
			log_error("error overflow (full error ring buffer - %u errors have not been handled)", size_t(size));
		}
	}
	
	asio_error_handler::error pop() {
		idx = (idx == 0 ? size - 1 : idx - 1);
		if(unhandled > 0) --unhandled;
		else {
			log_error("error underflow (no errors are currently unhandled)");
		}
		return errors[idx];
	}
	
	asio_error_handler::error peek() {
		if(unhandled == 0) return {};
		const auto prev_idx = (idx == 0 ? size - 1 : idx - 1);
		return errors[prev_idx];
	}
	
	bool empty() const {
		return (unhandled == 0);
	}
};

static _Thread_local asio_error_stack asio_errors;

void asio_error_handler::handle_exception(const exception& exc) {
	log_error("asio/net error: %s", exc.what());
	asio_errors.add(exc.what());
}

asio_error_handler::error asio_error_handler::get_error() {
	if(asio_errors.empty()) return {};
	return asio_errors.pop();
}

asio_error_handler::error asio_error_handler::peek_error() {
	return asio_errors.peek();
}

bool asio_error_handler::is_error() {
	return !asio_errors.empty();
}

string asio_error_handler::handle_all() {
	string ret { "" };
	while(is_error()) {
		ret += get_error().what();
		ret += '\n';
	}
	return ret;
}

#endif
