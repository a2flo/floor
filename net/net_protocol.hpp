/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef __FLOOR_NET_PROTOCOL_HPP__
#define __FLOOR_NET_PROTOCOL_HPP__

#include "core/platform.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
using boost::asio::ip::tcp;

template <class socket_type, bool use_ssl>
struct std_protocol {
	// these declarations aren't actually needed, but for the sake of understanding the "interface"
	// or getting an overview of it, i put these here
	
public:
	bool is_valid();
	bool ready();
	
	bool open_socket(const string& address, const string& port);
	
	int receive(void* data, const unsigned int max_len);
	bool send(const char* data, const int len);
	
};

#endif
