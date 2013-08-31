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

#ifndef __FLOOR_NET_TCP_HPP__
#define __FLOOR_NET_TCP_HPP__

#include "platform.hpp"
#include "net_protocol.hpp"

template<> struct std_protocol<tcp::socket> {
	std_protocol<tcp::socket>() : io_service(), resolver(io_service), socket(io_service) {
	}
	~std_protocol<tcp::socket>() {
		socket.close();
	}
	
	bool is_valid() const {
		return (valid && ((socket_set && socket.is_open()) ||
						  !socket_set));
	}
	
	bool open_socket(const string& address, const unsigned short int& port) {
		if(!valid) return false;
		socket_set = true;
		
		boost::system::error_code ec;
		auto endpoint_iterator = resolver.resolve({ address, to_str(port) });
		boost::asio::connect(socket, endpoint_iterator, ec);
		
		if(ec) {
			log_error("socket connection error: %s", ec.message());
			valid = false;
			return false;
		}
		if(!socket.is_open()) {
			log_error("couldn't open socket!");
			valid = false;
			return false;
		}
		
		// set keep-alive flag (this only handles the simple cases and
		// usually has a big timeout value, but still better than nothing)
		boost::asio::socket_base::keep_alive option(true);
		socket.set_option(option);
		
		return true;
	}
	
	int receive(void* data, const unsigned int max_len) {
		boost::system::error_code ec;
		auto data_received = socket.receive(boost::asio::buffer(data, max_len), 0, ec);
		if(ec) {
			log_error("error while receiving data: %s", ec.message());
			valid = false;
			return 0;
		}
		return (int)data_received;
	}
	
	bool ready() const {
		if(!socket_set || !valid) return false;
		return (socket.available() > 0);
	}
	
	bool send(const char* data, const int len) {
		boost::system::error_code ec;
		const auto data_sent = boost::asio::write(socket, boost::asio::buffer(data, len), ec);
		if(ec) {
			log_error("error while sending data: %s", ec.message());
			valid = false;
			return false;
		}
		if(data_sent != (size_t)len) {
			log_error("error while sending data: sent data length (%u) != requested data length (%u)!",
						 data_sent, len);
			valid = false;
			return false;
		}
		return true;
	}
	
	boost::asio::ip::address get_local_address() const {
		return socket.local_endpoint().address();
	}
	unsigned short int get_local_port() const {
		return socket.local_endpoint().port();
	}
	
	boost::asio::ip::address get_remote_address() const {
		return socket.remote_endpoint().address();
	}
	unsigned short int get_remote_port() const {
		return socket.remote_endpoint().port();
	}
	
	void invalidate() {
		valid = false;
	}
	
protected:
	atomic<bool> socket_set { false };
	atomic<bool> valid { true };
	
	boost::asio::io_service io_service;
	tcp::resolver resolver;
	tcp::socket socket;
	
};

typedef std_protocol<tcp::socket> TCP_protocol;

#endif
