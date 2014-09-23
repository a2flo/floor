/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_NET_PROTOCOL_HPP__
#define __FLOOR_NET_PROTOCOL_HPP__

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
using boost::asio::ip::tcp;

#include <floor/core/platform.hpp>

template <class socket_type, bool use_ssl>
struct std_protocol {
	// these declarations aren't actually needed, but for the sake of understanding the "interface"
	// or getting an overview of it, i put these here
	
public:
	bool is_valid();
	bool ready() const;
	void invalidate();
	
	bool connect(const string& address, const unsigned short int& port);
	
	size_t receive(void* recv_data, const size_t max_len);
	bool send(const char* data, const size_t len);
	
};

#endif
